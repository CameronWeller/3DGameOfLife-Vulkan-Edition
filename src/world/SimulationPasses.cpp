// SimulationPasses: pipelines, descriptor sets and the command recording for
// one batch (steps, then a build, then readbacks). The pass structure is
// described at the top of SimulationPasses.h.
//
// Synchronization: the frames and the batches are submitted to the same queue,
// and a pipeline barrier orders everything submitted before it on that queue,
// so barriers are all a batch needs; no semaphores.

#include "world/SimulationPasses.h"

#include <algorithm>
#include <chrono>
#include <stdexcept>
#include <string>
#include <vector>

#include "gpu/Barriers.h"
#include "gpu/Descriptors.h"
#include "gpu/GpuContext.h"
#include "gpu/ImmediateCommands.h"
#include "gpu/ShaderModule.h"
#include "util/Units.h"
#include "world/BlockInstances.h"
#include "world/ChunkLayout.h"
#include "world/ChunkWorld.h"

namespace gol3d {
namespace {

// Push constants of life3d_step.comp. Must match `Params` there, field for
// field: dispatchOverChunks() fills in the first two.
struct StepConstants {
    uint32_t firstIndex; // first entry of the active list in this dispatch
    uint32_t count;      // entries in this dispatch
    uint32_t surviveMask;
    uint32_t birthMask;
    uint32_t frontBackSides; // countNeighbors() in life3d_bits.glsl
    uint32_t frontBackMiddle;
};
static_assert(sizeof(StepConstants) == 24, "six uints, as in life3d_step.comp");

// Push constants of life3d_build.comp. Must match `Params` there, field for
// field (std430 packs these 4-byte scalars after the mat4 with no padding).
struct BuildConstants {
    glm::mat4 cullViewProjection;
    uint32_t firstIndex;
    uint32_t count;
    uint32_t margin; // cells from a face that count as reaching the neighbor chunk
    uint32_t flags;  // BUILD_* bits below
    uint32_t maxInstances;
    float cullDistance;
    float cameraX;
    float cameraY;
    float cameraZ;
};
static_assert(sizeof(BuildConstants) == 100, "a mat4 and nine scalars, as in life3d_build.comp");

// BuildConstants::flags; the same values are in life3d_build.comp.
constexpr uint32_t BUILD_ANIMATE = 1; // flag births, draw the last deaths
constexpr uint32_t BUILD_DRAW = 2;    // write the block list
constexpr uint32_t BUILD_STATS = 4;   // count population and reach
constexpr uint32_t BUILD_LAYERS = 8;  // the rule's neighbors are in the cell's x-y layer

// Both passes use one descriptor set layout: these bindings, all storage
// buffers. The numbers must match the `binding = N` declarations in
// life3d_storage.glsl, life3d_step.comp and life3d_build.comp.
enum Binding : uint32_t {
    CurrentCells = 0, // the generation being read
    OtherCells = 1,   // the next generation (step) or the previous one (build)
    Neighbors = 2,
    ActiveList = 3,
    BlockPool = 4,
    BlockSlots = 5,
    Stats = 6,
    Instances = 7,
    IndirectDraw = 8,
    Origins = 9,
    BindingCount,
};

// The compute grid: x is the slab within a chunk, y the chunk within this
// dispatch's range of the active list. Must match SLAB_DEPTH in
// life3d_storage.glsl (each workgroup is CHUNK_SIZE x SLAB_DEPTH rows).
constexpr uint32_t SLAB_DEPTH = 8;
constexpr uint32_t SLABS_PER_CHUNK = CHUNK_SIZE / SLAB_DEPTH;

// Vulkan only guarantees 65535 workgroups along y (maxComputeWorkGroupCount[1]),
// so larger worlds are split into several dispatches.
constexpr uint32_t MAX_DISPATCH_CHUNKS = 65535;

// Two descriptor sets, one per cell buffer that can hold the current generation.
constexpr uint32_t DESCRIPTOR_SET_COUNT = 2;

// A pass never reads as free, so the governor's divisions stay finite.
constexpr double MIN_PASS_MS = 1e-4;
constexpr double NANOSECONDS_PER_MILLISECOND = 1e6;

// Timestamp query slots.
enum Timestamp : uint32_t {
    BeforeSteps,
    AfterSteps,
    AfterBuild,
    BatchTimestampCount,
    // Then the same three for each slot of the ring of unwaited submissions.
    TimestampCount = BatchTimestampCount * (1 + ASYNC_IN_FLIGHT),
};
uint32_t asyncQuery(uint32_t slot, Timestamp timestamp) {
    return BatchTimestampCount * (1 + slot) + timestamp;
}

// The step pass's push constants for `rule`; dispatchOverChunks() fills in the range.
StepConstants stepConstants(const LifeRule& rule) {
    const FrontBackMasks masks = frontBackMasks(rule);
    return {0, 0, rule.surviveMask, rule.birthMask, masks.sides, masks.middle};
}

} // namespace

// ------------------------------------------------------------------- setup

void SimulationPasses::init(const GpuContext& gpu, BufferAllocator& allocator,
                            ImmediateCommands& commands, const std::filesystem::path& shaderDir,
                            const ChunkWorld& world) {
    gpu_ = &gpu;
    allocator_ = &allocator;
    commands_ = &commands;
    for (ImmediateCommands& asyncCommands : asyncCommands_) {
        asyncCommands.init(gpu);
    }
    allocator.create(instanceBuffer_,
                     static_cast<VkDeviceSize>(MAX_BLOCK_INSTANCES) * sizeof(glm::uvec2),
                     usage::STORAGE, {memory::DEVICE, memory::HOST});
    allocator.create(indirectBuffer_, sizeof(VkDrawIndexedIndirectCommand),
                     usage::STORAGE_COPY | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT,
                     {memory::DEVICE, memory::HOST});
    allocator.create(readbackBuffer_, sizeof(VkDrawIndexedIndirectCommand),
                     VK_BUFFER_USAGE_TRANSFER_DST_BIT, {memory::HOST_CACHED, memory::HOST});
    createPipelines(shaderDir);
    createDescriptorSets(world);
    createTimestampQueries();
}

void SimulationPasses::destroy() {
    if (!gpu_) return;
    VkDevice device = gpu_->device;
    for (ImmediateCommands& asyncCommands : asyncCommands_) {
        asyncCommands.destroy();
    }
    if (timestamps_) vkDestroyQueryPool(device, timestamps_, nullptr);
    vkDestroyPipeline(device, stepPipeline_, nullptr);
    vkDestroyPipeline(device, buildPipeline_, nullptr);
    vkDestroyPipelineLayout(device, pipelineLayout_, nullptr);
    vkDestroyDescriptorPool(device, descriptorPool_, nullptr);
    vkDestroyDescriptorSetLayout(device, setLayout_, nullptr);
    for (GpuBuffer* buffer : {&instanceBuffer_, &indirectBuffer_, &readbackBuffer_}) {
        allocator_->destroy(*buffer);
    }
    gpu_ = nullptr;
}

void SimulationPasses::createPipelines(const std::filesystem::path& shaderDir) {
    std::array<VkDescriptorSetLayoutBinding, BindingCount> bindings{};
    for (uint32_t i = 0; i < bindings.size(); ++i) {
        bindings[i] = {i, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT,
                       nullptr};
    }
    VkDescriptorSetLayoutCreateInfo layoutInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    layoutInfo.pBindings = bindings.data();
    if (vkCreateDescriptorSetLayout(gpu_->device, &layoutInfo, nullptr, &setLayout_) !=
        VK_SUCCESS) {
        throw std::runtime_error("Could not create compute descriptor set layout.");
    }

    // One push-constant range big enough for either pass.
    VkPushConstantRange pushRange{VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(BuildConstants)};
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &setLayout_;
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &pushRange;
    if (vkCreatePipelineLayout(gpu_->device, &pipelineLayoutInfo, nullptr, &pipelineLayout_) !=
        VK_SUCCESS) {
        throw std::runtime_error("Could not create compute pipeline layout.");
    }
    stepPipeline_ = createPipeline(shaderDir / "life3d_step.comp.spv");
    buildPipeline_ = createPipeline(shaderDir / "life3d_build.comp.spv");
}

VkPipeline SimulationPasses::createPipeline(const std::filesystem::path& shader) {
    VkShaderModule module = loadShaderModule(gpu_->device, shader);
    VkComputePipelineCreateInfo pipelineInfo{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
    pipelineInfo.layout = pipelineLayout_;
    pipelineInfo.stage = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                          nullptr,
                          0,
                          VK_SHADER_STAGE_COMPUTE_BIT,
                          module,
                          "main",
                          nullptr};
    VkPipeline pipeline = VK_NULL_HANDLE;
    VkResult result = vkCreateComputePipelines(gpu_->device, VK_NULL_HANDLE, 1, &pipelineInfo,
                                               nullptr, &pipeline);
    vkDestroyShaderModule(gpu_->device, module, nullptr);
    if (result != VK_SUCCESS) {
        throw std::runtime_error("Could not create compute pipeline " + shader.filename().string());
    }
    return pipeline;
}

void SimulationPasses::createDescriptorSets(const ChunkWorld& world) {
    static_assert(DESCRIPTOR_SET_COUNT == std::tuple_size_v<decltype(descriptorSets_)>);
    VkDescriptorPoolSize poolSize{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                  DESCRIPTOR_SET_COUNT * BindingCount};
    VkDescriptorPoolCreateInfo poolInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = &poolSize;
    poolInfo.maxSets = DESCRIPTOR_SET_COUNT;
    if (vkCreateDescriptorPool(gpu_->device, &poolInfo, nullptr, &descriptorPool_) != VK_SUCCESS) {
        throw std::runtime_error("Could not create compute descriptor pool.");
    }
    for (VkDescriptorSet& set : descriptorSets_) {
        VkDescriptorSetAllocateInfo allocInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
        allocInfo.descriptorPool = descriptorPool_;
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts = &setLayout_;
        if (vkAllocateDescriptorSets(gpu_->device, &allocInfo, &set) != VK_SUCCESS) {
            throw std::runtime_error("Could not allocate compute descriptor set.");
        }
    }
    writeDescriptors(world);
}

// Set `current` binds cell buffer `current` as CurrentCells and the other one
// as OtherCells, so flipping generations is just binding the other set.
void SimulationPasses::writeDescriptors(const ChunkWorld& world) {
    const ChunkPool& pool = world.pool();
    for (uint32_t current = 0; current < DESCRIPTOR_SET_COUNT; ++current) {
        std::array<VkBuffer, BindingCount> buffers{};
        buffers[CurrentCells] = pool.cells[current].buffer;
        buffers[OtherCells] = pool.cells[1 - current].buffer;
        buffers[Neighbors] = pool.neighbors.buffer;
        buffers[ActiveList] = pool.activeList.buffer;
        buffers[BlockPool] = world.blockPool().buffer;
        buffers[BlockSlots] = pool.blockSlots.buffer;
        buffers[Stats] = pool.stats.buffer;
        buffers[Instances] = instanceBuffer_.buffer;
        buffers[IndirectDraw] = indirectBuffer_.buffer;
        buffers[Origins] = pool.origins.buffer;
        for (uint32_t binding = 0; binding < BindingCount; ++binding) {
            writeBufferDescriptor(gpu_->device, descriptorSets_[current], binding,
                                  VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, buffers[binding]);
        }
    }
}

void SimulationPasses::createTimestampQueries() {
    uint32_t familyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(gpu_->physicalDevice, &familyCount, nullptr);
    std::vector<VkQueueFamilyProperties> families(familyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(gpu_->physicalDevice, &familyCount, families.data());
    const uint32_t validBits = families[gpu_->queueFamily].timestampValidBits;
    const float nsPerTick = gpu_->properties.limits.timestampPeriod;
    if (validBits == 0 || nsPerTick <= 0.0f) return; // no timestamps: costs come from wall time

    VkQueryPoolCreateInfo queryInfo{VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO};
    queryInfo.queryType = VK_QUERY_TYPE_TIMESTAMP;
    queryInfo.queryCount = TimestampCount;
    if (vkCreateQueryPool(gpu_->device, &queryInfo, nullptr, &timestamps_) == VK_SUCCESS) {
        msPerTimestampTick_ = nsPerTick / NANOSECONDS_PER_MILLISECOND;
        // A counter with fewer than 64 valid bits wraps at 2^validBits.
        timestampMask_ = validBits >= 64 ? ~0ull : (1ull << validBits) - 1;
    }
}

// --------------------------------------------------------------- a batch

// Runs the bound pipeline over `count` entries of the active list from `first`
// on: SLABS_PER_CHUNK workgroups per chunk, at most MAX_DISPATCH_CHUNKS chunks
// per dispatch. `constants` gets each dispatch's range filled in.
template <typename PushConstants>
void SimulationPasses::dispatchOverChunks(VkCommandBuffer cmd, uint32_t first, uint32_t count,
                                          PushConstants constants) {
    const uint32_t end = first + count;
    for (uint32_t start = first; start < end; start += MAX_DISPATCH_CHUNKS) {
        constants.firstIndex = start;
        constants.count = std::min(MAX_DISPATCH_CHUNKS, end - start);
        vkCmdPushConstants(cmd, pipelineLayout_, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(constants),
                           &constants);
        vkCmdDispatch(cmd, SLABS_PER_CHUNK, constants.count, 1);
    }
}

// Orders a submission after everything before it on the queue, then takes the
// timestamp `startQuery` (after resetting it and the `queryCount - 1` that
// follow it).
void SimulationPasses::startSubmission(VkCommandBuffer cmd, uint32_t startQuery,
                                       uint32_t queryCount) {
    // Write-after-read: frames submitted earlier may still be drawing the block
    // list (indirect command and vertex shader reads). This batch's shaders,
    // clears and copies overwrite it, so they wait for those draws. The source
    // also covers the previous batch's compute writes: waiting on its fence told
    // the CPU it finished, but on the GPU a later submission to the same queue
    // still needs a memory barrier before its shaders may read those writes.
    memoryBarrier(cmd,
                  VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT |
                      VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                  VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT |
                      VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT,
                  VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_2_CLEAR_BIT |
                      VK_PIPELINE_STAGE_2_COPY_BIT,
                  VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT |
                      VK_ACCESS_2_TRANSFER_WRITE_BIT);
    if (!timestamps_) return;
    vkCmdResetQueryPool(cmd, timestamps_, startQuery, queryCount);
    // The start is taken once earlier work (such as a frame still drawing
    // ahead of a slice) is done, so it is not counted.
    writeTimestampAfterWork(cmd, startQuery);
}

// Takes a timestamp once the work recorded so far has finished. A timestamp is
// not ordered after the dispatches before it: Mesa's Intel driver writes it as
// soon as they start, which made the build pass look free and let the governor
// overrun its budget in large worlds. The barrier makes it wait.
void SimulationPasses::writeTimestampAfterWork(VkCommandBuffer cmd, uint32_t query) {
    if (!timestamps_) return;
    memoryBarrier(cmd, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, VK_ACCESS_2_NONE,
                  VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, VK_ACCESS_2_NONE);
    vkCmdWriteTimestamp2(cmd, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, timestamps_, query);
}

void SimulationPasses::submitAsync(const AsyncRequest& request, const ChunkWorld& world) {
    // The ring slot used ASYNC_IN_FLIGHT submissions ago: done by now, as
    // frames were drawn since.
    const uint32_t slot = nextAsync_;
    nextAsync_ = (nextAsync_ + 1) % ASYNC_IN_FLIGHT;
    finishAsync(slot);

    ImmediateCommands& commands = asyncCommands_[slot];
    VkCommandBuffer cmd = commands.begin();
    startSubmission(cmd, asyncQuery(slot, BeforeSteps), BatchTimestampCount);
    const uint32_t current = world.currentBuffer();
    if (request.sliceCount > 0) {
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, stepPipeline_);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout_, 0, 1,
                                &descriptorSets_[current], 0, nullptr);
        dispatchOverChunks(cmd, request.sliceFirst, request.sliceCount,
                           stepConstants(*request.rule));
    }
    writeTimestampAfterWork(cmd, asyncQuery(slot, AfterSteps));
    const BatchRequest& build = request.build;
    const bool built = build.writeBlockList || build.collectStats;
    if (built) {
        // The build reads what the slices wrote once they are all done.
        memoryBarrier(cmd, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_WRITE_BIT,
                      VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT);
        const uint32_t source = request.buildNextGeneration ? 1 - current : current;
        recordBuild(cmd, build, world, source);
    }
    writeTimestampAfterWork(cmd, asyncQuery(slot, AfterBuild));
    if (built) recordReadback(cmd, build, world);
    commands.submit("the simulation, alongside the frame");
    const double share = request.sliceCount > 0
                             ? static_cast<double>(request.sliceCount) / world.activeChunkCount()
                             : 0.0;
    asyncSlots_[slot] = {.pending = true,
                         .share = share,
                         .writeBlockList = build.writeBlockList,
                         .collectStats = build.collectStats};
}

void SimulationPasses::finishAsync() {
    for (uint32_t slot = 0; slot < ASYNC_IN_FLIGHT; ++slot) {
        finishAsync(slot);
    }
}

// Waits for the submission in ring slot `slot`, if any, and records its costs.
// Slices are too small to measure a step one by one (fixed costs would
// dominate), so their GPU times add up until they cover half the world or more.
void SimulationPasses::finishAsync(uint32_t slot) {
    AsyncSlot& done = asyncSlots_[slot];
    if (!done.pending) return;
    done.pending = false;
    const auto waitStart = std::chrono::steady_clock::now();
    asyncCommands_[slot].wait();
    totalGpuMs_ += millisecondsSince(waitStart);

    std::array<uint64_t, BatchTimestampCount> ticks{};
    const bool haveTimestamps =
        timestamps_ &&
        vkGetQueryPoolResults(gpu_->device, timestamps_, asyncQuery(slot, BeforeSteps),
                              BatchTimestampCount, sizeof(ticks), ticks.data(), sizeof(uint64_t),
                              VK_QUERY_RESULT_64_BIT) == VK_SUCCESS;
    // It ran alongside a frame, so without timestamps there is no time to go by.
    if (!haveTimestamps) return;
    auto elapsedMs = [&](Timestamp from, Timestamp to) {
        uint64_t elapsedTicks = (ticks[to] - ticks[from]) & timestampMask_;
        return std::max(MIN_PASS_MS, static_cast<double>(elapsedTicks) * msPerTimestampTick_);
    };
    if (done.writeBlockList || done.collectStats) {
        recordBuildCost(done.writeBlockList, done.collectStats, elapsedMs(AfterSteps, AfterBuild));
    }
    if (done.share <= 0.0) return;
    slicedMs_ += elapsedMs(BeforeSteps, AfterSteps);
    slicedShare_ += done.share;
    constexpr double SHARE_TO_MEASURE = 0.5;
    if (slicedShare_ >= SHARE_TO_MEASURE) {
        smoothCost(costs_.stepMs, std::max(MIN_PASS_MS, slicedMs_ / slicedShare_));
        slicedMs_ = 0.0;
        slicedShare_ = 0.0;
    }
}

void SimulationPasses::run(const BatchRequest& request, ChunkWorld& world) {
    finishAsync(); // work still running is not this batch's time
    const auto wallStart = std::chrono::steady_clock::now();
    VkCommandBuffer cmd = commands_->begin();
    startSubmission(cmd, BeforeSteps, BatchTimestampCount);
    const uint32_t newest = recordSteps(cmd, request, world);
    writeTimestampAfterWork(cmd, AfterSteps);
    recordBuild(cmd, request, world, newest);
    writeTimestampAfterWork(cmd, AfterBuild);
    recordReadback(cmd, request, world);

    const auto submitStart = std::chrono::steady_clock::now();
    commands_->submitAndWait(request.steps ? "the simulation pass" : "the block list");
    totalGpuMs_ += millisecondsSince(submitStart);

    world.setCurrentBuffer(newest);
    updateCosts(request, millisecondsSince(wallStart));
}

// Records the steps; each reads one cell buffer and writes the other. Returns
// the buffer that will hold the newest generation.
uint32_t SimulationPasses::recordSteps(VkCommandBuffer cmd, const BatchRequest& request,
                                       const ChunkWorld& world) {
    const uint32_t chunkCount = world.activeChunkCount();
    uint32_t current = world.currentBuffer();
    if (request.steps == 0) return current;
    // An empty world stays empty; only the buffer parity advances, one flip per step.
    if (chunkCount == 0) return (current + request.steps) & 1u;

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, stepPipeline_);
    const StepConstants constants = stepConstants(*request.rule);
    for (uint32_t step = 0; step < request.steps; ++step) {
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout_, 0, 1,
                                &descriptorSets_[current], 0, nullptr);
        dispatchOverChunks(cmd, 0, chunkCount, constants);
        // The next step reads the rows this one wrote (read-after-write) and
        // overwrites the rows this one read (write-after-read).
        memoryBarrier(cmd, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_WRITE_BIT,
                      VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                      VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT);
        current = 1 - current;
    }
    return current;
}

// Records the build pass over the newest generation. It accumulates into the
// stats and the draw's instance count with atomics, so those are reset first.
void SimulationPasses::recordBuild(VkCommandBuffer cmd, const BatchRequest& request,
                                   const ChunkWorld& world, uint32_t newest) {
    if (request.collectStats) vkCmdFillBuffer(cmd, world.pool().stats.buffer, 0, VK_WHOLE_SIZE, 0);
    if (request.writeBlockList) {
        // Every block is drawn with the same BLOCK_INDEX_COUNT indices; the
        // shader counts the instances up from zero.
        const VkDrawIndexedIndirectCommand emptyDraw{.indexCount = BLOCK_INDEX_COUNT,
                                                     .instanceCount = 0,
                                                     .firstIndex = 0,
                                                     .vertexOffset = 0,
                                                     .firstInstance = 0};
        vkCmdUpdateBuffer(cmd, indirectBuffer_.buffer, 0, sizeof(emptyDraw), &emptyDraw);
    }
    // The fill and update (transfer commands) finish before the shader's atomics.
    memoryBarrier(cmd, VK_PIPELINE_STAGE_2_CLEAR_BIT | VK_PIPELINE_STAGE_2_COPY_BIT,
                  VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                  VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT);

    const uint32_t chunkCount = world.activeChunkCount();
    const bool anyWork = request.writeBlockList || request.collectStats;
    if (chunkCount == 0 || !anyWork) return;
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, buildPipeline_);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout_, 0, 1,
                            &descriptorSets_[newest], 0, nullptr);
    BuildConstants constants{};
    constants.cullViewProjection = request.cullViewProjection;
    // Flag neighbors for life within MAX_BATCH cells, so the next batch of up to
    // MAX_BATCH generations cannot outrun the chunks maintain() creates.
    constants.margin = MAX_BATCH;
    const bool layered = request.rule->neighborhood == Neighborhood::Layer;
    constants.flags = (request.animate ? BUILD_ANIMATE : 0) |
                      (request.writeBlockList ? BUILD_DRAW : 0) |
                      (request.collectStats ? BUILD_STATS : 0) | (layered ? BUILD_LAYERS : 0);
    constants.maxInstances = MAX_BLOCK_INSTANCES;
    constants.cullDistance = request.cullDistance;
    constants.cameraX = request.eye.x;
    constants.cameraY = request.eye.y;
    constants.cameraZ = request.eye.z;
    dispatchOverChunks(cmd, 0, chunkCount, constants);
}

// Hands the results to the indirect draw and the vertex shader, and copies the
// draw count and (with collectStats) the chunk stats where the CPU can read them.
void SimulationPasses::recordReadback(VkCommandBuffer cmd, const BatchRequest& request,
                                      const ChunkWorld& world) {
    // Read-after-write: the build's results are read by later frames' draws (as
    // the indirect command and by the vertex shader) and by the copies below.
    memoryBarrier(cmd, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_WRITE_BIT,
                  VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT | VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT |
                      VK_PIPELINE_STAGE_2_COPY_BIT | VK_PIPELINE_STAGE_2_HOST_BIT,
                  VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT | VK_ACCESS_2_SHADER_READ_BIT |
                      VK_ACCESS_2_TRANSFER_READ_BIT | VK_ACCESS_2_HOST_READ_BIT);
    VkBufferCopy drawCopy{0, 0, sizeof(VkDrawIndexedIndirectCommand)};
    vkCmdCopyBuffer(cmd, indirectBuffer_.buffer, readbackBuffer_.buffer, 1, &drawCopy);
    if (request.collectStats) {
        const ChunkPool& pool = world.pool();
        VkBufferCopy statsCopy{0, 0, pool.stats.size};
        vkCmdCopyBuffer(cmd, pool.stats.buffer, pool.statsReadback.buffer, 1, &statsCopy);
    }
    // The copies land before the CPU reads them after the fence (same as
    // copiesVisibleToHost, minus host writes).
    memoryBarrier(cmd, VK_PIPELINE_STAGE_2_COPY_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT,
                  VK_PIPELINE_STAGE_2_HOST_BIT, VK_ACCESS_2_HOST_READ_BIT);
}

uint64_t SimulationPasses::visibleBlocks() const {
    return readbackBuffer_.as<VkDrawIndexedIndirectCommand>()->instanceCount;
}

// A build that also writes the block list is charged to the stats beyond what
// a block list alone costs.
void SimulationPasses::recordBuildCost(bool writeBlockList, bool collectStats, double buildMs) {
    if (!collectStats) {
        smoothCost(costs_.drawMs, buildMs);
    } else if (writeBlockList) {
        smoothCost(costs_.statsMs, std::max(MIN_PASS_MS, buildMs - costs_.drawMs));
    } else {
        smoothCost(costs_.statsMs, buildMs);
    }
}

// Folds the batch that just finished into the cost estimates (see PassCosts
// for which build each estimate stands for).
void SimulationPasses::updateCosts(const BatchRequest& request, double wallMs) {
    std::array<uint64_t, BatchTimestampCount> ticks{};
    bool haveTimestamps =
        timestamps_ &&
        vkGetQueryPoolResults(gpu_->device, timestamps_, 0, BatchTimestampCount, sizeof(ticks),
                              ticks.data(), sizeof(uint64_t), VK_QUERY_RESULT_64_BIT) == VK_SUCCESS;
    const uint32_t steps = request.steps;
    const bool built = request.writeBlockList || request.collectStats;
    auto recordBuildCost = [&](double buildMs) {
        this->recordBuildCost(request.writeBlockList, request.collectStats, buildMs);
    };
    if (!haveTimestamps) {
        // Charge the whole wall time to the steps (or to the build alone).
        if (steps > 0) {
            smoothCost(costs_.stepMs, std::max(MIN_PASS_MS, wallMs / steps));
        } else if (built) {
            recordBuildCost(wallMs);
        }
        return;
    }
    // Unsigned subtraction wraps modulo 2^64; masking to the counter's valid
    // bits turns that into the right difference even when the counter itself
    // wrapped between the two timestamps.
    auto elapsedMs = [&](Timestamp from, Timestamp to) {
        uint64_t elapsedTicks = (ticks[to] - ticks[from]) & timestampMask_;
        return std::max(MIN_PASS_MS, static_cast<double>(elapsedTicks) * msPerTimestampTick_);
    };
    double stepMs = elapsedMs(BeforeSteps, AfterSteps);
    double buildMs = elapsedMs(AfterSteps, AfterBuild);
    if (steps > 0) smoothCost(costs_.stepMs, stepMs / steps);
    if (built) recordBuildCost(buildMs);
    smoothCost(costs_.overheadMs, std::max(0.0, wallMs - stepMs - buildMs));
}

} // namespace gol3d

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

// Push constants of life3d_step.comp.
struct StepConstants {
    uint32_t firstIndex; // first entry of the active list in this dispatch
    uint32_t count;      // entries in this dispatch
    uint32_t surviveMask;
    uint32_t birthMask;
};

// Push constants of life3d_build.comp.
struct BuildConstants {
    glm::mat4 cullViewProjection;
    uint32_t firstIndex;
    uint32_t count;
    uint32_t margin; // cells from a face that count as reaching the neighbor chunk
    uint32_t flags;  // BUILD_* bits below
    uint32_t maxInstances;
    float cullDistance;
    float cameraX, cameraY, cameraZ;
};
constexpr uint32_t BUILD_ANIMATE = 1; // flag births, draw the last deaths
constexpr uint32_t BUILD_DRAW = 2;    // write the block list
constexpr uint32_t BUILD_STATS = 4;   // count population and reach

// Both passes use one descriptor set layout: these bindings, all storage
// buffers, in the order of life3d_storage.glsl and the shaders.
enum Binding : uint32_t {
    CurrentCells, // the generation being read
    OtherCells,   // the next generation (step) or the previous one (build)
    Neighbors,
    ActiveList,
    BlockPool,
    BlockSlots,
    Stats,
    Instances,
    IndirectDraw,
    Origins,
    BindingCount,
};

// The spec only guarantees 65535 workgroups per dispatch dimension.
constexpr uint32_t MAX_DISPATCH_CHUNKS = 65535;

// Timestamp query slots.
enum Timestamp : uint32_t { BeforeSteps, AfterSteps, AfterBuild, TimestampCount };

} // namespace

// ------------------------------------------------------------------- setup

void SimulationPasses::init(const GpuContext& gpu, BufferAllocator& allocator,
                            ImmediateCommands& commands, const std::filesystem::path& shaderDir,
                            const ChunkWorld& world) {
    gpu_ = &gpu;
    allocator_ = &allocator;
    commands_ = &commands;
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
    VkDescriptorPoolSize poolSize{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2 * BindingCount};
    VkDescriptorPoolCreateInfo poolInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = &poolSize;
    poolInfo.maxSets = 2;
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

void SimulationPasses::writeDescriptors(const ChunkWorld& world) {
    const ChunkPool& pool = world.pool();
    for (uint32_t current = 0; current < 2; ++current) {
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
        msPerTimestampTick_ = nsPerTick / 1e6;
        timestampMask_ = validBits >= 64 ? ~0ull : (1ull << validBits) - 1;
    }
}

// --------------------------------------------------------------- a batch

// Each chunk is four workgroups of 32 x 8 rows; dispatches cover at most
// MAX_DISPATCH_CHUNKS chunks of the active list each.
template <typename PushConstants>
void SimulationPasses::dispatchOverChunks(VkCommandBuffer cmd, uint32_t chunkCount,
                                          PushConstants constants) {
    for (uint32_t first = 0; first < chunkCount; first += MAX_DISPATCH_CHUNKS) {
        constants.firstIndex = first;
        constants.count = std::min(MAX_DISPATCH_CHUNKS, chunkCount - first);
        vkCmdPushConstants(cmd, pipelineLayout_, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(constants),
                           &constants);
        vkCmdDispatch(cmd, CHUNK_SIZE / 8, constants.count, 1);
    }
}

void SimulationPasses::run(const BatchRequest& request, ChunkWorld& world) {
    const auto wallStart = std::chrono::steady_clock::now();
    VkCommandBuffer cmd = commands_->begin();
    if (timestamps_) {
        vkCmdResetQueryPool(cmd, timestamps_, 0, TimestampCount);
        vkCmdWriteTimestamp2(cmd, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, timestamps_, BeforeSteps);
    }
    // Frames still in flight may be drawing the block list this batch rewrites.
    memoryBarrier(cmd,
                  VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT |
                      VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                  VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT |
                      VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT,
                  VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_2_CLEAR_BIT |
                      VK_PIPELINE_STAGE_2_COPY_BIT,
                  VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT |
                      VK_ACCESS_2_TRANSFER_WRITE_BIT);

    const uint32_t newest = recordSteps(cmd, request, world);
    if (timestamps_) {
        vkCmdWriteTimestamp2(cmd, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, timestamps_, AfterSteps);
    }
    recordBuild(cmd, request, world, newest);
    if (timestamps_) {
        vkCmdWriteTimestamp2(cmd, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, timestamps_, AfterBuild);
    }
    recordReadback(cmd, request, world);

    const auto submitStart = std::chrono::steady_clock::now();
    commands_->submitAndWait(request.steps ? "the simulation pass" : "the block list");
    totalGpuMs_ += millisecondsSince(submitStart);

    world.setCurrentBuffer(newest);
    updateCosts(request.steps, request.writeBlockList && !request.collectStats,
                millisecondsSince(wallStart));
}

// Records the steps; each reads one cell buffer and writes the other. Returns
// the buffer that will hold the newest generation.
uint32_t SimulationPasses::recordSteps(VkCommandBuffer cmd, const BatchRequest& request,
                                       const ChunkWorld& world) {
    const uint32_t chunkCount = world.activeChunkCount();
    uint32_t current = world.currentBuffer();
    if (request.steps == 0) return current;
    if (chunkCount == 0) return (current + request.steps) & 1u; // an empty world stays empty

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, stepPipeline_);
    const StepConstants constants{0, 0, request.rule->surviveMask, request.rule->birthMask};
    for (uint32_t step = 0; step < request.steps; ++step) {
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout_, 0, 1,
                                &descriptorSets_[current], 0, nullptr);
        dispatchOverChunks(cmd, chunkCount, constants);
        // The next step reads what this one wrote.
        memoryBarrier(cmd, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_WRITE_BIT,
                      VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                      VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT);
        current = 1 - current;
    }
    return current;
}

// Records the build pass over the newest generation. It accumulates into the
// stats and the draw count, so those are cleared first.
void SimulationPasses::recordBuild(VkCommandBuffer cmd, const BatchRequest& request,
                                   const ChunkWorld& world, uint32_t newest) {
    if (request.collectStats) vkCmdFillBuffer(cmd, world.pool().stats.buffer, 0, VK_WHOLE_SIZE, 0);
    if (request.writeBlockList) {
        const VkDrawIndexedIndirectCommand emptyDraw{BLOCK_INDEX_COUNT, 0, 0, 0, 0};
        vkCmdUpdateBuffer(cmd, indirectBuffer_.buffer, 0, sizeof(emptyDraw), &emptyDraw);
    }
    memoryBarrier(cmd, VK_PIPELINE_STAGE_2_CLEAR_BIT | VK_PIPELINE_STAGE_2_COPY_BIT,
                  VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                  VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT);

    const uint32_t chunkCount = world.activeChunkCount();
    if (chunkCount == 0) return;
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, buildPipeline_);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout_, 0, 1,
                            &descriptorSets_[newest], 0, nullptr);
    BuildConstants constants{};
    constants.cullViewProjection = request.cullViewProjection;
    constants.margin = MAX_BATCH;
    constants.flags = (request.animate ? BUILD_ANIMATE : 0) |
                      (request.writeBlockList ? BUILD_DRAW : 0) |
                      (request.collectStats ? BUILD_STATS : 0);
    constants.maxInstances = MAX_BLOCK_INSTANCES;
    constants.cullDistance = request.cullDistance;
    constants.cameraX = request.eye.x;
    constants.cameraY = request.eye.y;
    constants.cameraZ = request.eye.z;
    dispatchOverChunks(cmd, chunkCount, constants);
}

// Hands the results to the indirect draw and the vertex shader, and copies the
// draw count and (with collectStats) the chunk stats where the CPU can read them.
void SimulationPasses::recordReadback(VkCommandBuffer cmd, const BatchRequest& request,
                                      const ChunkWorld& world) {
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
    memoryBarrier(cmd, VK_PIPELINE_STAGE_2_COPY_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT,
                  VK_PIPELINE_STAGE_2_HOST_BIT, VK_ACCESS_2_HOST_READ_BIT);
}

uint64_t SimulationPasses::visibleBlocks() const {
    return readbackBuffer_.as<VkDrawIndexedIndirectCommand>()->instanceCount;
}

// Folds the batch that just finished into the cost estimates.
void SimulationPasses::updateCosts(uint32_t steps, bool drawOnly, double wallMs) {
    uint64_t ticks[TimestampCount] = {};
    bool haveTimestamps =
        timestamps_ &&
        vkGetQueryPoolResults(gpu_->device, timestamps_, 0, TimestampCount, sizeof(ticks), ticks,
                              sizeof(uint64_t), VK_QUERY_RESULT_64_BIT) == VK_SUCCESS;
    double& buildCost = drawOnly ? costs_.drawMs : costs_.statsMs;
    if (!haveTimestamps) {
        // Charge the whole wall time to the steps (or to the build alone).
        if (steps > 0) {
            smoothCost(costs_.stepMs, std::max(1e-4, wallMs / steps));
        } else {
            smoothCost(buildCost, wallMs);
        }
        return;
    }
    // Counters may wrap past their valid bits; masking the difference handles
    // that. A pass never reads as free, so costs stay positive.
    auto elapsedMs = [&](Timestamp from, Timestamp to) {
        uint64_t elapsedTicks = (ticks[to] - ticks[from]) & timestampMask_;
        return std::max(1e-4, static_cast<double>(elapsedTicks) * msPerTimestampTick_);
    };
    double stepMs = elapsedMs(BeforeSteps, AfterSteps);
    double buildMs = elapsedMs(AfterSteps, AfterBuild);
    if (steps > 0) smoothCost(costs_.stepMs, stepMs / steps);
    smoothCost(buildCost, buildMs);
    smoothCost(costs_.overheadMs, std::max(0.0, wallMs - stepMs - buildMs));
}

} // namespace gol3d

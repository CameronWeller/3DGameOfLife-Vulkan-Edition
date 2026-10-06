#pragma once

// The two compute passes that run the world on the GPU:
//
//   step  (shaders/life3d_step.comp)  advances every active chunk one generation,
//         reading one cell buffer and writing the other;
//   build (shaders/life3d_build.comp) runs after the last step of a batch. It
//         counts each chunk's population, flags which neighbor chunks its life
//         could reach (the CPU then adds and frees chunks, see
//         ChunkWorld::maintain), and writes the block list for drawing.
//
// A *batch* is up to MAX_BATCH steps and one build in a single submission. The
// CPU waits for each batch, and GPU timestamps around the passes feed the tick
// governor's cost estimates.

#include <array>
#include <filesystem>

#include <glm/glm.hpp>
#include <volk.h>

#include "gpu/GpuBuffer.h"
#include "life/LifeRules.h"
#include "world/PassCosts.h"

namespace gol3d {

class ChunkWorld;
class GpuContext;
class ImmediateCommands;

// Generations per batch. The build pass flags neighbor chunks for live cells
// within this many cells of a face, and life spreads at most one cell per
// generation, so this many generations can run before the CPU must add chunks.
constexpr uint32_t MAX_BATCH = 8;

struct BatchRequest {
    uint32_t steps = 0;         // generations to advance, at most MAX_BATCH
    bool animate = false;       // flag births and keep the last deaths in the block list
    bool writeBlockList = true; // rebuild the block list for drawing
    bool collectStats = true;   // gather per-chunk population and reach for maintain()
    const LifeRule* rule = nullptr;
    glm::mat4 cullViewProjection{1.0f}; // chunks outside this view are not drawn
    glm::vec3 eye{0.0f};
    float cullDistance = 0.0f; // chunks farther than this are not drawn
};

class SimulationPasses {
public:
    void init(const GpuContext& gpu, BufferAllocator& allocator, ImmediateCommands& commands,
              const std::filesystem::path& shaderDir, const ChunkWorld& world);
    void destroy();

    // Points the descriptor sets at the world's current buffers (after growth).
    void writeDescriptors(const ChunkWorld& world);

    // Runs one batch and waits for it. Afterwards the world's current buffer is
    // the newest generation and, with collectStats, the stats are read back.
    void run(const BatchRequest& request, ChunkWorld& world);

    // Blocks in the last block list, before the MAX_BLOCK_INSTANCES cap.
    uint64_t visibleBlocks() const;
    const PassCosts& costs() const { return costs_; }
    // Total wall time spent waiting for batches (reported by --bench).
    double totalGpuMilliseconds() const { return totalGpuMs_; }

    // The block list, for drawing.
    const GpuBuffer& instanceBuffer() const { return instanceBuffer_; }
    const GpuBuffer& indirectBuffer() const { return indirectBuffer_; }

private:
    void createPipelines(const std::filesystem::path& shaderDir);
    VkPipeline createPipeline(const std::filesystem::path& shader);
    void createDescriptorSets(const ChunkWorld& world);
    void createTimestampQueries();
    template <typename PushConstants>
    void dispatchOverChunks(VkCommandBuffer cmd, uint32_t chunkCount, PushConstants constants);
    uint32_t recordSteps(VkCommandBuffer cmd, const BatchRequest& request, const ChunkWorld& world);
    void recordBuild(VkCommandBuffer cmd, const BatchRequest& request, const ChunkWorld& world,
                     uint32_t newest);
    void recordReadback(VkCommandBuffer cmd, const BatchRequest& request, const ChunkWorld& world);
    void updateCosts(uint32_t steps, bool drawOnly, double wallMs);

    const GpuContext* gpu_ = nullptr;
    BufferAllocator* allocator_ = nullptr;
    ImmediateCommands* commands_ = nullptr;

    VkDescriptorSetLayout setLayout_ = VK_NULL_HANDLE;
    VkPipelineLayout pipelineLayout_ = VK_NULL_HANDLE;
    VkPipeline stepPipeline_ = VK_NULL_HANDLE;
    VkPipeline buildPipeline_ = VK_NULL_HANDLE;
    VkDescriptorPool descriptorPool_ = VK_NULL_HANDLE;
    // descriptorSets_[b] reads cell buffer b as the current generation.
    std::array<VkDescriptorSet, 2> descriptorSets_{};

    GpuBuffer instanceBuffer_; // the block list
    GpuBuffer indirectBuffer_; // VkDrawIndexedIndirectCommand for drawing it
    GpuBuffer readbackBuffer_; // a CPU copy of the indirect command, for the block count

    // GPU timestamps: before the steps, after the steps, after the build.
    VkQueryPool timestamps_ = VK_NULL_HANDLE;
    double msPerTimestampTick_ = 0.0;
    uint64_t timestampMask_ = ~0ull; // the bits a timestamp counter actually has

    PassCosts costs_;
    double totalGpuMs_ = 0.0;
};

} // namespace gol3d

#pragma once

// GPU work the CPU waits for: record into begin()'s command buffer, then
// submitAndWait(). The simulation passes, chunk-pool growth and edit snapshots
// all go through one of these. Waiting keeps the CPU's view of the world simple
// (after a pass returns, its results are in memory), and the tick governor
// keeps the waits within the frame budget.

#include <volk.h>

namespace gol3d {

class GpuContext;

class ImmediateCommands {
public:
    void init(const GpuContext& gpu);
    void destroy();

    // Resets the command buffer and starts recording.
    VkCommandBuffer begin();
    // Ends recording, submits, and waits; `what` names the work in errors.
    void submitAndWait(const char* what);

private:
    const GpuContext* gpu_ = nullptr;
    VkCommandBuffer commandBuffer_ = VK_NULL_HANDLE;
    VkFence fence_ = VK_NULL_HANDLE;
};

} // namespace gol3d

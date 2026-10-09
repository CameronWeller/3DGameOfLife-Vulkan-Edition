#pragma once

// GPU work the CPU waits for: record into begin()'s command buffer, then
// submitAndWait(). The simulation passes, chunk-pool growth and edit snapshots
// all go through one of these. Waiting keeps the CPU's view of the world simple
// (after a pass returns, its results are in memory), and the tick governor
// keeps the waits within the frame budget.
//
// submit() leaves the work running instead, for work whose results the CPU does
// not read (a slice of a sliced generation): the frame is drawn meanwhile. The
// next begin() or wait() waits for it.

#include <volk.h>

namespace gol3d {

class GpuContext;

class ImmediateCommands {
public:
    void init(const GpuContext& gpu);
    void destroy();

    // Waits for any work still running, then resets the command buffer and
    // starts recording.
    VkCommandBuffer begin();
    // Ends recording, submits to the GPU's queue, and blocks until the GPU has
    // finished; `what` names the work in errors.
    void submitAndWait(const char* what);
    // Ends recording and submits without waiting.
    void submit(const char* what);
    // Blocks until the last submission has finished (at once if it has).
    void wait();

private:
    const GpuContext* gpu_ = nullptr;
    VkCommandBuffer commandBuffer_ = VK_NULL_HANDLE;
    VkFence fence_ = VK_NULL_HANDLE;
    const char* pending_ = nullptr; // what was submitted and not waited for yet
};

} // namespace gol3d

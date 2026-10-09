// One reusable command buffer and fence for GPU work the CPU waits on.

#include "gpu/ImmediateCommands.h"

#include <stdexcept>

#include "gpu/GpuContext.h"

namespace gol3d {

void ImmediateCommands::init(const GpuContext& gpu) {
    gpu_ = &gpu;
    VkCommandBufferAllocateInfo allocInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    allocInfo.commandPool = gpu.commandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY; // submitted directly to the queue
    allocInfo.commandBufferCount = 1;
    if (vkAllocateCommandBuffers(gpu.device, &allocInfo, &commandBuffer_) != VK_SUCCESS) {
        throw std::runtime_error("Could not allocate compute command buffer.");
    }
    VkFenceCreateInfo fenceInfo{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    if (vkCreateFence(gpu.device, &fenceInfo, nullptr, &fence_) != VK_SUCCESS) {
        throw std::runtime_error("Could not create compute fence.");
    }
}

void ImmediateCommands::destroy() {
    if (!gpu_) return;
    // The command buffer is freed with the context's command pool.
    vkDestroyFence(gpu_->device, fence_, nullptr);
    fence_ = VK_NULL_HANDLE;
    commandBuffer_ = VK_NULL_HANDLE;
    gpu_ = nullptr;
}

VkCommandBuffer ImmediateCommands::begin() {
    // Safe to reset once the GPU is done with the buffer.
    wait();
    vkResetCommandBuffer(commandBuffer_, 0);
    VkCommandBufferBeginInfo beginInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT; // re-recorded before each use
    vkBeginCommandBuffer(commandBuffer_, &beginInfo);
    return commandBuffer_;
}

void ImmediateCommands::submit(const char* what) {
    vkEndCommandBuffer(commandBuffer_);
    VkCommandBufferSubmitInfo commandInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO};
    commandInfo.commandBuffer = commandBuffer_;
    VkSubmitInfo2 submitInfo{VK_STRUCTURE_TYPE_SUBMIT_INFO_2};
    submitInfo.commandBufferInfoCount = 1;
    submitInfo.pCommandBufferInfos = &commandInfo;
    // The last submission left the fence signaled; lower it so that a wait
    // sees this submission finish.
    vkResetFences(gpu_->device, 1, &fence_);
    if (vkQueueSubmit2(gpu_->queue, 1, &submitInfo, fence_) != VK_SUCCESS) {
        throw std::runtime_error("Could not submit compute work.");
    }
    pending_ = what;
}

void ImmediateCommands::wait() {
    if (!pending_) return;
    const char* what = pending_;
    pending_ = nullptr;
    gpu_->waitForFence(fence_, what);
}

void ImmediateCommands::submitAndWait(const char* what) {
    submit(what);
    wait();
}

} // namespace gol3d

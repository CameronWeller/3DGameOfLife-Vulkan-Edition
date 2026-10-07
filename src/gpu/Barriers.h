#pragma once

// Pipeline barriers (Vulkan synchronization2), spelled out as "work of these
// stages, with these accesses, must finish before work of those stages with
// those accesses starts".
//
// The GPU runs the commands of a command buffer overlapped and out of order, and
// its caches do not see each other's writes. A barrier is how a command buffer
// says "this later work depends on that earlier work": it waits for the source
// stages and makes their writes visible to the destination accesses. Without
// one, a compute pass could read a buffer that a copy before it has not
// finished writing.

#include <volk.h>

namespace gol3d {

// Orders all memory accesses: `src` stage/access before `dst` stage/access.
inline void memoryBarrier(VkCommandBuffer cmd, VkPipelineStageFlags2 srcStage,
                          VkAccessFlags2 srcAccess, VkPipelineStageFlags2 dstStage,
                          VkAccessFlags2 dstAccess) {
    VkMemoryBarrier2 barrier{VK_STRUCTURE_TYPE_MEMORY_BARRIER_2};
    barrier.srcStageMask = srcStage;
    barrier.srcAccessMask = srcAccess;
    barrier.dstStageMask = dstStage;
    barrier.dstAccessMask = dstAccess;
    VkDependencyInfo dependency{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
    dependency.memoryBarrierCount = 1;
    dependency.pMemoryBarriers = &barrier;
    vkCmdPipelineBarrier2(cmd, &dependency);
}

// The common "GPU copies are done; the CPU may now read or write the memory".
// The CPU must still wait for the submission's fence before touching it: the
// barrier only makes the copied data visible to the host once the work is done.
inline void copiesVisibleToHost(VkCommandBuffer cmd) {
    memoryBarrier(cmd, VK_PIPELINE_STAGE_2_COPY_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT,
                  VK_PIPELINE_STAGE_2_HOST_BIT,
                  VK_ACCESS_2_HOST_READ_BIT | VK_ACCESS_2_HOST_WRITE_BIT);
}

// Moves one mip level and layer of an image from layout `from` to `to`, with
// the same ordering as memoryBarrier. A layout is how the GPU arranges an
// image's pixels for one kind of use (drawing into, copying from, presenting);
// an image must be in the right layout before each use.
inline void imageBarrier(VkCommandBuffer cmd, VkImage image, VkImageAspectFlags aspect,
                         VkImageLayout from, VkImageLayout to, VkPipelineStageFlags2 srcStage,
                         VkAccessFlags2 srcAccess, VkPipelineStageFlags2 dstStage,
                         VkAccessFlags2 dstAccess) {
    VkImageMemoryBarrier2 barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2};
    barrier.srcStageMask = srcStage;
    barrier.srcAccessMask = srcAccess;
    barrier.dstStageMask = dstStage;
    barrier.dstAccessMask = dstAccess;
    barrier.oldLayout = from;
    barrier.newLayout = to;
    // Not a hand-over between queue families: the game uses a single queue.
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange = {.aspectMask = aspect,
                                .baseMipLevel = 0,
                                .levelCount = 1,
                                .baseArrayLayer = 0,
                                .layerCount = 1};
    VkDependencyInfo dependency{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
    dependency.imageMemoryBarrierCount = 1;
    dependency.pImageMemoryBarriers = &barrier;
    vkCmdPipelineBarrier2(cmd, &dependency);
}

} // namespace gol3d

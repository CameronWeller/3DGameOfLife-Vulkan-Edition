#include "VulkanMemoryManager.h"
#include "VulkanContext.h"
#include <stdexcept>
#include <algorithm>

namespace VulkanHIP {

VulkanMemoryManager::VulkanMemoryManager(VkDevice device, VkPhysicalDevice physicalDevice)
    : device_(device), physicalDevice_(physicalDevice) {
}

void VulkanMemoryManager::destroyBuffer(const BufferAllocation& allocation) {
    if (allocation.buffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(device_, allocation.buffer, nullptr);
    }
    if (allocation.memory != VK_NULL_HANDLE) {
        vkFreeMemory(device_, allocation.memory, nullptr);
    }
}

void* VulkanMemoryManager::mapMemory(const BufferAllocation& allocation) {
    void* data;
    vkMapMemory(device_, allocation.memory, 0, allocation.size, 0, &data);
    return data;
}

void VulkanMemoryManager::unmapMemory(const BufferAllocation& allocation) {
    vkUnmapMemory(device_, allocation.memory);
}

void VulkanMemoryManager::copyBuffer(
    const BufferAllocation& src,
    const BufferAllocation& dst,
    VkDeviceSize size) {
    
    VkCommandBuffer commandBuffer = beginSingleTimeCommands();

    VkBufferCopy copyRegion{};
    copyRegion.size = size;
    vkCmdCopyBuffer(commandBuffer, src.buffer, dst.buffer, 1, &copyRegion);

    endSingleTimeCommands(commandBuffer);
}

VkCommandBuffer VulkanMemoryManager::beginSingleTimeCommands() {
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = commandPool_;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    vkAllocateCommandBuffers(device_, &allocInfo, &commandBuffer);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(commandBuffer, &beginInfo);

    return commandBuffer;
}

void VulkanMemoryManager::endSingleTimeCommands(VkCommandBuffer commandBuffer) {
    vkEndCommandBuffer(commandBuffer);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    vkQueueSubmit(VulkanContext::getInstance().getGraphicsQueue(), 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(VulkanContext::getInstance().getGraphicsQueue());

    vkFreeCommandBuffers(device_, commandPool_, 1, &commandBuffer);
}

void VulkanMemoryManager::createCommandPool() {
    auto& context = VulkanContext::getInstance();
    QueueFamilyIndices queueFamilyIndices = context.findQueueFamilies(physicalDevice_);

    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value();
    poolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;

    if (vkCreateCommandPool(device_, &poolInfo, nullptr, &commandPool_) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create command pool!");
    }
}

void VulkanMemoryManager::destroyCommandPool() {
    if (commandPool_ != VK_NULL_HANDLE) {
        vkDestroyCommandPool(device_, commandPool_, nullptr);
        commandPool_ = VK_NULL_HANDLE;
    }
}

BufferAllocation VulkanMemoryManager::allocateBuffer(
    VkDeviceSize size,
    VkBufferUsageFlags usage,
    VkMemoryPropertyFlags properties) {
    BufferAllocation allocation{};

    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo allocCreateInfo = {};
    allocCreateInfo.usage = (properties & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) ? VMA_MEMORY_USAGE_CPU_TO_GPU : VMA_MEMORY_USAGE_GPU_ONLY;

    if (vmaCreateBuffer(allocator_, &bufferInfo, &allocCreateInfo, &allocation.buffer, &allocation.allocation, &allocation.allocationInfo) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create buffer with VMA!");
    }
    return allocation;
}

VulkanMemoryManager::StagingBuffer VulkanMemoryManager::getStagingBuffer(VkDeviceSize size) {
    // For simplicity, always create a new staging buffer (could be pooled for efficiency)
    StagingBuffer staging{};
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo allocCreateInfo = {};
    allocCreateInfo.usage = VMA_MEMORY_USAGE_CPU_ONLY;
    allocCreateInfo.requiredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

    if (vmaCreateBuffer(allocator_, &bufferInfo, &allocCreateInfo, &staging.buffer, &staging.allocation, &staging.allocationInfo) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create staging buffer!");
    }
    staging.size = size;
    vmaMapMemory(allocator_, staging.allocation, &staging.mappedData);
    return staging;
}

void VulkanMemoryManager::returnStagingBuffer(StagingBuffer& buffer) {
    if (buffer.mappedData) {
        vmaUnmapMemory(allocator_, buffer.allocation);
        buffer.mappedData = nullptr;
    }
    if (buffer.buffer != VK_NULL_HANDLE) {
        vmaDestroyBuffer(allocator_, buffer.buffer, buffer.allocation);
        buffer.buffer = VK_NULL_HANDLE;
        buffer.allocation = nullptr;
    }
}

} // namespace VulkanHIP 
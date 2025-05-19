#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include <memory>
#include <stdexcept>
#include <algorithm>

// No need to forward declare VulkanEngine if physicalDevice is passed in constructor

class MemoryPool {
public:
    struct BufferAllocation {
        VkBuffer buffer = VK_NULL_HANDLE;
        VkDeviceMemory memory = VK_NULL_HANDLE;
        VkDeviceSize size = 0;
        VkBufferUsageFlags usage = 0;
        VkMemoryPropertyFlags properties = 0;
        bool inUse = false;
    };

    struct StagingBuffer {
        VkBuffer buffer = VK_NULL_HANDLE;
        VkDeviceMemory memory = VK_NULL_HANDLE;
        VkDeviceSize size = 0;
        bool inUse = false;
    };

    MemoryPool(VkDevice device, VkPhysicalDevice physicalDevice);
    ~MemoryPool();

    MemoryPool(const MemoryPool&) = delete;
    MemoryPool& operator=(const MemoryPool&) = delete;

    BufferAllocation allocateBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties);
    void freeBuffer(const BufferAllocation& allocation);
    StagingBuffer getStagingBuffer(VkDeviceSize size);
    void returnStagingBuffer(const StagingBuffer& buffer);

private:
    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);

    VkDevice device_;
    VkPhysicalDevice physicalDevice_;
    std::vector<BufferAllocation> bufferPool_;
    std::vector<StagingBuffer> stagingPool_;
    VkDeviceSize maxStagingSize_ = 0;
}; 
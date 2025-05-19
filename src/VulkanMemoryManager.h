#pragma once

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>
#include <memory>
#include <vector>
#include <string>

class VulkanMemoryManager {
public:
    struct BufferAllocation {
        VkBuffer buffer;
        VmaAllocation allocation;
        VkDeviceSize size;
        bool inUse;
    };

    struct StagingBuffer {
        VkBuffer buffer;
        VmaAllocation allocation;
        VkDeviceSize size;
        bool inUse;
    };

    VulkanMemoryManager(VkDevice device, VkPhysicalDevice physicalDevice);
    ~VulkanMemoryManager();

    // Delete copy constructor and assignment operator
    VulkanMemoryManager(const VulkanMemoryManager&) = delete;
    VulkanMemoryManager& operator=(const VulkanMemoryManager&) = delete;

    // Buffer management
    BufferAllocation allocateBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties);
    void freeBuffer(const BufferAllocation& allocation);
    
    // Staging buffer management
    StagingBuffer getStagingBuffer(VkDeviceSize size);
    void returnStagingBuffer(const StagingBuffer& staging);

    // Memory type finding
    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) const;

    // Getters
    VmaAllocator getAllocator() const { return allocator_; }
    VkDevice getDevice() const { return device_; }

private:
    VkDevice device_;
    VkPhysicalDevice physicalDevice_;
    VmaAllocator allocator_;
    
    std::vector<BufferAllocation> bufferPool_;
    std::vector<StagingBuffer> stagingPool_;
    VkDeviceSize maxStagingSize_;

    void createAllocator();
    void destroyAllocator();
}; 
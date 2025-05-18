#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include <memory>

class MemoryPool {
public:
    struct BufferAllocation {
        VkBuffer buffer;
        VkDeviceMemory memory;
        VkDeviceSize size;
        VkBufferUsageFlags usage;
        VkMemoryPropertyFlags properties;
        bool inUse;
    };

    struct StagingBuffer {
        VkBuffer buffer;
        VkDeviceMemory memory;
        VkDeviceSize size;
        bool inUse;
    };

    explicit MemoryPool(VkDevice device);
    ~MemoryPool();

    // Prevent copying
    MemoryPool(const MemoryPool&) = delete;
    MemoryPool& operator=(const MemoryPool&) = delete;

    // Buffer management
    BufferAllocation allocateBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties);
    void freeBuffer(const BufferAllocation& allocation);
    
    // Staging buffer management
    StagingBuffer getStagingBuffer(VkDeviceSize size);
    void returnStagingBuffer(const StagingBuffer& buffer);

private:
    VkDevice device;
    std::vector<BufferAllocation> bufferPool;
    std::vector<StagingBuffer> stagingPool;
    VkDeviceSize maxStagingSize;
    
    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
}; 
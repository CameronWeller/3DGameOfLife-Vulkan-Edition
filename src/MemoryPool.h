#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include <memory>

// Forward declaration
class VulkanEngine;

/**
 * @brief A buffer allocation from the memory pool
 */
struct BufferAllocation {
    VkBuffer buffer;
    VkDeviceMemory memory;
    VkDeviceSize size;
    VkBufferUsageFlags usage;
    VkMemoryPropertyFlags memoryProperties;
    bool inUse;
};

/**
 * @brief Memory pool for efficient resource management
 */
class MemoryPool {
public:
    /**
     * @brief Construct a new Memory Pool
     * @param device The Vulkan device
     */
    MemoryPool(VkDevice device);
    
    /**
     * @brief Destroy the Memory Pool
     */
    ~MemoryPool();
    
    /**
     * @brief Allocate a buffer from the pool
     * 
     * @param size Size of the buffer
     * @param usage Buffer usage flags
     * @param properties Memory property flags
     * @return BufferAllocation The allocated buffer
     */
    BufferAllocation allocateBuffer(VkDeviceSize size, VkBufferUsageFlags usage, 
                                   VkMemoryPropertyFlags properties);
    
    /**
     * @brief Free a buffer allocation
     * 
     * @param allocation The buffer allocation to free
     */
    void freeBuffer(const BufferAllocation& allocation);
    
    /**
     * @brief Get a staging buffer for temporary use
     * 
     * @param size Size of the buffer
     * @return BufferAllocation The staging buffer
     */
    BufferAllocation getStagingBuffer(VkDeviceSize size);
    
    /**
     * @brief Return a staging buffer to the pool
     * 
     * @param allocation The staging buffer to return
     */
    void returnStagingBuffer(const BufferAllocation& allocation);
    
private:
    VkDevice device;
    std::vector<BufferAllocation> bufferAllocations;
    std::vector<BufferAllocation> stagingBuffers;
}; 
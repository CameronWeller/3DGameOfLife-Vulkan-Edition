#pragma once

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>
#include <vector>
#include <memory>
#include <mutex>

namespace VulkanHIP {

class VulkanMemoryManager {
public:
    struct BufferAllocation {
        VkBuffer buffer;
        VmaAllocation allocation;
        VmaAllocationInfo allocationInfo;
        VkDeviceSize size;
        void* mappedData;  // For host-visible buffers
    };

    struct StagingBuffer {
        VkBuffer buffer;
        VmaAllocation allocation;
        VmaAllocationInfo allocationInfo;
        void* mappedData;
        VkDeviceSize size;
    };

    VulkanMemoryManager(VkDevice device, VkPhysicalDevice physicalDevice);
    ~VulkanMemoryManager();

    // Buffer management
    BufferAllocation createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage);
    void destroyBuffer(const BufferAllocation& allocation);
    void* mapMemory(const BufferAllocation& allocation);
    void unmapMemory(const BufferAllocation& allocation);

    // Staging buffer management
    StagingBuffer createStagingBuffer(VkDeviceSize size);
    void destroyStagingBuffer(StagingBuffer& stagingBuffer);
    void* mapStagingBuffer(StagingBuffer& stagingBuffer);
    void unmapStagingBuffer(StagingBuffer& stagingBuffer);

    // Memory pool management
    void createStagingPool(VkDeviceSize size);
    void destroyStagingPool();
    StagingBuffer allocateFromStagingPool(VkDeviceSize size);
    void freeStagingBuffer(StagingBuffer& stagingBuffer);

    // Image management
    struct ImageAllocation {
        VkImage image;
        VmaAllocation allocation;
        VkDeviceSize size;
        bool inUse;
    };

    ImageAllocation allocateImage(uint32_t width, uint32_t height, VkFormat format, 
                                VkImageTiling tiling, VkImageUsageFlags usage, 
                                VkMemoryPropertyFlags properties);
    void freeImage(const ImageAllocation& allocation);
    void createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags,
                        VkImageView& imageView);
    void transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout,
                             VkImageLayout newLayout);
    void copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);
    void generateMipmaps(VkImage image, VkFormat imageFormat, int32_t texWidth, int32_t texHeight, uint32_t mipLevels);

    // Memory type finding
    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) const;

    // Getters
    VmaAllocator getAllocator() const { return allocator_; }
    VkDevice getDevice() const { return device_; }
    VkPhysicalDevice getPhysicalDevice() const { return physicalDevice_; }
    VkCommandPool getCommandPool() const { return commandPool_; }
    VkQueue getGraphicsQueue() const { return graphicsQueue_; }

    // Command buffer management
    VkCommandBuffer beginSingleTimeCommands();
    void endSingleTimeCommands(VkCommandBuffer commandBuffer);

    // Cleanup
    void cleanup();

    // Compatibility stub
    BufferAllocation allocateBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage) {
        return createBuffer(size, usage, memoryUsage);
    }

private:
    VkDevice device_;
    VkPhysicalDevice physicalDevice_;
    VmaAllocator allocator_;
    VkCommandPool commandPool_;
    VkQueue graphicsQueue_;
    
    std::vector<BufferAllocation> bufferPool_;
    std::vector<StagingBuffer> stagingPool_;
    std::mutex stagingMutex_;
    VkDeviceSize maxStagingSize_;

    void createAllocator();
    void destroyAllocator();
    void cleanupStagingBuffers();
};

} // namespace VulkanHIP 
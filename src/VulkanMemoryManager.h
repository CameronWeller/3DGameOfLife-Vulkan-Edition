#pragma once

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>
#include <memory>
#include <vector>
#include <string>
#include <mutex>

struct BufferAllocation {
    VkBuffer buffer;
    VmaAllocation allocation;
    VmaAllocationInfo allocationInfo;
};

struct StagingBuffer {
    VkBuffer buffer;
    VmaAllocation allocation;
    VmaAllocationInfo allocationInfo;
    size_t size;
};

class VulkanMemoryManager {
public:
    struct ImageAllocation {
        VkImage image;
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
    
    // Image management
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
    
    // Staging buffer management
    StagingBuffer getStagingBuffer(size_t size);
    void returnStagingBuffer(const StagingBuffer& buffer);

    // Memory type finding
    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) const;

    // Getters
    VmaAllocator getAllocator() const { return allocator_; }
    VkDevice getDevice() const { return device_; }

    void cleanup();

private:
    VkDevice device_;
    VkPhysicalDevice physicalDevice_;
    VmaAllocator allocator_;
    VkCommandPool commandPool_;
    VkQueue graphicsQueue_;
    
    std::vector<BufferAllocation> bufferPool_;
    std::vector<StagingBuffer> stagingBuffers_;
    std::mutex stagingBuffersMutex_;
    VkDeviceSize maxStagingSize_;

    void createAllocator();
    void destroyAllocator();
    VkCommandBuffer beginSingleTimeCommands();
    void endSingleTimeCommands(VkCommandBuffer commandBuffer);

    void cleanupStagingBuffers();
}; 
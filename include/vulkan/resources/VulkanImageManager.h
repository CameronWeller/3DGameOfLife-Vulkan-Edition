#pragma once

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>
#include <vector>
#include <memory>
#include <string>

namespace VulkanHIP {

class VulkanContext;
class VulkanMemoryManager;

class VulkanImageManager {
public:
    explicit VulkanImageManager(VulkanContext* context, VulkanMemoryManager* memoryManager);
    ~VulkanImageManager();
    
    // Disable copy
    VulkanImageManager(const VulkanImageManager&) = delete;
    VulkanImageManager& operator=(const VulkanImageManager&) = delete;
    
    // Image creation and management
    void createDepthResources(uint32_t width, uint32_t height, VkSampleCountFlagBits msaaSamples);
    void createColorResources(uint32_t width, uint32_t height, VkFormat format, VkSampleCountFlagBits msaaSamples);
    void createTextureImage(unsigned char* pixels, uint32_t width, uint32_t height, int channels);
    void createTextureImageView();
    void createTextureSampler();
    
    // Image operations
    void transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout);
    void copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);
    
    // Public utility methods
    VkFormat findDepthFormat();
    bool hasStencilComponent(VkFormat format);
    
    // Getters
    VkImage getDepthImage() const { return depthImage_; }
    VkImageView getDepthImageView() const { return depthImageView_; }
    VkImage getColorImage() const { return colorImage_; }
    VkImageView getColorImageView() const { return colorImageView_; }
    VkImageView getTextureImageView() const { return textureImageView_; }
    VkSampler getTextureSampler() const { return textureSampler_; }
    
    void cleanup();
    
private:
    VulkanContext* vulkanContext_;
    VulkanMemoryManager* memoryManager_;
    
    // Image resources
    VkImage depthImage_ = VK_NULL_HANDLE;
    VmaAllocation depthImageAllocation_ = VK_NULL_HANDLE;
    VkImageView depthImageView_ = VK_NULL_HANDLE;
    
    VkImage colorImage_ = VK_NULL_HANDLE;
    VmaAllocation colorImageAllocation_ = VK_NULL_HANDLE;
    VkImageView colorImageView_ = VK_NULL_HANDLE;
    
    VkImage textureImage_ = VK_NULL_HANDLE;
    VmaAllocation textureImageAllocation_ = VK_NULL_HANDLE;
    VkImageView textureImageView_ = VK_NULL_HANDLE;
    VkSampler textureSampler_ = VK_NULL_HANDLE;
    
    // Private helpers
    VkImageView createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags);
};

} // namespace VulkanHIP
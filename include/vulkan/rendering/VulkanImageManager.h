#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include <memory>

namespace VulkanHIP {

class VulkanContext;
class VulkanMemoryManager;

class VulkanImageManager {
public:
    VulkanImageManager(VulkanContext& context, VulkanMemoryManager& memoryManager);
    ~VulkanImageManager();

    // Image creation and management
    VkImage createImage(uint32_t width, uint32_t height, VkFormat format, 
                       VkImageTiling tiling, VkImageUsageFlags usage, 
                       VkMemoryPropertyFlags properties, VkDeviceMemory& imageMemory);
    
    VkImageView createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags);
    
    // Texture management
    void createTexture(const std::string& filename, VkImage& textureImage, 
                      VkDeviceMemory& textureImageMemory);
    
    // Image layout transitions
    void transitionImageLayout(VkImage image, VkFormat format, 
                              VkImageLayout oldLayout, VkImageLayout newLayout);
    
    // Copy operations
    void copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);
    
    // Depth buffer management
    VkFormat findDepthFormat();
    VkImageView getDepthImageView() const { return depthImageView_; }
    
    // Cleanup
    void cleanup();

private:
    VulkanContext& context_;
    VulkanMemoryManager& memoryManager_;
    
    std::vector<VkImage> managedImages_;
    std::vector<VkImageView> managedImageViews_;
    std::vector<VkDeviceMemory> managedMemory_;
    
    // Depth buffer resources
    VkImage depthImage_ = VK_NULL_HANDLE;
    VkDeviceMemory depthImageMemory_ = VK_NULL_HANDLE;
    VkImageView depthImageView_ = VK_NULL_HANDLE;
    
    // Helper methods
    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
    VkCommandBuffer beginSingleTimeCommands();
    void endSingleTimeCommands(VkCommandBuffer commandBuffer);
};

} // namespace VulkanHIP 
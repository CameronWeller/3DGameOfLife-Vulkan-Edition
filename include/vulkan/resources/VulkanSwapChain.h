#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include "SwapChainSupportDetails.h"

namespace VulkanHIP {

class VulkanContext;
class WindowManager;

class VulkanSwapChain {
public:
    VulkanSwapChain(VulkanContext* context, WindowManager* windowManager, VkSurfaceKHR surface);
    ~VulkanSwapChain();
    
    VulkanSwapChain(const VulkanSwapChain&) = delete;
    VulkanSwapChain& operator=(const VulkanSwapChain&) = delete;
    
    void createSwapChain();
    void createImageViews();
    void recreateSwapChain();
    void cleanup();
    
    // Getters
    VkSwapchainKHR getSwapChain() const { return swapChain_; }
    const std::vector<VkImage>& getImages() const { return swapChainImages_; }
    const std::vector<VkImageView>& getImageViews() const { return swapChainImageViews_; }
    VkFormat getImageFormat() const { return swapChainImageFormat_; }
    VkExtent2D getExtent() const { return swapChainExtent_; }
    
private:
    VulkanContext* vulkanContext_;
    WindowManager* windowManager_;
    VkSurfaceKHR surface_;
    
    VkSwapchainKHR swapChain_ = VK_NULL_HANDLE;
    std::vector<VkImage> swapChainImages_;
    std::vector<VkImageView> swapChainImageViews_;
    VkFormat swapChainImageFormat_;
    VkExtent2D swapChainExtent_;
    
    SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device);
    VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
    VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);
    VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities);
};

} // namespace VulkanHIP
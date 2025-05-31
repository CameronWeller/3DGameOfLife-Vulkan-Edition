#pragma once
#include <vulkan/vulkan.h>
#include <vector>
#include <memory>

// Forward declarations
struct SwapChainSupportDetails;

namespace VulkanHIP {

class VulkanContext;
class WindowManager;

class VulkanSwapChain {
public:
    explicit VulkanSwapChain(VulkanContext* context, WindowManager* windowManager);
    ~VulkanSwapChain();
    
    void createSwapChain();
    void createImageViews();
    void recreateSwapChain();
    void cleanup();
    
    // Getters
    VkSwapchainKHR getSwapChain() const { return swapChain_; }
    const std::vector<VkImage>& getSwapChainImages() const { return swapChainImages_; }
    const std::vector<VkImageView>& getSwapChainImageViews() const { return swapChainImageViews_; }
    VkFormat getSwapChainImageFormat() const { return swapChainImageFormat_; }
    VkExtent2D getSwapChainExtent() const { return swapChainExtent_; }
    
private:
    VulkanContext* context_;
    WindowManager* windowManager_;
    
    VkSwapchainKHR swapChain_ = VK_NULL_HANDLE;
    std::vector<VkImage> swapChainImages_;
    std::vector<VkImageView> swapChainImageViews_;
    VkFormat swapChainImageFormat_;
    VkExtent2D swapChainExtent_;
    
    // Helper methods
    SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device);
    VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
    VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);
    VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities);
};

}
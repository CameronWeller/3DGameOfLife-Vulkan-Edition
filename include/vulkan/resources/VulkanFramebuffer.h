#pragma once

#include <vulkan/vulkan.h>
#include <vector>

namespace VulkanHIP {

class VulkanContext;
class VulkanSwapChain;
class VulkanImageManager;

class VulkanFramebuffer {
public:
    VulkanFramebuffer(VulkanContext* context, VulkanSwapChain* swapChain, VulkanImageManager* imageManager);
    ~VulkanFramebuffer();
    
    VulkanFramebuffer(const VulkanFramebuffer&) = delete;
    VulkanFramebuffer& operator=(const VulkanFramebuffer&) = delete;
    
    void createRenderPass();
    void createFramebuffers();
    void cleanup();
    
    VkRenderPass getRenderPass() const { return renderPass_; }
    const std::vector<VkFramebuffer>& getFramebuffers() const { return swapChainFramebuffers_; }
    
private:
    VulkanContext* vulkanContext_;
    VulkanSwapChain* swapChain_;
    VulkanImageManager* imageManager_;
    
    VkRenderPass renderPass_ = VK_NULL_HANDLE;
    std::vector<VkFramebuffer> swapChainFramebuffers_;
};

} // namespace VulkanHIP
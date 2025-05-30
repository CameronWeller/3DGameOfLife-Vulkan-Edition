#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include <memory>

namespace VulkanHIP {

class VulkanContext;
class VulkanSwapChain;
class VulkanImageManager;
class Camera;

class VulkanRenderer {
public:
    explicit VulkanRenderer(VulkanContext* context, VulkanSwapChain* swapChain, VulkanImageManager* imageManager);
    ~VulkanRenderer();
    
    void createRenderPass();
    void createFramebuffers();
    void createGraphicsPipeline();
    void createDescriptorSetLayout();
    void createDescriptorPool();
    void createDescriptorSets();
    
    void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex);
    void updateUniformBuffer(uint32_t currentImage, Camera* camera);
    
    // Getters
    VkRenderPass getRenderPass() const { return renderPass_; }
    const std::vector<VkFramebuffer>& getSwapChainFramebuffers() const { return swapChainFramebuffers_; }
    VkPipeline getGraphicsPipeline() const { return graphicsPipeline_; }
    VkPipelineLayout getPipelineLayout() const { return pipelineLayout_; }
    
    void cleanup();
    
private:
    VulkanContext* context_;
    VulkanSwapChain* swapChain_;
    VulkanImageManager* imageManager_;
    
    VkRenderPass renderPass_ = VK_NULL_HANDLE;
    std::vector<VkFramebuffer> swapChainFramebuffers_;
    VkPipeline graphicsPipeline_ = VK_NULL_HANDLE;
    VkPipelineLayout pipelineLayout_ = VK_NULL_HANDLE;
    VkDescriptorSetLayout descriptorSetLayout_ = VK_NULL_HANDLE;
    VkDescriptorPool descriptorPool_ = VK_NULL_HANDLE;
    std::vector<VkDescriptorSet> descriptorSets_;
};

} // namespace VulkanHIP
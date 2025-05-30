#pragma once

#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <string>
#include <vector>
#include <memory>
#include "VoxelData.h"

class VulkanEngine {
public:
    VulkanEngine();
    ~VulkanEngine();
    
    // Initialization and cleanup
    void init();
    void cleanup();
    
    // Main loop
    void run();
    
    // Device access
    VkDevice getDevice() const { return device_; }
    VkCommandPool getCommandPool() const { return commandPool_; }
    VkQueue getGraphicsQueue() const { return graphicsQueue_; }
    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
    
    // Pattern preview rendering
    void renderPatternPreview(VkCommandBuffer cmdBuffer, const VoxelData& voxelData, 
                            const glm::mat4& view, const glm::mat4& proj);
    bool saveImageToFile(VkImage image, const std::string& filename);
    
    // Command buffer helpers
    VkCommandBuffer beginSingleTimeCommands();
    void endSingleTimeCommands(VkCommandBuffer commandBuffer);
    
private:
    // Vulkan instance and device
    VkInstance instance_;
    VkPhysicalDevice physicalDevice_;
    VkDevice device_;
    VkQueue graphicsQueue_;
    VkCommandPool commandPool_;
    
    // Preview rendering resources
    VkPipeline previewPipeline_;
    VkPipelineLayout previewPipelineLayout_;
    VkDescriptorSetLayout previewDescriptorSetLayout_;
    VkDescriptorPool previewDescriptorPool_;
    VkDescriptorSet previewDescriptorSet_;
    
    // Preview buffers
    VkBuffer previewVertexBuffer_;
    VkDeviceMemory previewVertexBufferMemory_;
    VkBuffer previewIndexBuffer_;
    VkDeviceMemory previewIndexBufferMemory_;
    
    // Preview shaders
    VkShaderModule previewVertexShader_;
    VkShaderModule previewFragmentShader_;
    
    // Preview textures
    VkImage previewTexture_;
    VkDeviceMemory previewTextureMemory_;
    VkImageView previewTextureView_;
    VkSampler previewTextureSampler_;
    
    // Preview lighting and materials
    struct PreviewLight {
        glm::vec3 position;
        glm::vec3 color;
        float intensity;
    };
    
    struct PreviewMaterial {
        glm::vec3 ambient;
        glm::vec3 diffuse;
        glm::vec3 specular;
        float shininess;
    };
    
    PreviewLight previewLight_;
    PreviewMaterial previewMaterial_;
    
    // Helper methods
    void createPreviewPipeline();
    void createPreviewDescriptorSetLayout();
    void createPreviewDescriptorPool();
    void createPreviewDescriptorSet();
    void createPreviewBuffers();
    void createPreviewShaders();
    void createPreviewTexture();
    void updatePreviewUniforms(const glm::mat4& view, const glm::mat4& proj);
    
    // Initialization helpers
    void createInstance();
    void pickPhysicalDevice();
    void createLogicalDevice();
    void createCommandPool();
    
    // Cleanup helpers
    void cleanupPreviewResources();
}; 
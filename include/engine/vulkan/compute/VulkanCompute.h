#pragma once

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>
#include <vector>
#include <memory>

namespace VulkanHIP {

class VulkanContext;
class Grid3D;

struct GameOfLifePushConstants {
    uint32_t gridWidth;
    uint32_t gridHeight;
    uint32_t gridDepth;
    uint32_t ruleSet;
    uint32_t surviveMin;
    uint32_t surviveMax;
    uint32_t birthCount;
};

class VulkanCompute {
public:
    explicit VulkanCompute(VulkanContext* context);
    ~VulkanCompute();
    
    void createComputePipeline();
    void createComputeDescriptorSetLayout();
    void createComputeDescriptorPool();
    void createComputeDescriptorSets();
    void createComputeBuffers(uint32_t gridSize);
    
    void submitComputeWork();
    void updateComputePushConstants(const GameOfLifePushConstants& constants);
    
    void cleanup();
    
private:
    VulkanContext* context_;
    
    VkCommandPool computeCommandPool_ = VK_NULL_HANDLE;
    VkDescriptorPool computeDescriptorPool_ = VK_NULL_HANDLE;
    
    struct ComputePipelineInfo {
        VkPipeline pipeline;
        VkPipelineLayout layout;
        VkDescriptorSetLayout descriptorSetLayout;
        VkDescriptorPool descriptorPool;
        std::vector<VkDescriptorSet> descriptorSets;
        VkBuffer stateBuffer;
        VkBuffer nextStateBuffer;
        VmaAllocation stateBufferAllocation;
        VmaAllocation nextStateBufferAllocation;
        GameOfLifePushConstants pushConstants;
    } computePipeline_;
    
    // Helper methods
    std::vector<char> readFile(const std::string& filename);
    VkShaderModule createShaderModule(const std::vector<char>& code);
};

} // namespace VulkanHIP
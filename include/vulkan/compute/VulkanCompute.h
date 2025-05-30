#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include <unordered_map>
#include <string>

namespace VulkanHIP {

class VulkanContext;
class VulkanMemoryManager;

struct ComputePipelineInfo {
    VkPipeline pipeline;
    VkPipelineLayout layout;
    VkDescriptorSetLayout descriptorSetLayout;
    VkDescriptorPool descriptorPool;
    std::vector<VkDescriptorSet> descriptorSets;
};

class VulkanCompute {
public:
    VulkanCompute(VulkanContext* context, VulkanMemoryManager* memoryManager);
    ~VulkanCompute();
    
    VulkanCompute(const VulkanCompute&) = delete;
    VulkanCompute& operator=(const VulkanCompute&) = delete;
    
    void createComputeCommandPool();
    std::string createComputePipeline(const std::string& shaderPath, 
                                     const std::vector<VkDescriptorSetLayoutBinding>& bindings);
    void destroyComputePipeline(const std::string& pipelineId);
    
    void submitComputeCommand(const std::string& pipelineId, 
                             uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ);
    
    VkSemaphore getComputeSemaphore() const { return computeSemaphore_; }
    VkFence getComputeFence() const { return computeFence_; }
    
    void cleanup();
    
private:
    VulkanContext* vulkanContext_;
    VulkanMemoryManager* memoryManager_;
    
    VkCommandPool computeCommandPool_ = VK_NULL_HANDLE;
    std::vector<VkCommandBuffer> computeCommandBuffers_;
    VkSemaphore computeSemaphore_ = VK_NULL_HANDLE;
    VkFence computeFence_ = VK_NULL_HANDLE;
    
    std::unordered_map<std::string, ComputePipelineInfo> computePipelines_;
    
    VkShaderModule createShaderModule(const std::vector<char>& code);
    std::vector<char> readFile(const std::string& filename);
};

} // namespace VulkanHIP



#pragma once
#include <vulkan/vulkan.h>
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
};

} // namespace VulkanHIP
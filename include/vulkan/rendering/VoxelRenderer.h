#pragma once

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>
#include <vector>
#include <glm/glm.hpp>
#include "VoxelData.h"
#include "VulkanMemoryManager.h"
#include "VulkanError.h"

namespace VulkanHIP {

class VulkanContext;
class VulkanMemoryManager;
class Grid3D; // Forward declaration for Grid3D integration

struct VoxelInstance {
    glm::vec3 position;
    glm::vec4 color;
    float scale;
};

struct UniformBufferObject {
    alignas(16) glm::mat4 model;
    alignas(16) glm::mat4 view;
    alignas(16) glm::mat4 proj;
};

class VoxelRenderer {
public:
    VoxelRenderer(VulkanContext* context, VulkanMemoryManager* memoryManager);
    ~VoxelRenderer();
    
    VoxelRenderer(const VoxelRenderer&) = delete;
    VoxelRenderer& operator=(const VoxelRenderer&) = delete;
    
    // Core rendering setup
    void createVoxelRenderingResources();
    void cleanup();
    
    // Data update methods
    void updateVoxelInstances(const VoxelData& voxelData);
    void updateFromGrid3D(const Grid3D& grid); // New method for Grid3D integration
    
    // Rendering methods
    void renderVoxels(VkCommandBuffer commandBuffer);
    void renderGrid(VkCommandBuffer commandBuffer, const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix);
    
    // Uniform buffer management
    void updateUniformBuffer(const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix);
    
private:
    VulkanContext* vulkanContext_;
    VulkanMemoryManager* memoryManager_;
    
    // Buffer allocations
    VulkanMemoryManager::BufferAllocation voxelVertexBufferAllocation_;
    VulkanMemoryManager::BufferAllocation voxelIndexBufferAllocation_;
    VulkanMemoryManager::BufferAllocation voxelInstanceBufferAllocation_;
    VulkanMemoryManager::BufferAllocation uniformBufferAllocation_;
    
    // Buffer handles for convenience
    VkBuffer voxelVertexBuffer_ = VK_NULL_HANDLE;
    VkBuffer voxelIndexBuffer_ = VK_NULL_HANDLE;
    VkBuffer voxelInstanceBuffer_ = VK_NULL_HANDLE;
    VkBuffer uniformBuffer_ = VK_NULL_HANDLE;
    
    // Descriptor resources
    VkDescriptorSetLayout descriptorSetLayout_ = VK_NULL_HANDLE;
    
    // Instance data
    std::vector<VoxelInstance> voxelInstances_;
    
    // Private methods
    void createVoxelGeometry();
    void createRenderingPipeline();
    void createDescriptorResources();
};

} // namespace VulkanHIP
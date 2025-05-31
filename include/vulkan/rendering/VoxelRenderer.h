#pragma once

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>
#include <vector>
#include <glm/glm.hpp>
#include "VoxelData.h"
#include "VulkanMemoryManager.h"

namespace VulkanHIP {

class VulkanContext;
class VulkanMemoryManager;

struct VoxelInstance {
    glm::vec3 position;
    glm::vec4 color;
    float scale;
};

class VoxelRenderer {
public:
    VoxelRenderer(VulkanContext* context, VulkanMemoryManager* memoryManager);
    ~VoxelRenderer();
    
    VoxelRenderer(const VoxelRenderer&) = delete;
    VoxelRenderer& operator=(const VoxelRenderer&) = delete;
    
    void createVoxelRenderingResources();
    void updateVoxelInstances(const VoxelData& voxelData);
    void renderVoxels(VkCommandBuffer commandBuffer);
    void renderGrid(VkCommandBuffer commandBuffer, const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix);
    void cleanup();
    
private:
    VulkanContext* vulkanContext_;
    VulkanMemoryManager* memoryManager_;
    
    // Buffer allocations
    VulkanMemoryManager::BufferAllocation voxelVertexBufferAllocation_;
    VulkanMemoryManager::BufferAllocation voxelIndexBufferAllocation_;
    VulkanMemoryManager::BufferAllocation voxelInstanceBufferAllocation_;
    
    // Buffer handles for convenience
    VkBuffer voxelVertexBuffer_ = VK_NULL_HANDLE;
    VkBuffer voxelIndexBuffer_ = VK_NULL_HANDLE;
    VkBuffer voxelInstanceBuffer_ = VK_NULL_HANDLE;
    
    std::vector<VoxelInstance> voxelInstances_;
    
    void createVoxelGeometry();
    void updateUniformBuffer(const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix);
};

} // namespace VulkanHIP
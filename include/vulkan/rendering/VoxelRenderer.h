#pragma once

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>
#include <vector>
#include "VoxelData.h"

namespace VulkanHIP {

class VulkanContext;
class VulkanMemoryManager;

struct VoxelInstance {
    glm::vec3 position;
    glm::vec3 color;
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
    void cleanup();
    
private:
    VulkanContext* vulkanContext_;
    VulkanMemoryManager* memoryManager_;
    
    VkBuffer voxelVertexBuffer_ = VK_NULL_HANDLE;
    VmaAllocation voxelVertexBufferAllocation_ = VK_NULL_HANDLE;
    VkBuffer voxelIndexBuffer_ = VK_NULL_HANDLE;
    VmaAllocation voxelIndexBufferAllocation_ = VK_NULL_HANDLE;
    VkBuffer voxelInstanceBuffer_ = VK_NULL_HANDLE;
    VmaAllocation voxelInstanceBufferAllocation_ = VK_NULL_HANDLE;
    
    std::vector<VoxelInstance> voxelInstances_;
    
    void createVoxelGeometry();
};

} // namespace VulkanHIP
#include "VoxelRenderer.h"
#include "VulkanContext.h"
#include "VulkanMemoryManager.h"
#include <stdexcept>
#include <array>

namespace VulkanHIP {

VoxelRenderer::VoxelRenderer(VulkanContext* context, VulkanMemoryManager* memoryManager)
    : vulkanContext_(context), memoryManager_(memoryManager) {
    if (!vulkanContext_ || !memoryManager_) {
        throw std::runtime_error("VoxelRenderer: Invalid dependencies");
    }
}

VoxelRenderer::~VoxelRenderer() {
    cleanup();
}

void VoxelRenderer::createVoxelRenderingResources() {
    createVoxelGeometry();
}

void VoxelRenderer::createVoxelGeometry() {
    // Define cube vertices
    std::array<glm::vec3, 8> vertices = {{
        {-0.5f, -0.5f, -0.5f},
        { 0.5f, -0.5f, -0.5f},
        { 0.5f,  0.5f, -0.5f},
        {-0.5f,  0.5f, -0.5f},
        {-0.5f, -0.5f,  0.5f},
        { 0.5f, -0.5f,  0.5f},
        { 0.5f,  0.5f,  0.5f},
        {-0.5f,  0.5f,  0.5f}
    }};

    // Define cube indices
    std::array<uint16_t, 36> indices = {{
        0, 1, 2, 2, 3, 0,  // front
        4, 5, 6, 6, 7, 4,  // back
        0, 1, 5, 5, 4, 0,  // bottom
        2, 3, 7, 7, 6, 2,  // top
        0, 3, 7, 7, 4, 0,  // left
        1, 2, 6, 6, 5, 1   // right
    }};

    // Create vertex buffer
    VkDeviceSize vertexBufferSize = sizeof(vertices);
    voxelVertexBufferAllocation_ = memoryManager_->createBuffer(
        vertexBufferSize,
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE
    );
    voxelVertexBuffer_ = voxelVertexBufferAllocation_.buffer;

    // Create index buffer
    VkDeviceSize indexBufferSize = sizeof(indices);
    voxelIndexBufferAllocation_ = memoryManager_->createBuffer(
        indexBufferSize,
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE
    );
    voxelIndexBuffer_ = voxelIndexBufferAllocation_.buffer;

    // Copy data to buffers (staging buffer implementation needed)
    // This is a simplified version - full implementation would use staging buffers
}

void VoxelRenderer::updateVoxelInstances(const VoxelData& voxelData) {
    voxelInstances_.clear();
    
    // Convert voxel data to instances
    for (const auto& voxel : voxelData.getActiveVoxels()) {
        VoxelInstance instance;
        instance.position = voxel.position;
        instance.color = voxel.color;
        instance.scale = 1.0f;
        voxelInstances_.push_back(instance);
    }

    // Update instance buffer
    if (!voxelInstances_.empty()) {
        VkDeviceSize bufferSize = sizeof(VoxelInstance) * voxelInstances_.size();
        
        if (voxelInstanceBufferAllocation_) {
            memoryManager_->destroyBuffer(voxelInstanceBufferAllocation_);
        }

        voxelInstanceBufferAllocation_ = memoryManager_->createBuffer(
            bufferSize,
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
            VMA_MEMORY_USAGE_AUTO // Modern pattern for dynamic instance data
        );
        voxelInstanceBuffer_ = voxelInstanceBufferAllocation_.buffer;

        // Copy instance data (simplified - would need proper staging)
    }
}

void VoxelRenderer::renderVoxels(VkCommandBuffer commandBuffer) {
    if (voxelInstances_.empty()) {
        return;
    }

    VkBuffer vertexBuffers[] = {voxelVertexBuffer_, voxelInstanceBuffer_};
    VkDeviceSize offsets[] = {0, 0};
    
    vkCmdBindVertexBuffers(commandBuffer, 0, 2, vertexBuffers, offsets);
    vkCmdBindIndexBuffer(commandBuffer, voxelIndexBuffer_, 0, VK_INDEX_TYPE_UINT16);
    
    vkCmdDrawIndexed(commandBuffer, 36, static_cast<uint32_t>(voxelInstances_.size()), 0, 0, 0);
}

void VoxelRenderer::cleanup() {
    if (vulkanContext_->getDevice() != VK_NULL_HANDLE) {
        if (voxelVertexBufferAllocation_) {
            memoryManager_->destroyBuffer(voxelVertexBufferAllocation_);
            voxelVertexBuffer_ = VK_NULL_HANDLE;
        }
        
        if (voxelIndexBufferAllocation_) {
            memoryManager_->destroyBuffer(voxelIndexBufferAllocation_);
            voxelIndexBuffer_ = VK_NULL_HANDLE;
        }
        
        if (voxelInstanceBufferAllocation_) {
            memoryManager_->destroyBuffer(voxelInstanceBufferAllocation_);
            voxelInstanceBuffer_ = VK_NULL_HANDLE;
        }
    }
}

} // namespace VulkanHIP
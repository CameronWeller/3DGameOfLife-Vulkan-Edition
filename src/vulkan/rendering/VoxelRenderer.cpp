#include "VoxelRenderer.h"
#include "VulkanContext.h"
#include "VulkanMemoryManager.h"
#include <stdexcept>
#include <array>
#include <cstring>

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
    createRenderingPipeline();
    createDescriptorResources();
}

void VoxelRenderer::createVoxelGeometry() {
    // Define cube vertices with normals and UVs for proper 3D rendering
    struct Vertex {
        glm::vec3 position;
        glm::vec3 normal;
        glm::vec2 uv;
    };

    std::array<Vertex, 24> vertices = {{
        // Front face
        {{-0.5f, -0.5f,  0.5f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}},
        {{ 0.5f, -0.5f,  0.5f}, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f}},
        {{ 0.5f,  0.5f,  0.5f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},
        {{-0.5f,  0.5f,  0.5f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}},
        
        // Back face
        {{ 0.5f, -0.5f, -0.5f}, {0.0f, 0.0f, -1.0f}, {0.0f, 0.0f}},
        {{-0.5f, -0.5f, -0.5f}, {0.0f, 0.0f, -1.0f}, {1.0f, 0.0f}},
        {{-0.5f,  0.5f, -0.5f}, {0.0f, 0.0f, -1.0f}, {1.0f, 1.0f}},
        {{ 0.5f,  0.5f, -0.5f}, {0.0f, 0.0f, -1.0f}, {0.0f, 1.0f}},
        
        // Left face
        {{-0.5f, -0.5f, -0.5f}, {-1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
        {{-0.5f, -0.5f,  0.5f}, {-1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}},
        {{-0.5f,  0.5f,  0.5f}, {-1.0f, 0.0f, 0.0f}, {1.0f, 1.0f}},
        {{-0.5f,  0.5f, -0.5f}, {-1.0f, 0.0f, 0.0f}, {0.0f, 1.0f}},
        
        // Right face
        {{ 0.5f, -0.5f,  0.5f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
        {{ 0.5f, -0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}},
        {{ 0.5f,  0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}, {1.0f, 1.0f}},
        {{ 0.5f,  0.5f,  0.5f}, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f}},
        
        // Bottom face
        {{-0.5f, -0.5f, -0.5f}, {0.0f, -1.0f, 0.0f}, {0.0f, 0.0f}},
        {{ 0.5f, -0.5f, -0.5f}, {0.0f, -1.0f, 0.0f}, {1.0f, 0.0f}},
        {{ 0.5f, -0.5f,  0.5f}, {0.0f, -1.0f, 0.0f}, {1.0f, 1.0f}},
        {{-0.5f, -0.5f,  0.5f}, {0.0f, -1.0f, 0.0f}, {0.0f, 1.0f}},
        
        // Top face
        {{-0.5f,  0.5f,  0.5f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}},
        {{ 0.5f,  0.5f,  0.5f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}},
        {{ 0.5f,  0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f}},
        {{-0.5f,  0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f}}
    }};

    // Define cube indices for triangulated faces
    std::array<uint16_t, 36> indices = {{
        0,  1,  2,   2,  3,  0,   // Front face
        4,  5,  6,   6,  7,  4,   // Back face
        8,  9,  10,  10, 11, 8,   // Left face
        12, 13, 14,  14, 15, 12,  // Right face
        16, 17, 18,  18, 19, 16,  // Bottom face
        20, 21, 22,  22, 23, 20   // Top face
    }};

    // Create vertex buffer with staging
    VkDeviceSize vertexBufferSize = sizeof(vertices);
    
    // Create staging buffer for vertex data
    auto vertexStagingBuffer = memoryManager_->createHostVisibleBuffer(
        vertexBufferSize,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT
    );
    
    // Copy vertex data to staging buffer
    void* vertexData = memoryManager_->mapMemory(vertexStagingBuffer);
    memcpy(vertexData, vertices.data(), vertexBufferSize);
    memoryManager_->unmapMemory(vertexStagingBuffer);
    
    // Create device-local vertex buffer
    voxelVertexBufferAllocation_ = memoryManager_->createBuffer(
        vertexBufferSize,
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE
    );
    voxelVertexBuffer_ = voxelVertexBufferAllocation_.buffer;

    // Create index buffer with staging
    VkDeviceSize indexBufferSize = sizeof(indices);
    
    // Create staging buffer for index data
    auto indexStagingBuffer = memoryManager_->createHostVisibleBuffer(
        indexBufferSize,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT
    );
    
    // Copy index data to staging buffer
    void* indexData = memoryManager_->mapMemory(indexStagingBuffer);
    memcpy(indexData, indices.data(), indexBufferSize);
    memoryManager_->unmapMemory(indexStagingBuffer);
    
    // Create device-local index buffer
    voxelIndexBufferAllocation_ = memoryManager_->createBuffer(
        indexBufferSize,
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE
    );
    voxelIndexBuffer_ = voxelIndexBufferAllocation_.buffer;

    // Copy data from staging to device buffers
    VkCommandBuffer commandBuffer = vulkanContext_->beginSingleTimeCommands();
    
    VkBufferCopy vertexCopy{};
    vertexCopy.size = vertexBufferSize;
    vkCmdCopyBuffer(commandBuffer, vertexStagingBuffer.buffer, voxelVertexBuffer_, 1, &vertexCopy);
    
    VkBufferCopy indexCopy{};
    indexCopy.size = indexBufferSize;
    vkCmdCopyBuffer(commandBuffer, indexStagingBuffer.buffer, voxelIndexBuffer_, 1, &indexCopy);
    
    vulkanContext_->endSingleTimeCommands(commandBuffer);
    
    // Clean up staging buffers
    memoryManager_->destroyBuffer(vertexStagingBuffer);
    memoryManager_->destroyBuffer(indexStagingBuffer);
}

void VoxelRenderer::createRenderingPipeline() {
    // This will be implemented in the next step
    // For now, we'll create a placeholder that can be extended
}

void VoxelRenderer::createDescriptorResources() {
    // Create descriptor set layout for uniform buffer (view/projection matrices)
    VkDescriptorSetLayoutBinding uboLayoutBinding{};
    uboLayoutBinding.binding = 0;
    uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uboLayoutBinding.descriptorCount = 1;
    uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    uboLayoutBinding.pImmutableSamplers = nullptr;

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 1;
    layoutInfo.pBindings = &uboLayoutBinding;

    VK_CHECK(vkCreateDescriptorSetLayout(vulkanContext_->getDevice(), &layoutInfo, nullptr, &descriptorSetLayout_));

    // Create uniform buffer for matrices
    VkDeviceSize uniformBufferSize = sizeof(UniformBufferObject);
    uniformBufferAllocation_ = memoryManager_->createHostVisibleBuffer(
        uniformBufferSize,
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT
    );
    uniformBuffer_ = uniformBufferAllocation_.buffer;
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
        
        // Destroy previous instance buffer if it exists
        if (voxelInstanceBufferAllocation_.buffer != VK_NULL_HANDLE) {
            memoryManager_->destroyBuffer(voxelInstanceBufferAllocation_);
        }

        // Create new instance buffer - use host-visible for frequent updates
        voxelInstanceBufferAllocation_ = memoryManager_->createHostVisibleBuffer(
            bufferSize,
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT
        );
        voxelInstanceBuffer_ = voxelInstanceBufferAllocation_.buffer;

        // Copy instance data directly to host-visible buffer
        void* instanceData = memoryManager_->mapMemory(voxelInstanceBufferAllocation_);
        memcpy(instanceData, voxelInstances_.data(), bufferSize);
        memoryManager_->unmapMemory(voxelInstanceBufferAllocation_);
    }
}

void VoxelRenderer::updateFromGrid3D(const class Grid3D& grid) {
    voxelInstances_.clear();
    
    // Extract active voxels from Grid3D
    uint32_t width = grid.getWidth();
    uint32_t height = grid.getHeight();
    uint32_t depth = grid.getDepth();
    
    for (uint32_t z = 0; z < depth; ++z) {
        for (uint32_t y = 0; y < height; ++y) {
            for (uint32_t x = 0; x < width; ++x) {
                if (grid.getCell(x, y, z)) {
                    VoxelInstance instance;
                    instance.position = glm::vec3(x, y, z);
                    
                    // Color based on 3D Game of Life rules - living cells are bright
                    instance.color = glm::vec4(0.2f, 0.8f, 0.3f, 1.0f); // Bright green for alive
                    instance.scale = 0.9f; // Slightly smaller than grid cell for visual separation
                    
                    voxelInstances_.push_back(instance);
                }
            }
        }
    }

    // Update instance buffer with Grid3D data
    if (!voxelInstances_.empty()) {
        VkDeviceSize bufferSize = sizeof(VoxelInstance) * voxelInstances_.size();
        
        // Destroy previous instance buffer if it exists
        if (voxelInstanceBufferAllocation_.buffer != VK_NULL_HANDLE) {
            memoryManager_->destroyBuffer(voxelInstanceBufferAllocation_);
        }

        // Create new instance buffer optimized for frequent updates
        voxelInstanceBufferAllocation_ = memoryManager_->createHostVisibleBuffer(
            bufferSize,
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT
        );
        voxelInstanceBuffer_ = voxelInstanceBufferAllocation_.buffer;

        // Copy instance data
        void* instanceData = memoryManager_->mapMemory(voxelInstanceBufferAllocation_);
        memcpy(instanceData, voxelInstances_.data(), bufferSize);
        memoryManager_->unmapMemory(voxelInstanceBufferAllocation_);
    }
}

void VoxelRenderer::updateUniformBuffer(const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix) {
    UniformBufferObject ubo{};
    ubo.view = viewMatrix;
    ubo.proj = projectionMatrix;
    ubo.model = glm::mat4(1.0f); // Identity for now, can be used for global transformations
    
    // Copy to uniform buffer
    void* uniformData = memoryManager_->mapMemory(uniformBufferAllocation_);
    memcpy(uniformData, &ubo, sizeof(ubo));
    memoryManager_->unmapMemory(uniformBufferAllocation_);
}

void VoxelRenderer::renderVoxels(VkCommandBuffer commandBuffer) {
    if (voxelInstances_.empty()) {
        return;
    }

    // Bind vertex buffers: [0] = geometry vertices, [1] = instance data
    VkBuffer vertexBuffers[] = {voxelVertexBuffer_, voxelInstanceBuffer_};
    VkDeviceSize offsets[] = {0, 0};
    
    vkCmdBindVertexBuffers(commandBuffer, 0, 2, vertexBuffers, offsets);
    vkCmdBindIndexBuffer(commandBuffer, voxelIndexBuffer_, 0, VK_INDEX_TYPE_UINT16);
    
    // Draw instanced voxels (36 indices per cube, instanced by number of active voxels)
    vkCmdDrawIndexed(commandBuffer, 36, static_cast<uint32_t>(voxelInstances_.size()), 0, 0, 0);
}

void VoxelRenderer::renderGrid(VkCommandBuffer commandBuffer, const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix) {
    // Update uniform buffer with current matrices
    updateUniformBuffer(viewMatrix, projectionMatrix);
    
    // Render the voxels
    renderVoxels(commandBuffer);
}

void VoxelRenderer::cleanup() {
    if (vulkanContext_->getDevice() != VK_NULL_HANDLE) {
        // Cleanup descriptor set layout
        if (descriptorSetLayout_ != VK_NULL_HANDLE) {
            vkDestroyDescriptorSetLayout(vulkanContext_->getDevice(), descriptorSetLayout_, nullptr);
            descriptorSetLayout_ = VK_NULL_HANDLE;
        }
        
        // Cleanup buffers
        if (voxelVertexBufferAllocation_.buffer != VK_NULL_HANDLE) {
            memoryManager_->destroyBuffer(voxelVertexBufferAllocation_);
            voxelVertexBuffer_ = VK_NULL_HANDLE;
        }
        
        if (voxelIndexBufferAllocation_.buffer != VK_NULL_HANDLE) {
            memoryManager_->destroyBuffer(voxelIndexBufferAllocation_);
            voxelIndexBuffer_ = VK_NULL_HANDLE;
        }
        
        if (voxelInstanceBufferAllocation_.buffer != VK_NULL_HANDLE) {
            memoryManager_->destroyBuffer(voxelInstanceBufferAllocation_);
            voxelInstanceBuffer_ = VK_NULL_HANDLE;
        }
        
        if (uniformBufferAllocation_.buffer != VK_NULL_HANDLE) {
            memoryManager_->destroyBuffer(uniformBufferAllocation_);
            uniformBuffer_ = VK_NULL_HANDLE;
        }
    }
}

} // namespace VulkanHIP
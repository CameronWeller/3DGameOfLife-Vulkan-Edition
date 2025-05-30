#pragma once

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>
#include <vector>
#include <memory>
#include <glm/glm.hpp>
#include "Vertex.h"

namespace VulkanHIP {

class VulkanContext;
class VulkanMemoryManager;

/**
 * @brief Uniform buffer object for shader transformation matrices
 */
struct UniformBufferObject {
    alignas(16) glm::mat4 model;
    alignas(16) glm::mat4 view;
    alignas(16) glm::mat4 proj;
    alignas(16) glm::vec3 cameraPos;
    alignas(4) float time;
    alignas(4) int renderMode;
    alignas(4) float minLODDistance;
    alignas(4) float maxLODDistance;
};

/**
 * @brief Manages Vulkan buffer creation and operations
 * 
 * This class handles vertex buffers, index buffers, uniform buffers,
 * and buffer copy operations. It abstracts buffer management from
 * the main VulkanEngine class.
 */
class VulkanBufferManager {
public:
    /**
     * @brief Construct a new Vulkan Buffer Manager
     * @param context Vulkan context for device access
     * @param memoryManager Memory manager for allocations
     */
    VulkanBufferManager(VulkanContext* context, VulkanMemoryManager* memoryManager);
    
    /**
     * @brief Destroy the Vulkan Buffer Manager and cleanup resources
     */
    ~VulkanBufferManager();

    // Delete copy constructor and assignment operator
    VulkanBufferManager(const VulkanBufferManager&) = delete;
    VulkanBufferManager& operator=(const VulkanBufferManager&) = delete;

    /**
     * @brief Create vertex buffer from vertex data
     * @param vertices Vector of vertex data
     */
    void createVertexBuffer(const std::vector<Vertex>& vertices);
    
    /**
     * @brief Create index buffer from index data
     * @param indices Vector of index data
     */
    void createIndexBuffer(const std::vector<uint32_t>& indices);
    
    /**
     * @brief Create uniform buffers for shader uniforms
     * @param maxFramesInFlight Number of frames in flight
     */
    void createUniformBuffers(uint32_t maxFramesInFlight);
    
    /**
     * @brief Copy data between buffers
     * @param srcBuffer Source buffer
     * @param dstBuffer Destination buffer
     * @param size Size of data to copy
     */
    void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);
    
    /**
     * @brief Begin a single-time command buffer
     * @return Command buffer ready for recording
     */
    VkCommandBuffer beginSingleTimeCommands();
    
    /**
     * @brief End and submit a single-time command buffer
     * @param commandBuffer Command buffer to submit
     */
    void endSingleTimeCommands(VkCommandBuffer commandBuffer);
    
    // Getters
    VkBuffer getVertexBuffer() const { return vertexBuffer_; }
    VkBuffer getIndexBuffer() const { return indexBuffer_; }
    const std::vector<VkBuffer>& getUniformBuffers() const { return uniformBuffers_; }
    const std::vector<VmaAllocation>& getUniformBufferAllocations() const { return uniformBufferAllocations_; }
    const std::vector<void*>& getUniformBuffersMapped() const { return uniformBuffersMapped_; }
    
    /**
     * @brief Cleanup all managed buffers
     */
    void cleanup();

private:
    VulkanContext* vulkanContext_;
    VulkanMemoryManager* memoryManager_;
    
    // Vertex and index buffers
    VkBuffer vertexBuffer_ = VK_NULL_HANDLE;
    VmaAllocation vertexBufferAllocation_ = VK_NULL_HANDLE;
    VkBuffer indexBuffer_ = VK_NULL_HANDLE;
    VmaAllocation indexBufferAllocation_ = VK_NULL_HANDLE;
    
    // Uniform buffers
    std::vector<VkBuffer> uniformBuffers_;
    std::vector<VmaAllocation> uniformBufferAllocations_;
    std::vector<void*> uniformBuffersMapped_;
    
    /**
     * @brief Create a buffer with specified usage and properties
     * @param size Size of the buffer
     * @param usage Buffer usage flags
     * @param properties Memory property flags
     * @param buffer Output buffer handle
     * @param allocation Output allocation handle
     */
    void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, 
                     VkMemoryPropertyFlags properties, VkBuffer& buffer, 
                     VmaAllocation& allocation);
};

} // namespace VulkanHIP
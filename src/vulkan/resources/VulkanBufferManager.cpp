#include "vulkan/resources/VulkanBufferManager.h"
#include "VulkanContext.h"
#include "VulkanMemoryManager.h"
#include <stdexcept>
#include <cstring>

namespace VulkanHIP {

VulkanBufferManager::VulkanBufferManager(VulkanContext* context, VulkanMemoryManager* memoryManager)
    : vulkanContext_(context), memoryManager_(memoryManager) {
    if (!vulkanContext_ || !memoryManager_) {
        throw std::runtime_error("VulkanBufferManager: Invalid context or memory manager");
    }
}

VulkanBufferManager::~VulkanBufferManager() {
    cleanup();
}

void VulkanBufferManager::createVertexBuffer(const std::vector<Vertex>& vertices) {
    VkDeviceSize bufferSize = sizeof(vertices[0]) * vertices.size();

    // Create staging buffer
    auto stagingBuffer = memoryManager_->createStagingBuffer(bufferSize);
    void* data = memoryManager_->mapStagingBuffer(stagingBuffer);
    memcpy(data, vertices.data(), static_cast<size_t>(bufferSize));
    memoryManager_->unmapStagingBuffer(stagingBuffer);

    // Create vertex buffer
    auto vertexBufferAlloc = memoryManager_->createBuffer(
        bufferSize,
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VMA_MEMORY_USAGE_GPU_ONLY
    );
    vertexBuffer_ = vertexBufferAlloc.buffer;
    vertexBufferAllocation_ = vertexBufferAlloc.allocation;

    // Copy data
    copyBuffer(stagingBuffer.buffer, vertexBuffer_, bufferSize);

    // Destroy staging buffer
    memoryManager_->destroyStagingBuffer(stagingBuffer);
}

void VulkanBufferManager::createIndexBuffer(const std::vector<uint32_t>& indices) {
    VkDeviceSize bufferSize = sizeof(indices[0]) * indices.size();

    // Create staging buffer
    auto stagingBuffer = memoryManager_->createStagingBuffer(bufferSize);
    void* data = memoryManager_->mapStagingBuffer(stagingBuffer);
    memcpy(data, indices.data(), static_cast<size_t>(bufferSize));
    memoryManager_->unmapStagingBuffer(stagingBuffer);

    // Create index buffer
    auto indexBufferAlloc = memoryManager_->createBuffer(
        bufferSize,
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        VMA_MEMORY_USAGE_GPU_ONLY
    );
    indexBuffer_ = indexBufferAlloc.buffer;
    indexBufferAllocation_ = indexBufferAlloc.allocation;

    // Copy data
    copyBuffer(stagingBuffer.buffer, indexBuffer_, bufferSize);

    // Destroy staging buffer
    memoryManager_->destroyStagingBuffer(stagingBuffer);
}

void VulkanBufferManager::createUniformBuffers(uint32_t maxFramesInFlight) {
    VkDeviceSize bufferSize = sizeof(UniformBufferObject);

    uniformBuffers_.resize(maxFramesInFlight);
    uniformBufferAllocations_.resize(maxFramesInFlight);
    uniformBuffersMapped_.resize(maxFramesInFlight);

    for (size_t i = 0; i < maxFramesInFlight; i++) {
        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = bufferSize;
        bufferInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VmaAllocationCreateInfo allocInfo{};
        allocInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
        allocInfo.requiredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        allocInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;

        VmaAllocationInfo allocationInfo;
        if (vmaCreateBuffer(memoryManager_->getAllocator(), &bufferInfo, &allocInfo,
            &uniformBuffers_[i], &uniformBufferAllocations_[i], &allocationInfo) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create uniform buffer!");
        }

        uniformBuffersMapped_[i] = allocationInfo.pMappedData;
    }
}

void VulkanBufferManager::copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size) {
    VkCommandBuffer commandBuffer = beginSingleTimeCommands();

    VkBufferCopy copyRegion{};
    copyRegion.size = size;
    vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

    endSingleTimeCommands(commandBuffer);
}

VkCommandBuffer VulkanBufferManager::beginSingleTimeCommands() {
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = vulkanContext_->getGraphicsCommandPool();
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    vkAllocateCommandBuffers(vulkanContext_->getDevice(), &allocInfo, &commandBuffer);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(commandBuffer, &beginInfo);

    return commandBuffer;
}

void VulkanBufferManager::endSingleTimeCommands(VkCommandBuffer commandBuffer) {
    vkEndCommandBuffer(commandBuffer);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    vkQueueSubmit(vulkanContext_->getGraphicsQueue(), 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(vulkanContext_->getGraphicsQueue());

    vkFreeCommandBuffers(vulkanContext_->getDevice(), vulkanContext_->getGraphicsCommandPool(), 1, &commandBuffer);
}

void VulkanBufferManager::cleanup() {
    if (!memoryManager_) return;

    // Cleanup uniform buffers
    for (size_t i = 0; i < uniformBuffers_.size(); i++) {
        if (uniformBuffers_[i] != VK_NULL_HANDLE) {
            vmaDestroyBuffer(memoryManager_->getAllocator(), uniformBuffers_[i], uniformBufferAllocations_[i]);
            uniformBuffers_[i] = VK_NULL_HANDLE;
            uniformBufferAllocations_[i] = VK_NULL_HANDLE;
            uniformBuffersMapped_[i] = nullptr;
        }
    }
    uniformBuffers_.clear();
    uniformBufferAllocations_.clear();
    uniformBuffersMapped_.clear();

    // Cleanup vertex buffer
    if (vertexBuffer_ != VK_NULL_HANDLE) {
        vmaDestroyBuffer(memoryManager_->getAllocator(), vertexBuffer_, vertexBufferAllocation_);
        vertexBuffer_ = VK_NULL_HANDLE;
        vertexBufferAllocation_ = VK_NULL_HANDLE;
    }

    // Cleanup index buffer
    if (indexBuffer_ != VK_NULL_HANDLE) {
        vmaDestroyBuffer(memoryManager_->getAllocator(), indexBuffer_, indexBufferAllocation_);
        indexBuffer_ = VK_NULL_HANDLE;
        indexBufferAllocation_ = VK_NULL_HANDLE;
    }
}

void VulkanBufferManager::createBuffer(VkDeviceSize size, VkBufferUsageFlags usage,
                                     VkMemoryPropertyFlags properties, VkBuffer& buffer,
                                     VmaAllocation& allocation) {
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo allocInfo{};
    if (properties & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) {
        allocInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
    } else {
        allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    }
    allocInfo.requiredFlags = properties;

    if (vmaCreateBuffer(memoryManager_->getAllocator(), &bufferInfo, &allocInfo,
        &buffer, &allocation, nullptr) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create buffer!");
    }
}

} // namespace VulkanHIP
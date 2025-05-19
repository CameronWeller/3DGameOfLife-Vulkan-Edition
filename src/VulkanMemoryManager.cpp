#include "VulkanMemoryManager.h"
#include <stdexcept>
#include <algorithm>

VulkanMemoryManager::VulkanMemoryManager(VkDevice device, VkPhysicalDevice physicalDevice)
    : device_(device), physicalDevice_(physicalDevice), maxStagingSize_(0) {
    if (device_ == VK_NULL_HANDLE) {
        throw std::runtime_error("VulkanMemoryManager: Device cannot be null");
    }
    if (physicalDevice_ == VK_NULL_HANDLE) {
        throw std::runtime_error("VulkanMemoryManager: Physical device cannot be null");
    }
    createAllocator();
}

VulkanMemoryManager::~VulkanMemoryManager() {
    // Free all buffers
    for (const auto& allocation : bufferPool_) {
        if (allocation.buffer != VK_NULL_HANDLE) {
            vmaDestroyBuffer(allocator_, allocation.buffer, allocation.allocation);
        }
    }
    bufferPool_.clear();

    // Free all staging buffers
    for (const auto& staging : stagingPool_) {
        if (staging.buffer != VK_NULL_HANDLE) {
            vmaDestroyBuffer(allocator_, staging.buffer, staging.allocation);
        }
    }
    stagingPool_.clear();

    destroyAllocator();
}

void VulkanMemoryManager::createAllocator() {
    VmaAllocatorCreateInfo allocatorInfo = {};
    allocatorInfo.vulkanApiVersion = VK_API_VERSION_1_2;
    allocatorInfo.physicalDevice = physicalDevice_;
    allocatorInfo.device = device_;
    allocatorInfo.instance = VK_NULL_HANDLE; // Will be set by VulkanContext

    if (vmaCreateAllocator(&allocatorInfo, &allocator_) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create VMA allocator");
    }
}

void VulkanMemoryManager::destroyAllocator() {
    if (allocator_ != VK_NULL_HANDLE) {
        vmaDestroyAllocator(allocator_);
        allocator_ = VK_NULL_HANDLE;
    }
}

VulkanMemoryManager::BufferAllocation VulkanMemoryManager::allocateBuffer(
    VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties) {
    
    VkBufferCreateInfo bufferInfo = {};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo allocInfo = {};
    allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
    allocInfo.requiredFlags = properties;

    VkBuffer buffer;
    VmaAllocation allocation;
    if (vmaCreateBuffer(allocator_, &bufferInfo, &allocInfo, &buffer, &allocation, nullptr) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create buffer with VMA");
    }

    BufferAllocation result{buffer, allocation, size, true};
    bufferPool_.push_back(result);
    return result;
}

void VulkanMemoryManager::freeBuffer(const BufferAllocation& allocation) {
    auto it = std::find_if(bufferPool_.begin(), bufferPool_.end(),
        [&](const BufferAllocation& a) {
            return a.buffer == allocation.buffer && a.allocation == allocation.allocation;
        });

    if (it != bufferPool_.end()) {
        vmaDestroyBuffer(allocator_, it->buffer, it->allocation);
        bufferPool_.erase(it);
    }
}

VulkanMemoryManager::StagingBuffer VulkanMemoryManager::getStagingBuffer(VkDeviceSize size) {
    // Try to find an existing staging buffer
    for (auto& staging : stagingPool_) {
        if (!staging.inUse && staging.size >= size) {
            staging.inUse = true;
            return staging;
        }
    }

    // Create new staging buffer if none found
    VkBufferCreateInfo bufferInfo = {};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo allocInfo = {};
    allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
    allocInfo.requiredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

    VkBuffer buffer;
    VmaAllocation allocation;
    if (vmaCreateBuffer(allocator_, &bufferInfo, &allocInfo, &buffer, &allocation, nullptr) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create staging buffer with VMA");
    }

    StagingBuffer staging{buffer, allocation, size, true};
    stagingPool_.push_back(staging);
    maxStagingSize_ = std::max(maxStagingSize_, size);
    return staging;
}

void VulkanMemoryManager::returnStagingBuffer(const StagingBuffer& staging) {
    auto it = std::find_if(stagingPool_.begin(), stagingPool_.end(),
        [&](const StagingBuffer& s) {
            return s.buffer == staging.buffer && s.allocation == staging.allocation;
        });

    if (it != stagingPool_.end()) {
        it->inUse = false;
    }
}

uint32_t VulkanMemoryManager::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) const {
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice_, &memProperties);

    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) && 
            (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }

    throw std::runtime_error("Failed to find suitable memory type!");
} 
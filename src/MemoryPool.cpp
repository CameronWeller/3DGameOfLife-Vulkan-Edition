#include <algorithm>
#include <stdexcept>
#include <iostream>
#include "VulkanEngine.h"

// MemoryPool implementation
MemoryPool::MemoryPool(VkDevice device) : device(device), maxStagingSize(0) {
    if (device == VK_NULL_HANDLE) {
        throw std::runtime_error("Cannot create memory pool with null device");
    }
}

MemoryPool::~MemoryPool() {
    try {
        // Free all buffers in the pool
        for (const auto& allocation : bufferPool) {
            if (allocation.buffer != VK_NULL_HANDLE) {
                vkDestroyBuffer(device, allocation.buffer, nullptr);
            }
            if (allocation.memory != VK_NULL_HANDLE) {
                vkFreeMemory(device, allocation.memory, nullptr);
            }
        }
        bufferPool.clear();

        // Free all staging buffers
        for (const auto& staging : stagingPool) {
            if (staging.buffer != VK_NULL_HANDLE) {
                vkDestroyBuffer(device, staging.buffer, nullptr);
            }
            if (staging.memory != VK_NULL_HANDLE) {
                vkFreeMemory(device, staging.memory, nullptr);
            }
        }
        stagingPool.clear();
        
        std::cout << "MemoryPool cleanup completed successfully" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Error during MemoryPool cleanup: " << e.what() << std::endl;
    }
}

MemoryPool::BufferAllocation MemoryPool::allocateBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties) {
    if (size == 0) {
        throw std::runtime_error("Cannot allocate buffer of size 0");
    }

    // Try to find an existing buffer that matches requirements
    for (auto& allocation : bufferPool) {
        if (!allocation.inUse && allocation.size >= size && 
            allocation.usage == usage && allocation.properties == properties) {
            allocation.inUse = true;
            return allocation;
        }
    }

    // Create new buffer if no suitable one found
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VkBuffer buffer = VK_NULL_HANDLE;
    VkResult result = vkCreateBuffer(device, &bufferInfo, nullptr, &buffer);
    if (result != VK_SUCCESS) {
        throw std::runtime_error("Failed to create buffer in memory pool: " + std::to_string(result));
    }

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(device, buffer, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);

    VkDeviceMemory memory = VK_NULL_HANDLE;
    result = vkAllocateMemory(device, &allocInfo, nullptr, &memory);
    if (result != VK_SUCCESS) {
        vkDestroyBuffer(device, buffer, nullptr);
        throw std::runtime_error("Failed to allocate memory in memory pool: " + std::to_string(result));
    }

    result = vkBindBufferMemory(device, buffer, memory, 0);
    if (result != VK_SUCCESS) {
        vkDestroyBuffer(device, buffer, nullptr);
        vkFreeMemory(device, memory, nullptr);
        throw std::runtime_error("Failed to bind buffer memory in memory pool: " + std::to_string(result));
    }

    BufferAllocation allocation{buffer, memory, size, usage, properties, true};
    bufferPool.push_back(allocation);
    
    std::cout << "Allocated buffer of size " << size << " bytes (total buffers: " << bufferPool.size() << ")" << std::endl;
    return allocation;
}

void MemoryPool::freeBuffer(const BufferAllocation& allocation) {
    if (allocation.buffer == VK_NULL_HANDLE) {
        return; // Nothing to free
    }

    auto it = std::find_if(bufferPool.begin(), bufferPool.end(),
        [&](const BufferAllocation& a) {
            return a.buffer == allocation.buffer && a.memory == allocation.memory;
        });
    
    if (it != bufferPool.end()) {
        it->inUse = false;
        std::cout << "Returned buffer to pool (size: " << it->size << " bytes)" << std::endl;
    }
}

MemoryPool::StagingBuffer MemoryPool::getStagingBuffer(VkDeviceSize size) {
    if (size == 0) {
        throw std::runtime_error("Cannot get staging buffer of size 0");
    }

    // Try to find an existing staging buffer
    for (auto& staging : stagingPool) {
        if (!staging.inUse && staging.size >= size) {
            staging.inUse = true;
            return staging;
        }
    }

    // Create new staging buffer if needed
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VkBuffer buffer = VK_NULL_HANDLE;
    VkResult result = vkCreateBuffer(device, &bufferInfo, nullptr, &buffer);
    if (result != VK_SUCCESS) {
        throw std::runtime_error("Failed to create staging buffer: " + std::to_string(result));
    }

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(device, buffer, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(
        memRequirements.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    );

    VkDeviceMemory memory = VK_NULL_HANDLE;
    result = vkAllocateMemory(device, &allocInfo, nullptr, &memory);
    if (result != VK_SUCCESS) {
        vkDestroyBuffer(device, buffer, nullptr);
        throw std::runtime_error("Failed to allocate staging memory: " + std::to_string(result));
    }

    result = vkBindBufferMemory(device, buffer, memory, 0);
    if (result != VK_SUCCESS) {
        vkDestroyBuffer(device, buffer, nullptr);
        vkFreeMemory(device, memory, nullptr);
        throw std::runtime_error("Failed to bind staging buffer memory: " + std::to_string(result));
    }

    StagingBuffer staging{buffer, memory, size, true};
    stagingPool.push_back(staging);
    maxStagingSize = std::max(maxStagingSize, size);
    
    std::cout << "Created staging buffer of size " << size << " bytes (total staging buffers: " << stagingPool.size() << ")" << std::endl;
    return staging;
}

void MemoryPool::returnStagingBuffer(const StagingBuffer& buffer) {
    if (buffer.buffer == VK_NULL_HANDLE) {
        return; // Nothing to return
    }
    
    auto it = std::find_if(stagingPool.begin(), stagingPool.end(),
        [&](const StagingBuffer& s) {
            return s.buffer == buffer.buffer && s.memory == buffer.memory;
        });
    
    if (it != stagingPool.end()) {
        it->inUse = false;
        std::cout << "Returned staging buffer to pool (size: " << it->size << " bytes)" << std::endl;
    }
}

uint32_t MemoryPool::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) {
    if (device == VK_NULL_HANDLE) {
        throw std::runtime_error("Cannot find memory type with null device");
    }

    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(VulkanEngine::getPhysicalDevice(), &memProperties);

    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) && 
            (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }

    throw std::runtime_error("Failed to find suitable memory type");
} 
#include <algorithm>
#include <stdexcept>
#include <iostream>

// MemoryPool implementation
MemoryPool::MemoryPool(VkDevice device, VkPhysicalDevice physicalDevice)
    : device_(device), physicalDevice_(physicalDevice), maxStagingSize_(0) {
    if (device_ == VK_NULL_HANDLE) {
        throw std::runtime_error("MemoryPool: Vulkan device cannot be null.");
    }
    if (physicalDevice_ == VK_NULL_HANDLE) {
        throw std::runtime_error("MemoryPool: Vulkan physical device cannot be null.");
    }
}

MemoryPool::~MemoryPool() {
    try {
        // Free all buffers in the pool
        for (const auto& allocation : bufferPool_) {
            if (allocation.buffer != VK_NULL_HANDLE) {
                vkDestroyBuffer(device_, allocation.buffer, nullptr);
            }
            if (allocation.memory != VK_NULL_HANDLE) {
                vkFreeMemory(device_, allocation.memory, nullptr);
            }
        }
        bufferPool_.clear();

        // Free all staging buffers
        for (const auto& staging : stagingPool_) {
            if (staging.buffer != VK_NULL_HANDLE) {
                vkDestroyBuffer(device_, staging.buffer, nullptr);
            }
            if (staging.memory != VK_NULL_HANDLE) {
                vkFreeMemory(device_, staging.memory, nullptr);
            }
        }
        stagingPool_.clear();
        
        std::cout << "MemoryPool cleanup completed successfully" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Error during MemoryPool cleanup: " << e.what() << std::endl;
    }
}

MemoryPool::BufferAllocation MemoryPool::allocateBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties) {
    if (size == 0) {
        throw std::runtime_error("MemoryPool: Cannot allocate buffer of size 0.");
    }

    // Try to find an existing buffer that matches requirements
    for (auto& allocation : bufferPool_) {
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

    VkBuffer buffer;
    if (vkCreateBuffer(device_, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
        throw std::runtime_error("MemoryPool: Failed to create buffer!");
    }

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(device_, buffer, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);

    VkDeviceMemory memory;
    if (vkAllocateMemory(device_, &allocInfo, nullptr, &memory) != VK_SUCCESS) {
        vkDestroyBuffer(device_, buffer, nullptr);
        throw std::runtime_error("MemoryPool: Failed to allocate buffer memory!");
    }

    if (vkBindBufferMemory(device_, buffer, memory, 0) != VK_SUCCESS) {
        vkDestroyBuffer(device_, buffer, nullptr);
        vkFreeMemory(device_, memory, nullptr);
        throw std::runtime_error("MemoryPool: Failed to bind buffer memory!");
    }

    BufferAllocation newAllocation{buffer, memory, size, usage, properties, true};
    bufferPool_.push_back(newAllocation);
    
    std::cout << "Allocated buffer of size " << size << " bytes (total buffers: " << bufferPool_.size() << ")" << std::endl;
    return newAllocation;
}

void MemoryPool::freeBuffer(const BufferAllocation& allocation) {
    if (allocation.buffer == VK_NULL_HANDLE) {
        return; // Nothing to free
    }

    auto it = std::find_if(bufferPool_.begin(), bufferPool_.end(),
        [&](const BufferAllocation& a) {
            return a.buffer == allocation.buffer && a.memory == allocation.memory;
        });
    
    if (it != bufferPool_.end()) {
        it->inUse = false;
        std::cout << "Returned buffer to pool (size: " << it->size << " bytes)" << std::endl;
    } else {
        // This could indicate an attempt to free a buffer not from this pool, or already freed and removed.
        // Or, the buffer was created outside the pool and passed here by mistake.
        // For now, just log or ignore. In a robust system, this might be an error.
        // std::cerr << "Warning: Attempted to free a buffer not managed by this pool or already freed." << std::endl;
    }
}

MemoryPool::StagingBuffer MemoryPool::getStagingBuffer(VkDeviceSize size) {
    if (size == 0) {
        throw std::runtime_error("MemoryPool: Cannot get staging buffer of size 0.");
    }

    // Try to find an existing staging buffer
    for (auto& staging : stagingPool_) {
        if (!staging.inUse && staging.size >= size) {
            staging.inUse = true;
            return staging;
        }
    }

    // Create new staging buffer if needed
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT; // Staging can be src or dst
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VkBuffer buffer;
    if (vkCreateBuffer(device_, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
        throw std::runtime_error("MemoryPool: Failed to create staging buffer!");
    }

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(device_, buffer, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(
        memRequirements.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    );

    VkDeviceMemory memory;
    if (vkAllocateMemory(device_, &allocInfo, nullptr, &memory) != VK_SUCCESS) {
        vkDestroyBuffer(device_, buffer, nullptr);
        throw std::runtime_error("MemoryPool: Failed to allocate staging memory!");
    }

    if (vkBindBufferMemory(device_, buffer, memory, 0) != VK_SUCCESS) {
        vkDestroyBuffer(device_, buffer, nullptr);
        vkFreeMemory(device_, memory, nullptr);
        throw std::runtime_error("MemoryPool: Failed to bind staging memory!");
    }

    StagingBuffer newStaging{buffer, memory, size, true};
    stagingPool_.push_back(newStaging);
    maxStagingSize_ = std::max(maxStagingSize_, size);
    
    std::cout << "Created staging buffer of size " << size << " bytes (total staging buffers: " << stagingPool_.size() << ")" << std::endl;
    return newStaging;
}

void MemoryPool::returnStagingBuffer(const StagingBuffer& buffer) {
    if (buffer.buffer == VK_NULL_HANDLE) {
        return; // Nothing to return
    }
    
    auto it = std::find_if(stagingPool_.begin(), stagingPool_.end(),
        [&](const StagingBuffer& s) {
            return s.buffer == buffer.buffer && s.memory == buffer.memory;
        });
    
    if (it != stagingPool_.end()) {
        it->inUse = false;
        std::cout << "Returned staging buffer to pool (size: " << it->size << " bytes)" << std::endl;
    } else {
        // std::cerr << "Warning: Attempted to return a staging buffer not managed by this pool." << std::endl;
    }
}

uint32_t MemoryPool::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice_, &memProperties);
    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    throw std::runtime_error("MemoryPool: Failed to find suitable memory type!");
} 
#include "VulkanMemoryManager.h"
#include "VulkanContext.h"
#include <stdexcept>
#include <algorithm>
#include <cstring>
#include <cstdio>

namespace VulkanHIP {

VulkanMemoryManager::VulkanMemoryManager(VkDevice device, VkPhysicalDevice physicalDevice)
    : device_(device), physicalDevice_(physicalDevice), allocator_(VK_NULL_HANDLE), 
      commandPool_(VK_NULL_HANDLE), graphicsQueue_(VK_NULL_HANDLE), maxStagingSize_(0) {
    
    // Get the graphics queue
    vkGetDeviceQueue(device_, VulkanContext::getInstance().getQueueFamilyIndices().graphicsFamily.value(), 0, &graphicsQueue_);
    if (graphicsQueue_ == VK_NULL_HANDLE) {
        throw std::runtime_error("Failed to get graphics queue!");
    }

    // Create command pool
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = VulkanContext::getInstance().getQueueFamilyIndices().graphicsFamily.value();

    if (vkCreateCommandPool(device_, &poolInfo, nullptr, &commandPool_) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create command pool!");
    }

    createAllocator();
}

VulkanMemoryManager::~VulkanMemoryManager() {
    cleanup();
}

void VulkanMemoryManager::cleanup() {
    // Clean up staging buffers first
    cleanupStagingBuffers();

    // Clean up instance pools
    cleanupInstancePools();

    // Clean up memory pools
    cleanupMemoryPools();

    // Clean up streaming buffers
    cleanupStreamingBuffers();

    // Clean up timeline semaphores
    cleanupTimelineSemaphores();

    // Clean up buffer pool
    for (const auto& buffer : bufferPool_) {
        if (buffer.buffer != VK_NULL_HANDLE && buffer.allocation != nullptr) {
            vmaDestroyBuffer(allocator_, buffer.buffer, buffer.allocation);
        }
    }
    bufferPool_.clear();

    // Clean up image views
    for (const auto& view : imageViews_) {
        if (view.view != VK_NULL_HANDLE) {
            vkDestroyImageView(device_, view.view, nullptr);
        }
    }
    imageViews_.clear();

    // Clean up command pool
    if (commandPool_ != VK_NULL_HANDLE) {
        vkDestroyCommandPool(device_, commandPool_, nullptr);
        commandPool_ = VK_NULL_HANDLE;
    }

    // Clean up allocator last
    destroyAllocator();
}

VulkanMemoryManager::BufferAllocation VulkanMemoryManager::createBuffer(
    VkDeviceSize size, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage) {
    
    BufferAllocation allocation{};
    allocation.size = size;
    allocation.mappedData = nullptr;

    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo allocInfo{};
    
    // Use modern VMA patterns instead of deprecated usage flags
    allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
    allocInfo.flags = 0;
    
    // Determine allocation flags based on buffer usage with advanced VMA 3.3.0 patterns
    if (usage & (VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT)) {
        if (usage & VK_BUFFER_USAGE_TRANSFER_DST_BIT) {
            // Dynamic buffer that needs CPU->GPU transfers
            allocInfo.flags |= VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
            allocInfo.flags |= VMA_ALLOCATION_CREATE_HOST_ACCESS_ALLOW_TRANSFER_INSTEAD_BIT;
            
            // Use MIN_TIME strategy for frequently updated buffers
            allocInfo.flags |= VMA_ALLOCATION_CREATE_STRATEGY_MIN_TIME_BIT;
        } else {
            // Static buffer - prefer device local memory with MIN_MEMORY strategy
            allocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
            allocInfo.flags |= VMA_ALLOCATION_CREATE_STRATEGY_MIN_MEMORY_BIT;
        }
    } else if (usage & VK_BUFFER_USAGE_TRANSFER_SRC_BIT) {
        // Staging buffer - optimize for sequential host writes
        allocInfo.flags |= VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
        allocInfo.flags |= VMA_ALLOCATION_CREATE_MAPPED_BIT;
        allocInfo.flags |= VMA_ALLOCATION_CREATE_STRATEGY_MIN_TIME_BIT; // Fast allocation for temporary buffers
        allocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST;
    } else if (usage & VK_BUFFER_USAGE_TRANSFER_DST_BIT) {
        // Readback buffer - optimize for random host access
        allocInfo.flags |= VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT;
        allocInfo.flags |= VMA_ALLOCATION_CREATE_MAPPED_BIT;
        allocInfo.flags |= VMA_ALLOCATION_CREATE_STRATEGY_MIN_TIME_BIT;
        allocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST;
    } else if (usage & (VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT)) {
        // Compute storage buffers - prefer device local with dedicated memory for large buffers
        allocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
        allocInfo.flags |= VMA_ALLOCATION_CREATE_STRATEGY_MIN_MEMORY_BIT;
        
        if (size > 64 * 1024 * 1024) { // 64MB threshold for dedicated memory
            allocInfo.flags |= VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
        }
    }
    
    // Add budget-aware allocation for large buffers (requires VK_EXT_memory_budget)
    if (size > 32 * 1024 * 1024) { // 32MB threshold
        allocInfo.flags |= VMA_ALLOCATION_CREATE_WITHIN_BUDGET_BIT;
        allocInfo.priority = 1.0f; // High priority for large allocations
    } else if (size > 4 * 1024 * 1024) { // 4MB threshold
        allocInfo.priority = 0.75f; // Medium-high priority
    } else if (usage & VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT) {
        allocInfo.priority = 0.9f; // High priority for uniform buffers (frequently accessed)
    } else {
        allocInfo.priority = 0.5f; // Default priority
    }
    
    // For render target or frequently accessed buffers, prefer dedicated memory
    if (usage & (VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT)) {
        allocInfo.flags |= VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
        allocInfo.priority = 1.0f;
    }

    if (vmaCreateBuffer(allocator_, &bufferInfo, &allocInfo, 
                       &allocation.buffer, &allocation.allocation, 
                       &allocation.allocationInfo) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create buffer!");
    }

    bufferPool_.push_back(allocation);
    return allocation;
}

void VulkanMemoryManager::destroyBuffer(const BufferAllocation& allocation) {
    if (allocation.allocation) {
        vmaDestroyBuffer(allocator_, allocation.buffer, allocation.allocation);
        // Remove from buffer pool
        auto it = std::find(bufferPool_.begin(), bufferPool_.end(), allocation);
        if (it != bufferPool_.end()) {
            bufferPool_.erase(it);
        }
    }
}

void* VulkanMemoryManager::mapMemory(const BufferAllocation& allocation) {
    void* data;
    if (vmaMapMemory(allocator_, allocation.allocation, &data) != VK_SUCCESS) {
        throw std::runtime_error("Failed to map buffer memory!");
    }
    return data;
}

void VulkanMemoryManager::unmapMemory(const BufferAllocation& allocation) {
    vmaUnmapMemory(allocator_, allocation.allocation);
}

VulkanMemoryManager::StagingBuffer VulkanMemoryManager::createStagingBuffer(VkDeviceSize size) {
    StagingBuffer staging{};
    staging.size = size;

    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo allocInfo{};
    // Use modern VMA patterns for staging buffers
    allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
    allocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | 
                      VMA_ALLOCATION_CREATE_MAPPED_BIT;

    VmaAllocationInfo allocationInfo;
    if (vmaCreateBuffer(allocator_, &bufferInfo, &allocInfo,
                       &staging.buffer, &staging.allocation, &allocationInfo) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create staging buffer!");
    }

    staging.allocationInfo = allocationInfo;
    staging.mappedData = allocationInfo.pMappedData;
    
    return staging;
}

void VulkanMemoryManager::destroyStagingBuffer(StagingBuffer& stagingBuffer) {
    if (stagingBuffer.allocation) {
        vmaDestroyBuffer(allocator_, stagingBuffer.buffer, stagingBuffer.allocation);
        stagingBuffer.buffer = VK_NULL_HANDLE;
        stagingBuffer.allocation = VK_NULL_HANDLE;
        stagingBuffer.mappedData = nullptr;
    }
}

void* VulkanMemoryManager::mapStagingBuffer(StagingBuffer& stagingBuffer) {
    return stagingBuffer.mappedData;  // Already mapped
}

void VulkanMemoryManager::unmapStagingBuffer(StagingBuffer& stagingBuffer) {
    // No-op since we use persistently mapped buffers
}

void VulkanMemoryManager::createAllocator() {
    VmaAllocatorCreateInfo allocatorInfo{};
    allocatorInfo.vulkanApiVersion = VK_API_VERSION_1_2;
    allocatorInfo.physicalDevice = physicalDevice_;
    allocatorInfo.device = device_;
    allocatorInfo.instance = VulkanContext::getInstance().getVkInstance();
    
    // Enable useful VMA features
    allocatorInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT |
                          VMA_ALLOCATOR_CREATE_EXT_MEMORY_BUDGET_BIT |
                          VMA_ALLOCATOR_CREATE_EXT_MEMORY_PRIORITY_BIT;

    if (vmaCreateAllocator(&allocatorInfo, &allocator_) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create Vulkan Memory Allocator!");
    }
}

void VulkanMemoryManager::destroyAllocator() {
    if (allocator_) {
        vmaDestroyAllocator(allocator_);
        allocator_ = VK_NULL_HANDLE;
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

void VulkanMemoryManager::createStagingPool(VkDeviceSize size) {
    std::lock_guard<std::mutex> lock(stagingMutex_);
    maxStagingSize_ = size;
    // Pre-allocate staging buffers if needed
}

void VulkanMemoryManager::destroyStagingPool() {
    std::lock_guard<std::mutex> lock(stagingMutex_);
    cleanupStagingBuffers();
    maxStagingSize_ = 0;
}

void VulkanMemoryManager::cleanupStagingBuffers() {
    for (auto& staging : stagingPool_) {
        destroyStagingBuffer(staging);
    }
    stagingPool_.clear();
}

VulkanMemoryManager::StagingBuffer VulkanMemoryManager::allocateFromStagingPool(VkDeviceSize size) {
    std::lock_guard<std::mutex> lock(stagingMutex_);
    
    // Try to find an existing buffer that's large enough
    for (auto& staging : stagingPool_) {
        if (!staging.inUse && staging.size >= size) {
            staging.inUse = true;
            return staging;
        }
    }
    
    // If no suitable buffer found, create a new one
    StagingBuffer newBuffer = createStagingBuffer(size);
    newBuffer.inUse = true;
    stagingPool_.push_back(newBuffer);
    return newBuffer;
}

void VulkanMemoryManager::freeStagingBuffer(StagingBuffer& stagingBuffer) {
    std::lock_guard<std::mutex> lock(stagingMutex_);
    
    // Find the buffer in the pool
    auto it = std::find_if(stagingPool_.begin(), stagingPool_.end(),
        [&stagingBuffer](const StagingBuffer& buffer) {
            return buffer.buffer == stagingBuffer.buffer;
        });
    
    if (it != stagingPool_.end()) {
        it->inUse = false;
        // Optionally destroy if pool is too large
        if (stagingPool_.size() > 10) {  // Arbitrary limit
            destroyStagingBuffer(*it);
            stagingPool_.erase(it);
        }
    } else {
        // If not found in pool, just destroy it
        destroyStagingBuffer(stagingBuffer);
    }
}

VulkanMemoryManager::ImageAllocation VulkanMemoryManager::allocateImage(
    uint32_t width, uint32_t height, VkFormat format, 
    VkImageTiling tiling, VkImageUsageFlags usage, 
    VkMemoryPropertyFlags properties) {
    
    ImageAllocation allocation{};
    
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = width;
    imageInfo.extent.height = height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = format;
    imageInfo.tiling = tiling;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = usage;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo allocInfo{};
    // Use modern VMA patterns for images
    allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
    allocInfo.flags = 0;
    
    // Calculate approximate image size for allocation decisions
    uint32_t bytesPerPixel = 4; // Default assumption
    switch (format) {
        case VK_FORMAT_R8G8B8A8_UNORM:
        case VK_FORMAT_B8G8R8A8_UNORM:
        case VK_FORMAT_R8G8B8A8_SRGB:
            bytesPerPixel = 4;
            break;
        case VK_FORMAT_R16G16B16A16_SFLOAT:
            bytesPerPixel = 8;
            break;
        case VK_FORMAT_R32G32B32A32_SFLOAT:
            bytesPerPixel = 16;
            break;
        case VK_FORMAT_D32_SFLOAT:
            bytesPerPixel = 4;
            break;
        case VK_FORMAT_D24_UNORM_S8_UINT:
            bytesPerPixel = 4;
            break;
        default:
            bytesPerPixel = 4; // Conservative estimate
            break;
    }
    
    VkDeviceSize imageSize = static_cast<VkDeviceSize>(width) * height * bytesPerPixel;
    
    // For large images or render targets, use dedicated memory
    if (imageSize > 16 * 1024 * 1024 || // 16MB threshold
        (usage & (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT))) {
        allocInfo.flags |= VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
        allocInfo.priority = 1.0f;
    }

    if (vmaCreateImage(allocator_, &imageInfo, &allocInfo,
                      &allocation.image, &allocation.allocation, nullptr) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create image!");
    }

    allocation.size = imageSize;
    allocation.inUse = true;
    
    return allocation;
}

void VulkanMemoryManager::freeImage(const ImageAllocation& allocation) {
    if (allocation.allocation) {
        // First destroy any associated image views
        for (auto& view : imageViews_) {
            if (view.image == allocation.image) {
                vkDestroyImageView(device_, view.view, nullptr);
                view.view = VK_NULL_HANDLE;
            }
        }
        // Remove destroyed views
        imageViews_.erase(
            std::remove_if(imageViews_.begin(), imageViews_.end(),
                [](const ImageViewInfo& view) { return view.view == VK_NULL_HANDLE; }),
            imageViews_.end()
        );
        
        // Then destroy the image
        vmaDestroyImage(allocator_, allocation.image, allocation.allocation);
    }
}

VkCommandBuffer VulkanMemoryManager::beginSingleTimeCommands() {
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = commandPool_;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    VkResult result = vkAllocateCommandBuffers(device_, &allocInfo, &commandBuffer);
    if (result != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate command buffer: " + std::to_string(result));
    }

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    result = vkBeginCommandBuffer(commandBuffer, &beginInfo);
    if (result != VK_SUCCESS) {
        vkFreeCommandBuffers(device_, commandPool_, 1, &commandBuffer);
        throw std::runtime_error("Failed to begin command buffer: " + std::to_string(result));
    }

    return commandBuffer;
}

void VulkanMemoryManager::endSingleTimeCommands(VkCommandBuffer commandBuffer) {
    if (commandBuffer == VK_NULL_HANDLE) {
        return;
    }

    VkResult result = vkEndCommandBuffer(commandBuffer);
    if (result != VK_SUCCESS) {
        vkFreeCommandBuffers(device_, commandPool_, 1, &commandBuffer);
        throw std::runtime_error("Failed to end command buffer: " + std::to_string(result));
    }

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    result = vkQueueSubmit(graphicsQueue_, 1, &submitInfo, VK_NULL_HANDLE);
    if (result != VK_SUCCESS) {
        vkFreeCommandBuffers(device_, commandPool_, 1, &commandBuffer);
        throw std::runtime_error("Failed to submit command buffer: " + std::to_string(result));
    }

    result = vkQueueWaitIdle(graphicsQueue_);
    if (result != VK_SUCCESS) {
        vkFreeCommandBuffers(device_, commandPool_, 1, &commandBuffer);
        throw std::runtime_error("Failed to wait for queue idle: " + std::to_string(result));
    }

    vkFreeCommandBuffers(device_, commandPool_, 1, &commandBuffer);
}

void VulkanMemoryManager::createImageView(VkImage image, VkFormat format, 
                                         VkImageAspectFlags aspectFlags, VkImageView& imageView) {
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = format;
    viewInfo.subresourceRange.aspectMask = aspectFlags;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    if (vkCreateImageView(device_, &viewInfo, nullptr, &imageView) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create image view!");
    }
    
    // Store the image view info for cleanup
    imageViews_.push_back({image, imageView});
}

void VulkanMemoryManager::transitionImageLayout(VkImage image, VkFormat format, 
                                               VkImageLayout oldLayout, VkImageLayout newLayout) {
    if (image == VK_NULL_HANDLE) {
        throw std::invalid_argument("Cannot transition layout of null image");
    }

    VkCommandBuffer commandBuffer = beginSingleTimeCommands();

    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;

    VkPipelineStageFlags sourceStage;
    VkPipelineStageFlags destinationStage;

    // Handle common layout transitions
    if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    } else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    } else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        destinationStage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        if (format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT) {
            barrier.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
        }
    } else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        destinationStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    } else {
        throw std::invalid_argument("Unsupported layout transition from " + 
            std::to_string(static_cast<int>(oldLayout)) + " to " + 
            std::to_string(static_cast<int>(newLayout)));
    }

    vkCmdPipelineBarrier(commandBuffer, sourceStage, destinationStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);

    endSingleTimeCommands(commandBuffer);
}

void VulkanMemoryManager::copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height) {
    VkCommandBuffer commandBuffer = beginSingleTimeCommands();

    VkBufferImageCopy region{};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = {0, 0, 0};
    region.imageExtent = {width, height, 1};

    vkCmdCopyBufferToImage(commandBuffer, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    endSingleTimeCommands(commandBuffer);
}

void VulkanMemoryManager::generateMipmaps(VkImage image, VkFormat imageFormat, 
                                         int32_t texWidth, int32_t texHeight, uint32_t mipLevels) {
    if (image == VK_NULL_HANDLE) {
        throw std::invalid_argument("Cannot generate mipmaps for null image");
    }

    if (texWidth <= 0 || texHeight <= 0) {
        throw std::invalid_argument("Invalid texture dimensions");
    }

    if (mipLevels <= 1) {
        return; // No mipmaps needed
    }

    // Check if image format supports linear blitting
    VkFormatProperties formatProperties;
    vkGetPhysicalDeviceFormatProperties(physicalDevice_, imageFormat, &formatProperties);

    if (!(formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT)) {
        throw std::runtime_error("Texture image format does not support linear blitting!");
    }

    VkCommandBuffer commandBuffer = beginSingleTimeCommands();

    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.image = image;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    barrier.subresourceRange.levelCount = 1;

    int32_t mipWidth = texWidth;
    int32_t mipHeight = texHeight;

    for (uint32_t i = 1; i < mipLevels; i++) {
        barrier.subresourceRange.baseMipLevel = i - 1;
        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

        vkCmdPipelineBarrier(commandBuffer,
            VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
            0, nullptr,
            0, nullptr,
            1, &barrier);

        VkImageBlit blit{};
        blit.srcOffsets[0] = {0, 0, 0};
        blit.srcOffsets[1] = {mipWidth, mipHeight, 1};
        blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        blit.srcSubresource.mipLevel = i - 1;
        blit.srcSubresource.baseArrayLayer = 0;
        blit.srcSubresource.layerCount = 1;
        blit.dstOffsets[0] = {0, 0, 0};
        blit.dstOffsets[1] = {mipWidth > 1 ? mipWidth / 2 : 1, mipHeight > 1 ? mipHeight / 2 : 1, 1};
        blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        blit.dstSubresource.mipLevel = i;
        blit.dstSubresource.baseArrayLayer = 0;
        blit.dstSubresource.layerCount = 1;

        vkCmdBlitImage(commandBuffer,
            image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1, &blit,
            VK_FILTER_LINEAR);

        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        vkCmdPipelineBarrier(commandBuffer,
            VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
            0, nullptr,
            0, nullptr,
            1, &barrier);

        if (mipWidth > 1) mipWidth /= 2;
        if (mipHeight > 1) mipHeight /= 2;
    }

    // Transition the last mip level
    barrier.subresourceRange.baseMipLevel = mipLevels - 1;
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    vkCmdPipelineBarrier(commandBuffer,
        VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
        0, nullptr,
        0, nullptr,
        1, &barrier);

    endSingleTimeCommands(commandBuffer);
}

VulkanMemoryManager::DoubleBuffer VulkanMemoryManager::createDoubleBuffer(
    VkDeviceSize size, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage) {
    
    DoubleBuffer result;
    
    // Create both buffers directly into the result
    result.buffers[0] = createBuffer(size, usage, memoryUsage);
    result.buffers[1] = createBuffer(size, usage, memoryUsage);
    result.currentBuffer = 0;
    
    return result; // NRVO should handle this without copy/move
}

void VulkanMemoryManager::destroyDoubleBuffer(DoubleBuffer& doubleBuffer) {
    std::lock_guard<std::mutex> lock(doubleBuffer.bufferMutex);
    
    for (int i = 0; i < 2; i++) {
        if (doubleBuffer.buffers[i].allocation) {
            destroyBuffer(doubleBuffer.buffers[i]);
        }
    }
}

VulkanMemoryManager::InstanceBufferPool* VulkanMemoryManager::createInstanceBufferPool(
    VkDeviceSize bufferSize, uint32_t maxInstances) {
    
    std::lock_guard<std::mutex> lock(instancePoolsMutex_);
    
    auto pool = std::make_unique<InstanceBufferPool>(bufferSize, maxInstances);
    auto* poolPtr = pool.get();
    
    // Pre-allocate buffers - use modern VMA patterns
    for (uint32_t i = 0; i < maxInstances; i++) {
        auto buffer = createBuffer(bufferSize, 
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            VMA_MEMORY_USAGE_AUTO); // Use AUTO instead of GPU_ONLY
        pool->buffers.push_back(buffer);
        pool->inUse.push_back(false);
    }
    
    instancePools_.push_back(std::move(pool));
    return poolPtr;
}

VulkanMemoryManager::BufferAllocation VulkanMemoryManager::allocateFromInstancePool(
    InstanceBufferPool* pool) {
    
    if (!pool) {
        throw std::invalid_argument("Invalid instance buffer pool");
    }
    
    std::lock_guard<std::mutex> lock(pool->poolMutex);
    
    // Find first available buffer
    for (size_t i = 0; i < pool->buffers.size(); i++) {
        if (!pool->inUse[i]) {
            pool->inUse[i] = true;
            return pool->buffers[i];
        }
    }
    
    throw std::runtime_error("No available buffers in instance pool");
}

void VulkanMemoryManager::freeInstanceBuffer(InstanceBufferPool* pool, 
    const BufferAllocation& allocation) {
    
    if (!pool) {
        throw std::invalid_argument("Invalid instance buffer pool");
    }
    
    std::lock_guard<std::mutex> lock(pool->poolMutex);
    
    // Find and mark buffer as available
    for (size_t i = 0; i < pool->buffers.size(); i++) {
        if (pool->buffers[i].buffer == allocation.buffer) {
            pool->inUse[i] = false;
            return;
        }
    }
    
    throw std::runtime_error("Buffer not found in instance pool");
}

void VulkanMemoryManager::destroyInstanceBufferPool(InstanceBufferPool* pool) {
    if (!pool) {
        return;
    }
    
    std::lock_guard<std::mutex> lock(instancePoolsMutex_);
    
    // Find and remove the pool
    auto it = std::find_if(instancePools_.begin(), instancePools_.end(),
        [pool](const std::unique_ptr<InstanceBufferPool>& p) {
            return p.get() == pool;
        });
    
    if (it != instancePools_.end()) {
        // Clean up all buffers in the pool
        for (const auto& buffer : (*it)->buffers) {
            if (buffer.allocation) {
                destroyBuffer(buffer);
            }
        }
        instancePools_.erase(it);
    }
}

void VulkanMemoryManager::cleanupInstancePools() {
    std::lock_guard<std::mutex> lock(instancePoolsMutex_);
    
    for (auto& pool : instancePools_) {
        for (const auto& buffer : pool->buffers) {
            if (buffer.allocation) {
                destroyBuffer(buffer);
            }
        }
    }
    instancePools_.clear();
}

VulkanMemoryManager::StreamingBuffer VulkanMemoryManager::createStreamingBuffer(
    VkDeviceSize size, VkBufferUsageFlags usage) {
    
    std::lock_guard<std::mutex> lock(streamingMutex_);
    
    // Use emplace_back to construct in place
    streamingBuffers_.emplace_back();
    StreamingBuffer& streamingBuffer = streamingBuffers_.back();
    
    streamingBuffer.size = size;
    
    // Create buffer with modern VMA patterns for streaming (frequently updated) buffers
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo allocInfo{};
    allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
    allocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                      VMA_ALLOCATION_CREATE_HOST_ACCESS_ALLOW_TRANSFER_INSTEAD_BIT |
                      VMA_ALLOCATION_CREATE_MAPPED_BIT;

    VmaAllocationInfo allocationInfo;
    if (vmaCreateBuffer(allocator_, &bufferInfo, &allocInfo,
                       &streamingBuffer.buffer.buffer, &streamingBuffer.buffer.allocation,
                       &allocationInfo) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create streaming buffer!");
    }

    streamingBuffer.buffer.allocationInfo = allocationInfo;
    streamingBuffer.buffer.size = size;
    streamingBuffer.buffer.mappedData = allocationInfo.pMappedData;
    
    // Add to buffer pool for tracking
    bufferPool_.push_back(streamingBuffer.buffer);
    
    updateMemoryStats(size, true);
    
    return std::move(streamingBuffer);
}

void VulkanMemoryManager::destroyStreamingBuffer(StreamingBuffer& buffer) {
    std::lock_guard<std::mutex> lock(streamingMutex_);
    
    if (buffer.buffer.allocation) {
        updateMemoryStats(buffer.size, false);
        destroyBuffer(buffer.buffer);
    }
    
    auto it = std::find_if(streamingBuffers_.begin(), streamingBuffers_.end(),
        [&buffer](const StreamingBuffer& b) {
            return b.buffer.buffer == buffer.buffer.buffer;
        });
    
    if (it != streamingBuffers_.end()) {
        streamingBuffers_.erase(it);
    }
}

void* VulkanMemoryManager::mapStreamingBuffer(StreamingBuffer& buffer) {
    std::lock_guard<std::mutex> lock(buffer.bufferMutex);
    return mapMemory(buffer.buffer);
}

void VulkanMemoryManager::unmapStreamingBuffer(StreamingBuffer& buffer) {
    std::lock_guard<std::mutex> lock(buffer.bufferMutex);
    unmapMemory(buffer.buffer);
}

void VulkanMemoryManager::updateStreamingBuffer(StreamingBuffer& buffer, 
    const void* data, VkDeviceSize size, VkDeviceSize offset) {
    
    std::lock_guard<std::mutex> lock(buffer.bufferMutex);
    
    if (offset + size > buffer.size) {
        throw std::runtime_error("Streaming buffer update exceeds buffer size");
    }
    
    void* mappedData = mapMemory(buffer.buffer);
    memcpy(static_cast<char*>(mappedData) + offset, data, size);
    unmapMemory(buffer.buffer);
}

VulkanMemoryManager::MemoryPool* VulkanMemoryManager::createMemoryPool(
    VkDeviceSize bufferSize, VmaMemoryUsage memoryUsage, VkBufferUsageFlags usage) {
    
    std::lock_guard<std::mutex> lock(memoryPoolsMutex_);
    
    auto pool = std::make_unique<MemoryPool>(bufferSize, memoryUsage, usage);
    auto* poolPtr = pool.get();
    
    // Pre-allocate some buffers
    for (uint32_t i = 0; i < 10; i++) {
        auto buffer = createBuffer(bufferSize, usage, memoryUsage);
        pool->buffers.push_back(buffer);
        pool->inUse.push_back(false);
    }
    
    memoryPools_.push_back(std::move(pool));
    return poolPtr;
}

VulkanMemoryManager::BufferAllocation VulkanMemoryManager::allocateFromPool(MemoryPool* pool) {
    if (!pool) {
        throw std::invalid_argument("Invalid memory pool");
    }
    
    std::lock_guard<std::mutex> lock(pool->poolMutex);
    
    // Find first available buffer
    for (size_t i = 0; i < pool->buffers.size(); i++) {
        if (!pool->inUse[i]) {
            pool->inUse[i] = true;
            updateMemoryStats(pool->bufferSize, true);
            return pool->buffers[i];
        }
    }
    
    // Create new buffer if none available
    auto buffer = createBuffer(pool->bufferSize, pool->usageFlags, pool->memoryUsage);
    pool->buffers.push_back(buffer);
    pool->inUse.push_back(true);
    updateMemoryStats(pool->bufferSize, true);
    
    return buffer;
}

void VulkanMemoryManager::freeToPool(MemoryPool* pool, const BufferAllocation& allocation) {
    if (!pool) {
        throw std::invalid_argument("Invalid memory pool");
    }
    
    std::lock_guard<std::mutex> lock(pool->poolMutex);
    
    for (size_t i = 0; i < pool->buffers.size(); i++) {
        if (pool->buffers[i].buffer == allocation.buffer) {
            pool->inUse[i] = false;
            updateMemoryStats(pool->bufferSize, false);
            return;
        }
    }
    
    throw std::runtime_error("Buffer not found in memory pool");
}

void VulkanMemoryManager::destroyMemoryPool(MemoryPool* pool) {
    if (!pool) {
        return;
    }
    
    std::lock_guard<std::mutex> lock(memoryPoolsMutex_);
    
    auto it = std::find_if(memoryPools_.begin(), memoryPools_.end(),
        [pool](const std::unique_ptr<MemoryPool>& p) {
            return p.get() == pool;
        });
    
    if (it != memoryPools_.end()) {
        for (const auto& buffer : (*it)->buffers) {
            if (buffer.allocation) {
                destroyBuffer(buffer);
            }
        }
        memoryPools_.erase(it);
    }
}

void VulkanMemoryManager::resetMemoryStats() {
    memoryStats_.reset();
}

const VulkanMemoryManager::MemoryStats& VulkanMemoryManager::getMemoryStats() const {
    return memoryStats_;
}

void VulkanMemoryManager::updateMemoryStats(size_t size, bool isAllocation) {
    memoryStats_.update(size, isAllocation);
}

VulkanMemoryManager::TimelineSemaphore VulkanMemoryManager::createTimelineSemaphore(uint64_t initialValue) {
    std::lock_guard<std::mutex> lock(semaphoreMutex_);
    
    TimelineSemaphore semaphore;
    semaphore.currentValue = initialValue;
    
    VkSemaphoreTypeCreateInfo timelineInfo{};
    timelineInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO;
    timelineInfo.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
    timelineInfo.initialValue = initialValue;
    
    VkSemaphoreCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    createInfo.pNext = &timelineInfo;
    
    if (vkCreateSemaphore(device_, &createInfo, nullptr, &semaphore.semaphore) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create timeline semaphore!");
    }
    
    timelineSemaphores_.push_back(std::move(semaphore));
    return std::move(timelineSemaphores_.back());
}

void VulkanMemoryManager::destroyTimelineSemaphore(TimelineSemaphore& semaphore) {
    std::lock_guard<std::mutex> lock(semaphoreMutex_);
    
    if (semaphore.semaphore != VK_NULL_HANDLE) {
        vkDestroySemaphore(device_, semaphore.semaphore, nullptr);
        semaphore.semaphore = VK_NULL_HANDLE;
    }
    
    auto it = std::find_if(timelineSemaphores_.begin(), timelineSemaphores_.end(),
        [&semaphore](const TimelineSemaphore& s) {
            return s.semaphore == semaphore.semaphore;
        });
    
    if (it != timelineSemaphores_.end()) {
        timelineSemaphores_.erase(it);
    }
}

void VulkanMemoryManager::waitTimelineSemaphore(const TimelineSemaphore& semaphore, uint64_t value) {
    std::lock_guard<std::mutex> lock(semaphore.semaphoreMutex);
    
    VkSemaphoreWaitInfo waitInfo{};
    waitInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO;
    waitInfo.semaphoreCount = 1;
    waitInfo.pSemaphores = &semaphore.semaphore;
    waitInfo.pValues = &value;
    
    if (vkWaitSemaphores(device_, &waitInfo, UINT64_MAX) != VK_SUCCESS) {
        throw std::runtime_error("Failed to wait for timeline semaphore!");
    }
}

void VulkanMemoryManager::signalTimelineSemaphore(TimelineSemaphore& semaphore, uint64_t value) {
    std::lock_guard<std::mutex> lock(semaphore.semaphoreMutex);
    
    VkSemaphoreSignalInfo signalInfo{};
    signalInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SIGNAL_INFO;
    signalInfo.semaphore = semaphore.semaphore;
    signalInfo.value = value;
    
    if (vkSignalSemaphore(device_, &signalInfo) != VK_SUCCESS) {
        throw std::runtime_error("Failed to signal timeline semaphore!");
    }
    
    semaphore.currentValue = value;
}

void VulkanMemoryManager::cleanupMemoryPools() {
    std::lock_guard<std::mutex> lock(memoryPoolsMutex_);
    
    for (auto& pool : memoryPools_) {
        for (const auto& buffer : pool->buffers) {
            if (buffer.allocation) {
                destroyBuffer(buffer);
            }
        }
    }
    memoryPools_.clear();
}

void VulkanMemoryManager::cleanupStreamingBuffers() {
    std::lock_guard<std::mutex> lock(streamingMutex_);
    
    for (auto& buffer : streamingBuffers_) {
        if (buffer.buffer.allocation) {
            destroyBuffer(buffer.buffer);
        }
    }
    streamingBuffers_.clear();
}

void VulkanMemoryManager::cleanupTimelineSemaphores() {
    std::lock_guard<std::mutex> lock(semaphoreMutex_);
    
    for (auto& semaphore : timelineSemaphores_) {
        if (semaphore.semaphore != VK_NULL_HANDLE) {
            vkDestroySemaphore(device_, semaphore.semaphore, nullptr);
        }
    }
    timelineSemaphores_.clear();
}

VkDeviceSize VulkanMemoryManager::alignSize(VkDeviceSize size, VkDeviceSize alignment) const {
    return (size + alignment - 1) & ~(alignment - 1);
}

// Modern VMA convenience functions
VulkanMemoryManager::BufferAllocation VulkanMemoryManager::createHostVisibleBuffer(
    VkDeviceSize size, VkBufferUsageFlags usage, bool sequentialWrite) {
    
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo allocInfo{};
    allocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST; // Prefer host for host-visible buffers
    allocInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;
    
    if (sequentialWrite) {
        allocInfo.flags |= VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
        allocInfo.flags |= VMA_ALLOCATION_CREATE_STRATEGY_MIN_TIME_BIT; // Fast allocation
    } else {
        allocInfo.flags |= VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT;
        allocInfo.flags |= VMA_ALLOCATION_CREATE_STRATEGY_MIN_TIME_BIT; // Fast allocation for random access
    }
    
    // Set priority based on usage and size
    if (usage & VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT) {
        allocInfo.priority = 0.9f; // High priority for uniform buffers
    } else if (size > 4 * 1024 * 1024) { // 4MB threshold
        allocInfo.priority = 0.75f; // Medium-high priority for large buffers
        allocInfo.flags |= VMA_ALLOCATION_CREATE_WITHIN_BUDGET_BIT;
    } else {
        allocInfo.priority = 0.6f; // Medium priority for other host-visible buffers
    }

    BufferAllocation allocation{};
    allocation.size = size;

    if (vmaCreateBuffer(allocator_, &bufferInfo, &allocInfo,
                       &allocation.buffer, &allocation.allocation,
                       &allocation.allocationInfo) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create host visible buffer!");
    }

    allocation.mappedData = allocation.allocationInfo.pMappedData;
    bufferPool_.push_back(allocation);
    
    return allocation;
}

VulkanMemoryManager::BufferAllocation VulkanMemoryManager::createDeviceLocalBuffer(
    VkDeviceSize size, VkBufferUsageFlags usage, bool dedicatedMemory) {
    
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo allocInfo{};
    allocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE; // Prefer device local memory
    allocInfo.flags = VMA_ALLOCATION_CREATE_STRATEGY_MIN_MEMORY_BIT; // Optimize for memory efficiency
    
    if (dedicatedMemory || size > 64 * 1024 * 1024) { // 64MB threshold
        allocInfo.flags |= VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
        allocInfo.priority = 1.0f; // Highest priority for dedicated memory
        allocInfo.flags |= VMA_ALLOCATION_CREATE_WITHIN_BUDGET_BIT; // Budget-aware for large allocations
    } else if (size > 16 * 1024 * 1024) { // 16MB threshold  
        allocInfo.priority = 0.85f; // High priority for large buffers
        allocInfo.flags |= VMA_ALLOCATION_CREATE_WITHIN_BUDGET_BIT;
    } else if (usage & (VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT)) {
        allocInfo.priority = 0.8f; // High priority for compute storage
    } else if (usage & (VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT)) {
        allocInfo.priority = 0.75f; // High priority for vertex/index data
    } else {
        allocInfo.priority = 0.5f; // Default priority
    }
    
    // Special handling for render targets
    if (usage & (VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT)) {
        allocInfo.flags |= VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
        allocInfo.priority = 1.0f;
    }

    BufferAllocation allocation{};
    allocation.size = size;
    allocation.mappedData = nullptr;

    if (vmaCreateBuffer(allocator_, &bufferInfo, &allocInfo,
                       &allocation.buffer, &allocation.allocation,
                       &allocation.allocationInfo) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create device local buffer!");
    }

    bufferPool_.push_back(allocation);
    
    return allocation;
}

VulkanMemoryManager::BufferAllocation VulkanMemoryManager::createUniformBuffer(
    VkDeviceSize size, bool dynamic) {
    
    VkBufferUsageFlags usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    if (dynamic) {
        usage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    }
    
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo allocInfo{};
    allocInfo.usage = VMA_MEMORY_USAGE_AUTO; // Let VMA decide host vs device based on usage
    allocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
    allocInfo.priority = 0.9f; // High priority for uniform buffers (frequently accessed)
    
    if (dynamic) {
        // For dynamic uniform buffers, allow transfer fallback and optimize for frequent updates
        allocInfo.flags |= VMA_ALLOCATION_CREATE_HOST_ACCESS_ALLOW_TRANSFER_INSTEAD_BIT;
        allocInfo.flags |= VMA_ALLOCATION_CREATE_MAPPED_BIT;
        allocInfo.flags |= VMA_ALLOCATION_CREATE_STRATEGY_MIN_TIME_BIT; // Fast allocation for frequent updates
        allocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST; // Prefer host for dynamic access
    } else {
        // Static uniform buffers can use device memory for better performance
        allocInfo.flags |= VMA_ALLOCATION_CREATE_STRATEGY_MIN_MEMORY_BIT; // Optimize for memory efficiency
        // For small static uniform buffers, we still want host access for initial upload
        if (size <= 64 * 1024) { // 64KB threshold
            allocInfo.flags |= VMA_ALLOCATION_CREATE_MAPPED_BIT;
        }
    }
    
    // Budget awareness for larger uniform buffers
    if (size > 1024 * 1024) { // 1MB threshold
        allocInfo.flags |= VMA_ALLOCATION_CREATE_WITHIN_BUDGET_BIT;
    }

    BufferAllocation allocation{};
    allocation.size = size;

    if (vmaCreateBuffer(allocator_, &bufferInfo, &allocInfo,
                       &allocation.buffer, &allocation.allocation,
                       &allocation.allocationInfo) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create uniform buffer!");
    }

    allocation.mappedData = allocation.allocationInfo.pMappedData;
    bufferPool_.push_back(allocation);
    
    return allocation;
}

void VulkanMemoryManager::copyToAllocation(const void* srcData, 
    const BufferAllocation& dstAllocation, VkDeviceSize size, VkDeviceSize offset) {
    
    if (!srcData) {
        throw std::invalid_argument("Source data cannot be null");
    }
    
    if (offset + size > dstAllocation.size) {
        throw std::invalid_argument("Copy size exceeds allocation bounds");
    }
    
    // Use VMA's convenient copy function
    VkResult result = vmaCopyMemoryToAllocation(allocator_, srcData, 
                                               dstAllocation.allocation, offset, size);
    
    if (result != VK_SUCCESS) {
        throw std::runtime_error("Failed to copy memory to allocation: " + std::to_string(result));
    }
}

void VulkanMemoryManager::copyFromAllocation(const BufferAllocation& srcAllocation, 
    void* dstData, VkDeviceSize size, VkDeviceSize offset) {
    
    if (!dstData) {
        throw std::invalid_argument("Destination data cannot be null");
    }
    
    if (offset + size > srcAllocation.size) {
        throw std::invalid_argument("Copy size exceeds allocation bounds");
    }
    
    // Use VMA's convenient copy function
    VkResult result = vmaCopyAllocationToMemory(allocator_, srcAllocation.allocation, 
                                               offset, dstData, size);
    
    if (result != VK_SUCCESS) {
        throw std::runtime_error("Failed to copy memory from allocation: " + std::to_string(result));
    }
}

void VulkanMemoryManager::getMemoryBudget(VmaBudget* budget) const {
    if (!budget) {
        throw std::invalid_argument("Budget pointer cannot be null");
    }
    
    vmaGetHeapBudgets(allocator_, budget);
}

void VulkanMemoryManager::printMemoryStatistics() const {
    VmaTotalStatistics stats;
    vmaCalculateStatistics(allocator_, &stats);
    
    std::printf("=== VMA Memory Statistics ===\n");
    std::printf("Allocation count: %u\n", stats.total.statistics.allocationCount);
    std::printf("Block count: %u\n", stats.total.statistics.blockCount);
    std::printf("Allocated bytes: %.2f MB\n", 
                stats.total.statistics.allocationBytes / (1024.0 * 1024.0));
    std::printf("Used bytes: %.2f MB\n", 
                stats.total.statistics.blockBytes / (1024.0 * 1024.0));
    std::printf("Unused bytes: %.2f MB\n", 
                (stats.total.statistics.blockBytes - stats.total.statistics.allocationBytes) / (1024.0 * 1024.0));
    
    VmaBudget budgets[VK_MAX_MEMORY_HEAPS];
    vmaGetHeapBudgets(allocator_, budgets);
    
    VkPhysicalDeviceMemoryProperties memProps;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice_, &memProps);
    
    std::printf("\n=== Memory Heap Usage ===\n");
    for (uint32_t i = 0; i < memProps.memoryHeapCount; i++) {
        std::printf("Heap %u: %.2f MB / %.2f MB (%.1f%%)\n", i,
                    budgets[i].usage / (1024.0 * 1024.0),
                    budgets[i].budget / (1024.0 * 1024.0),
                    (budgets[i].budget > 0) ? (budgets[i].usage * 100.0 / budgets[i].budget) : 0.0);
    }
    std::printf("=============================\n");
}

} // namespace VulkanHIP 
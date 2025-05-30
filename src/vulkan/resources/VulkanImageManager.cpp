#include "vulkan/resources/VulkanImageManager.h"
#include "VulkanContext.h"
#include "VulkanMemoryManager.h"
#include <stdexcept>
#include <array>

namespace VulkanHIP {

VulkanImageManager::VulkanImageManager(VulkanContext* context, VulkanMemoryManager* memoryManager)
    : vulkanContext_(context), memoryManager_(memoryManager) {
    if (!vulkanContext_ || !memoryManager_) {
        throw std::runtime_error("VulkanImageManager: Invalid context or memory manager");
    }
}

VulkanImageManager::~VulkanImageManager() {
    cleanup();
}

void VulkanImageManager::createDepthResources(uint32_t width, uint32_t height, VkSampleCountFlagBits msaaSamples) {
    VkFormat depthFormat = findDepthFormat();
    
    auto depthImageAlloc = memoryManager_->createImage(
        width, height, 1, msaaSamples, depthFormat,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
        VMA_MEMORY_USAGE_GPU_ONLY
    );
    
    depthImage_ = depthImageAlloc.image;
    depthImageAllocation_ = depthImageAlloc.allocation;
    
    depthImageView_ = createImageView(depthImage_, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT);
}

void VulkanImageManager::createColorResources(uint32_t width, uint32_t height, VkFormat format, VkSampleCountFlagBits msaaSamples) {
    auto colorImageAlloc = memoryManager_->createImage(
        width, height, 1, msaaSamples, format,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        VMA_MEMORY_USAGE_GPU_ONLY
    );
    
    colorImage_ = colorImageAlloc.image;
    colorImageAllocation_ = colorImageAlloc.allocation;
    
    colorImageView_ = createImageView(colorImage_, format, VK_IMAGE_ASPECT_COLOR_BIT);
}

void VulkanImageManager::createTextureImage(unsigned char* pixels, uint32_t width, uint32_t height, int channels) {
    VkDeviceSize imageSize = width * height * 4; // Assuming RGBA
    
    // Create staging buffer
    auto stagingBuffer = memoryManager_->createStagingBuffer(imageSize);
    void* data = memoryManager_->mapStagingBuffer(stagingBuffer);
    memcpy(data, pixels, static_cast<size_t>(imageSize));
    memoryManager_->unmapStagingBuffer(stagingBuffer);
    
    // Create texture image
    auto textureImageAlloc = memoryManager_->createImage(
        width, height, 1, VK_SAMPLE_COUNT_1_BIT, VK_FORMAT_R8G8B8A8_SRGB,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VMA_MEMORY_USAGE_GPU_ONLY
    );
    
    textureImage_ = textureImageAlloc.image;
    textureImageAllocation_ = textureImageAlloc.allocation;
    
    // Transition image layout and copy buffer to image
    memoryManager_->transitionImageLayout(textureImage_, VK_FORMAT_R8G8B8A8_SRGB,
        VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    memoryManager_->copyBufferToImage(stagingBuffer.buffer, textureImage_, width, height);
    memoryManager_->transitionImageLayout(textureImage_, VK_FORMAT_R8G8B8A8_SRGB,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    
    // Cleanup staging buffer
    memoryManager_->destroyStagingBuffer(stagingBuffer);
}

void VulkanImageManager::createTextureImageView() {
    textureImageView_ = createImageView(textureImage_, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_ASPECT_COLOR_BIT);
}

void VulkanImageManager::createTextureSampler() {
    VkPhysicalDeviceProperties properties{};
    vkGetPhysicalDeviceProperties(vulkanContext_->getPhysicalDevice(), &properties);
    
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.anisotropyEnable = VK_TRUE;
    samplerInfo.maxAnisotropy = properties.limits.maxSamplerAnisotropy;
    samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    
    if (vkCreateSampler(vulkanContext_->getDevice(), &samplerInfo, nullptr, &textureSampler_) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create texture sampler!");
    }
}

VkImageView VulkanImageManager::createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags) {
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
    
    VkImageView imageView;
    if (vkCreateImageView(vulkanContext_->getDevice(), &viewInfo, nullptr, &imageView) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create image view!");
    }
    
    return imageView;
}

VkFormat VulkanImageManager::findDepthFormat() {
    std::array<VkFormat, 3> candidates = {
        VK_FORMAT_D32_SFLOAT,
        VK_FORMAT_D32_SFLOAT_S8_UINT,
        VK_FORMAT_D24_UNORM_S8_UINT
    };
    
    for (VkFormat format : candidates) {
        VkFormatProperties props;
        vkGetPhysicalDeviceFormatProperties(vulkanContext_->getPhysicalDevice(), format, &props);
        
        if (props.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) {
            return format;
        }
    }
    
    throw std::runtime_error("Failed to find supported depth format!");
}

bool VulkanImageManager::hasStencilComponent(VkFormat format) {
    return format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT;
}

void VulkanImageManager::cleanup() {
    if (!memoryManager_) return;
    
    // Cleanup texture resources
    if (textureSampler_ != VK_NULL_HANDLE) {
        vkDestroySampler(vulkanContext_->getDevice(), textureSampler_, nullptr);
        textureSampler_ = VK_NULL_HANDLE;
    }
    
    if (textureImageView_ != VK_NULL_HANDLE) {
        vkDestroyImageView(vulkanContext_->getDevice(), textureImageView_, nullptr);
        textureImageView_ = VK_NULL_HANDLE;
    }
    
    if (textureImage_ != VK_NULL_HANDLE) {
        vmaDestroyImage(memoryManager_->getAllocator(), textureImage_, textureImageAllocation_);
        textureImage_ = VK_NULL_HANDLE;
        textureImageAllocation_ = VK_NULL_HANDLE;
    }
    
    // Cleanup color resources
    if (colorImageView_ != VK_NULL_HANDLE) {
        vkDestroyImageView(vulkanContext_->getDevice(), colorImageView_, nullptr);
        colorImageView_ = VK_NULL_HANDLE;
    }
    
    if (colorImage_ != VK_NULL_HANDLE) {
        vmaDestroyImage(memoryManager_->getAllocator(), colorImage_, colorImageAllocation_);
        colorImage_ = VK_NULL_HANDLE;
        colorImageAllocation_ = VK_NULL_HANDLE;
    }
    
    // Cleanup depth resources
    if (depthImageView_ != VK_NULL_HANDLE) {
        vkDestroyImageView(vulkanContext_->getDevice(), depthImageView_, nullptr);
        depthImageView_ = VK_NULL_HANDLE;
    }
    
    if (depthImage_ != VK_NULL_HANDLE) {
        vmaDestroyImage(memoryManager_->getAllocator(), depthImage_, depthImageAllocation_);
        depthImage_ = VK_NULL_HANDLE;
        depthImageAllocation_ = VK_NULL_HANDLE;
    }
}

} // namespace VulkanHIP
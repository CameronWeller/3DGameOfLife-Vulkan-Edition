#include "gpu/Swapchain.h"

#include <algorithm>
#include <stdexcept>

#include <GLFW/glfw3.h>

#include "gpu/GpuBuffer.h"
#include "gpu/GpuContext.h"

namespace gol3d {

void Swapchain::create(const GpuContext& gpu, const BufferAllocator& allocator,
                       GLFWwindow* window) {
    gpu_ = &gpu;
    allocator_ = &allocator;
    window_ = window;
    createSwapchain();
    createImageViews();
    depthFormat_ = pickDepthFormat();
    createDepthBuffer();
    createSemaphores();
}

void Swapchain::recreate() {
    int width = 0;
    int height = 0;
    glfwGetFramebufferSize(window_, &width, &height);
    while ((width == 0 || height == 0) && !glfwWindowShouldClose(window_)) {
        glfwWaitEvents(); // minimized: nothing to draw into
        glfwGetFramebufferSize(window_, &width, &height);
    }
    vkDeviceWaitIdle(gpu_->device);
    destroySizedResources();
    createSwapchain();
    createImageViews();
    createDepthBuffer();
    createSemaphores();
}

void Swapchain::destroy() {
    if (!gpu_) return;
    destroySizedResources();
    gpu_ = nullptr;
}

float Swapchain::aspectRatio() const {
    return extent_.height ? static_cast<float>(extent_.width) / static_cast<float>(extent_.height)
                          : 1.0f;
}

void Swapchain::createSwapchain() {
    SurfaceSupport support = gpu_->querySurface();
    const VkSurfaceCapabilitiesKHR& capabilities = support.capabilities;

    // 8-bit sRGB, so the GPU applies the gamma curve when writing pixels.
    VkSurfaceFormatKHR surfaceFormat = support.formats[0];
    for (const VkSurfaceFormatKHR& format : support.formats) {
        if (format.format == VK_FORMAT_B8G8R8A8_SRGB &&
            format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            surfaceFormat = format;
            break;
        }
    }

    // Some platforms leave the size to us (currentExtent = UINT32_MAX).
    VkExtent2D extent = capabilities.currentExtent;
    if (extent.width == UINT32_MAX) {
        int width = 0;
        int height = 0;
        glfwGetFramebufferSize(window_, &width, &height);
        extent.width = std::clamp(static_cast<uint32_t>(width), capabilities.minImageExtent.width,
                                  capabilities.maxImageExtent.width);
        extent.height =
            std::clamp(static_cast<uint32_t>(height), capabilities.minImageExtent.height,
                       capabilities.maxImageExtent.height);
    }

    // One more image than the minimum, so the GPU never waits on the compositor.
    uint32_t imageCount = capabilities.minImageCount + 1;
    if (capabilities.maxImageCount > 0) {
        imageCount = std::min(imageCount, capabilities.maxImageCount);
    }

    captureSupported_ = capabilities.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

    VkSwapchainCreateInfoKHR createInfo{VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
    createInfo.surface = gpu_->surface;
    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = surfaceFormat.format;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    createInfo.imageExtent = extent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                            (captureSupported_ ? VK_IMAGE_USAGE_TRANSFER_SRC_BIT : 0);
    createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE; // one queue draws and presents
    createInfo.preTransform = capabilities.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode = VK_PRESENT_MODE_FIFO_KHR; // vsync; always supported
    createInfo.clipped = VK_TRUE;
    if (vkCreateSwapchainKHR(gpu_->device, &createInfo, nullptr, &swapchain_) != VK_SUCCESS) {
        throw std::runtime_error("Could not create swap chain.");
    }

    uint32_t actualCount = 0;
    vkGetSwapchainImagesKHR(gpu_->device, swapchain_, &actualCount, nullptr);
    images_.resize(actualCount);
    vkGetSwapchainImagesKHR(gpu_->device, swapchain_, &actualCount, images_.data());
    colorFormat_ = surfaceFormat.format;
    extent_ = extent;
}

void Swapchain::createImageViews() {
    imageViews_.resize(images_.size());
    for (size_t i = 0; i < images_.size(); i++) {
        VkImageViewCreateInfo createInfo{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        createInfo.image = images_[i];
        createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        createInfo.format = colorFormat_;
        createInfo.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        if (vkCreateImageView(gpu_->device, &createInfo, nullptr, &imageViews_[i]) != VK_SUCCESS) {
            throw std::runtime_error("Could not create image views.");
        }
    }
}

VkFormat Swapchain::pickDepthFormat() const {
    for (VkFormat format :
         {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D24_UNORM_S8_UINT, VK_FORMAT_D32_SFLOAT_S8_UINT}) {
        VkFormatProperties properties;
        vkGetPhysicalDeviceFormatProperties(gpu_->physicalDevice, format, &properties);
        if (properties.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) {
            return format;
        }
    }
    throw std::runtime_error("Could not find supported depth format.");
}

void Swapchain::createDepthBuffer() {
    VkDevice device = gpu_->device;
    VkImageCreateInfo imageInfo{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent = {extent_.width, extent_.height, 1};
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = depthFormat_;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (vkCreateImage(device, &imageInfo, nullptr, &depthImage_) != VK_SUCCESS) {
        throw std::runtime_error("Could not create depth image.");
    }

    VkMemoryRequirements requirements;
    vkGetImageMemoryRequirements(device, depthImage_, &requirements);
    std::optional<uint32_t> type =
        allocator_->findMemoryType(requirements.memoryTypeBits, memory::DEVICE);
    if (!type) throw std::runtime_error("No device-local memory for the depth buffer.");
    VkMemoryAllocateInfo allocInfo{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    allocInfo.allocationSize = requirements.size;
    allocInfo.memoryTypeIndex = *type;
    if (vkAllocateMemory(device, &allocInfo, nullptr, &depthMemory_) != VK_SUCCESS) {
        throw std::runtime_error("Could not allocate depth image memory.");
    }
    vkBindImageMemory(device, depthImage_, depthMemory_, 0);

    VkImageViewCreateInfo viewInfo{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    viewInfo.image = depthImage_;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = depthFormat_;
    viewInfo.subresourceRange = {VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1};
    if (vkCreateImageView(device, &viewInfo, nullptr, &depthImageView_) != VK_SUCCESS) {
        throw std::runtime_error("Could not create depth image view.");
    }
}

void Swapchain::createSemaphores() {
    VkSemaphoreCreateInfo semaphoreInfo{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    renderFinished_.resize(images_.size());
    for (VkSemaphore& semaphore : renderFinished_) {
        if (vkCreateSemaphore(gpu_->device, &semaphoreInfo, nullptr, &semaphore) != VK_SUCCESS) {
            throw std::runtime_error("Could not create semaphore.");
        }
    }
}

void Swapchain::destroySizedResources() {
    VkDevice device = gpu_->device;
    for (VkSemaphore semaphore : renderFinished_) {
        vkDestroySemaphore(device, semaphore, nullptr);
    }
    renderFinished_.clear();
    vkDestroyImageView(device, depthImageView_, nullptr);
    vkDestroyImage(device, depthImage_, nullptr);
    vkFreeMemory(device, depthMemory_, nullptr);
    depthImageView_ = VK_NULL_HANDLE;
    depthImage_ = VK_NULL_HANDLE;
    depthMemory_ = VK_NULL_HANDLE;
    for (VkImageView view : imageViews_) {
        vkDestroyImageView(device, view, nullptr);
    }
    imageViews_.clear();
    vkDestroySwapchainKHR(device, swapchain_, nullptr);
    swapchain_ = VK_NULL_HANDLE;
}

} // namespace gol3d

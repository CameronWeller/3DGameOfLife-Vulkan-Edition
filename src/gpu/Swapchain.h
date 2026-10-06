#pragma once

// The window's swapchain plus everything sized like it: one image view and one
// "render finished" semaphore per swapchain image, and a depth buffer.
//
// Rendering uses Vulkan 1.3 dynamic rendering, so there are no render passes or
// framebuffers to keep in sync. Presentation is FIFO (vsync), which every
// driver supports.

#include <vector>

#include <volk.h>

struct GLFWwindow;

namespace gol3d {

class BufferAllocator;
class GpuContext;

class Swapchain {
public:
    void create(const GpuContext& gpu, const BufferAllocator& allocator, GLFWwindow* window);
    // After a resize: waits while the window is minimized, then rebuilds
    // everything with the new size.
    void recreate();
    void destroy();

    VkSwapchainKHR handle() const { return swapchain_; }
    VkExtent2D extent() const { return extent_; }
    float aspectRatio() const;
    const VkFormat& colorFormat() const { return colorFormat_; }
    VkFormat depthFormat() const { return depthFormat_; }
    size_t imageCount() const { return images_.size(); }
    VkImage image(uint32_t index) const { return images_[index]; }
    VkImageView imageView(uint32_t index) const { return imageViews_[index]; }
    VkImage depthImage() const { return depthImage_; }
    VkImageView depthImageView() const { return depthImageView_; }
    // Signaled when image `index` is ready to present. One per image rather than
    // per frame in flight, because presentation may still hold a semaphore
    // after the frame's fence has signaled.
    VkSemaphore renderFinished(uint32_t index) const { return renderFinished_[index]; }
    // Whether images can be copied out (for screenshots).
    bool captureSupported() const { return captureSupported_; }

private:
    void createSwapchain();
    void createImageViews();
    void createDepthBuffer();
    void createSemaphores();
    void destroySizedResources();
    VkFormat pickDepthFormat() const;

    const GpuContext* gpu_ = nullptr;
    const BufferAllocator* allocator_ = nullptr; // finds memory types for the depth buffer
    GLFWwindow* window_ = nullptr;

    VkSwapchainKHR swapchain_ = VK_NULL_HANDLE;
    std::vector<VkImage> images_;
    std::vector<VkImageView> imageViews_;
    std::vector<VkSemaphore> renderFinished_;
    VkFormat colorFormat_ = VK_FORMAT_UNDEFINED;
    VkExtent2D extent_{};
    bool captureSupported_ = false;

    VkFormat depthFormat_ = VK_FORMAT_UNDEFINED;
    VkImage depthImage_ = VK_NULL_HANDLE;
    VkDeviceMemory depthMemory_ = VK_NULL_HANDLE;
    VkImageView depthImageView_ = VK_NULL_HANDLE;
};

} // namespace gol3d

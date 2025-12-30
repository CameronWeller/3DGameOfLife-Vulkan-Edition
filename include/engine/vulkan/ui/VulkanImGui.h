#pragma once

#include <vulkan/vulkan.h>
#include "imgui.h"
#include "imgui_impl_vulkan.h"
#include "imgui_impl_glfw.h"

namespace VulkanHIP {

class VulkanContext;
class WindowManager;

class VulkanImGui {
public:
    VulkanImGui(VulkanContext* context, WindowManager* windowManager, VkRenderPass renderPass);
    ~VulkanImGui();
    
    VulkanImGui(const VulkanImGui&) = delete;
    VulkanImGui& operator=(const VulkanImGui&) = delete;
    
    void initialize();
    void createDescriptorPool();
    void beginFrame();
    void endFrame(VkCommandBuffer commandBuffer);
    void cleanup();
    
private:
    VulkanContext* vulkanContext_;
    WindowManager* windowManager_;
    VkRenderPass renderPass_;
    
    VkDescriptorPool imguiDescriptorPool_ = VK_NULL_HANDLE;
    bool initialized_ = false;
};

} // namespace VulkanHIP
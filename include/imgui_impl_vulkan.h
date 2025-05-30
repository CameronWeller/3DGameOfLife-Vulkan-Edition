#pragma once

#include <imgui.h>
#include <vulkan/vulkan.h>

namespace ImGui {
    struct VulkanInitInfo {
        VkInstance instance;
        VkPhysicalDevice physicalDevice;
        VkDevice device;
        uint32_t queueFamily;
        VkQueue queue;
        VkPipelineCache pipelineCache;
        VkDescriptorPool descriptorPool;
        uint32_t minImageCount;
        uint32_t imageCount;
        VkSampleCountFlagBits msaaSamples;
        const VkAllocationCallbacks* allocator;
    };

    bool InitForVulkan(const VulkanInitInfo& info);
    void ShutdownForVulkan();
    void NewFrameForVulkan();
    void RenderForVulkan(VkCommandBuffer commandBuffer);
    void CreateFontsTexture(VkCommandBuffer commandBuffer);
    void DestroyFontUploadObjects();
} 
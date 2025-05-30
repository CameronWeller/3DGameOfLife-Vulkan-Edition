#include "vulkan/ui/VulkanImGui.h"
#include "VulkanContext.h"
#include "WindowManager.h"
#include <stdexcept>

// Add missing ImGui implementation includes
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_vulkan.h"

namespace VulkanHIP {

VulkanImGui::VulkanImGui(VulkanContext* context, WindowManager* windowManager, VkRenderPass renderPass)
    : vulkanContext_(context), windowManager_(windowManager), renderPass_(renderPass) {
    if (!vulkanContext_ || !windowManager_) {
        throw std::runtime_error("VulkanImGui: Invalid dependencies");
    }
}

VulkanImGui::~VulkanImGui() {
    cleanup();
}

void VulkanImGui::initialize() {
    if (initialized_) {
        return;
    }

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();

    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForVulkan(windowManager_->getWindow(), true);
    
    createDescriptorPool();
    
    ImGui_ImplVulkan_InitInfo init_info = {};
    init_info.Instance = vulkanContext_->getInstance();
    init_info.PhysicalDevice = vulkanContext_->getPhysicalDevice();
    init_info.Device = vulkanContext_->getDevice();
    init_info.QueueFamily = vulkanContext_->getQueueFamilyIndices().graphicsFamily.value();
    init_info.Queue = vulkanContext_->getGraphicsQueue();
    init_info.PipelineCache = VK_NULL_HANDLE;
    init_info.DescriptorPool = imguiDescriptorPool_;
    init_info.Subpass = 0;
    init_info.MinImageCount = 2;
    init_info.ImageCount = 2;
    init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    init_info.Allocator = nullptr;
    init_info.CheckVkResultFn = nullptr;
    
    ImGui_ImplVulkan_Init(&init_info, renderPass_);

    // Upload fonts - use memory manager for command buffer operations
    VkCommandBuffer commandBuffer = vulkanContext_->getMemoryManager().beginSingleTimeCommands();
    ImGui_ImplVulkan_CreateFontsTexture(commandBuffer);
    vulkanContext_->getMemoryManager().endSingleTimeCommands(commandBuffer);
    
    ImGui_ImplVulkan_DestroyFontUploadObjects();
    
    initialized_ = true;
}

void VulkanImGui::createDescriptorPool() {
    VkDescriptorPoolSize pool_sizes[] = {
        { VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
        { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
        { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 }
    };
    
    VkDescriptorPoolCreateInfo pool_info = {};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    pool_info.maxSets = 1000 * IM_ARRAYSIZE(pool_sizes);
    pool_info.poolSizeCount = (uint32_t)IM_ARRAYSIZE(pool_sizes);
    pool_info.pPoolSizes = pool_sizes;
    
    if (vkCreateDescriptorPool(vulkanContext_->getDevice(), &pool_info, nullptr, &imguiDescriptorPool_) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create ImGui descriptor pool!");
    }
}

void VulkanImGui::beginFrame() {
    if (!initialized_) {
        return;
    }
    
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void VulkanImGui::endFrame(VkCommandBuffer commandBuffer) {
    if (!initialized_) {
        return;
    }
    
    ImGui::Render();
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), commandBuffer);
}

void VulkanImGui::cleanup() {
    if (initialized_) {
        ImGui_ImplVulkan_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        initialized_ = false;
    }
    
    if (imguiDescriptorPool_ != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(vulkanContext_->getDevice(), imguiDescriptorPool_, nullptr);
        imguiDescriptorPool_ = VK_NULL_HANDLE;
    }
}

} // namespace VulkanHIP
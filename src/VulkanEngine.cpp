#include "VulkanEngine.h"
#include <iostream>
#include <stdexcept>
#include <thread>
#include <chrono>
#include <memory>
#include <GLFW/glfw3.h>

// Fix the include paths
#include "VulkanContext.h"
#include "WindowManager.h"
#include "SaveManager.h"
#include "Camera.h"
#include "Grid3D.h"
#include "vulkan/resources/VulkanBufferManager.h"
#include "vulkan/resources/VulkanImageManager.h"
#include "vulkan/resources/VulkanSwapChain.h"
#include "vulkan/rendering/VulkanRenderer.h"
#include "vulkan/rendering/VoxelRenderer.h"
#include "vulkan/resources/VulkanFramebuffer.h"
#include "vulkan/ui/VulkanImGui.h"

namespace VulkanHIP {

void VulkanEngine::init() {
    try {
        // Initialize window manager (singleton pattern)
        windowManager_ = &VulkanHIP::WindowManager::getInstance();
        VulkanHIP::WindowManager::WindowConfig config;
        config.width = 800;
        config.height = 600;
        config.title = "3D Game of Life - Vulkan Edition";
        windowManager_->init(config);

        // Initialize Vulkan context (singleton pattern)
        vulkanContext_ = &VulkanHIP::VulkanContext::getInstance();
        std::vector<const char*> extensions;
        uint32_t glfwExtensionCount = 0;
        const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
        extensions.insert(extensions.end(), glfwExtensions, glfwExtensions + glfwExtensionCount);
        vulkanContext_->init(extensions);

        // Initialize memory manager
        memoryManager_ = std::make_unique<VulkanHIP::VulkanMemoryManager>(
            vulkanContext_->getDevice(), 
            vulkanContext_->getPhysicalDevice()
        );

        // Initialize save manager
        saveManager_ = std::make_unique<SaveManager>();

        // Initialize camera (Camera is not in VulkanHIP namespace)
        camera_ = std::make_unique<::Camera>(windowManager_->getWindow());
        camera_->setPosition(glm::vec3(0.0f, 0.0f, 5.0f));

        // Initialize grid
        grid_ = std::make_unique<::Grid3D>(64, 64, 64);
        grid_->initialize();

        std::cout << "VulkanEngine initialized successfully" << std::endl;

    } catch (const std::exception& e) {
        cleanup();
        throw std::runtime_error(std::string("Failed to initialize VulkanEngine: ") + e.what());
    }
}

void VulkanEngine::run() {
    try {
        while (!windowManager_->shouldClose()) {
            windowManager_->pollEvents();
            
            // Update camera
            if (camera_) {
                camera_->update(0.016f); // Assume 60fps
            }

            // Update grid simulation
            if (grid_) {
                grid_->update();
            }

            // Basic render loop would go here
            // For now just sleep to avoid spinning
            std::this_thread::sleep_for(std::chrono::milliseconds(16));
        }
    } catch (const std::exception& e) {
        std::cerr << "Error in main loop: " << e.what() << std::endl;
        throw;
    }
}

void VulkanEngine::cleanup() {
    if (vulkanContext_) {
        VkDevice device = vulkanContext_->getDevice();
        if (device != VK_NULL_HANDLE) {
            vkDeviceWaitIdle(device);
        }
    }

    // Cleanup in reverse order
    grid_.reset();
    camera_.reset(); 
    saveManager_.reset();
    memoryManager_.reset();
    vulkanContext_ = nullptr;
    
    // WindowManager is a singleton, just set pointer to nullptr
    windowManager_ = nullptr;

    std::cout << "VulkanEngine cleanup complete" << std::endl;
}

void VulkanEngine::applyEnabledDeviceFeatures(VkPhysicalDeviceFeatures& features) {
    features.samplerAnisotropy = VK_TRUE;
    features.fillModeNonSolid = VK_TRUE;
    features.wideLines = VK_TRUE;
}

// Add placeholder implementations for other required functions
void VulkanEngine::createCommandPools() {
    // Delegate to vulkanContext
    if (vulkanContext_) {
        // Implementation would go here
    }
}

void VulkanEngine::createDescriptorSetLayout() {
    // Placeholder - would be implemented by specific renderer components
}

void VulkanEngine::createGraphicsPipeline() {
    // Placeholder - would be implemented by specific renderer components  
}

void VulkanEngine::createComputePipeline() {
    // Placeholder - would be implemented by compute components
}

void VulkanEngine::updateComputePushConstants() {
    // Placeholder - would be implemented by compute components
}

void VulkanEngine::submitComputeWork() {
    // Placeholder - would be implemented by compute components
}

void VulkanEngine::waitForComputeCompletion() {
    // Placeholder - would be implemented by compute components
}

void VulkanEngine::submitComputeCommand(VkCommandBuffer commandBuffer) {
    // Placeholder - would be implemented by compute components
}

VkShaderModule VulkanEngine::createShaderModule(const std::vector<char>& code) {
    // Placeholder implementation
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = code.size();
    createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

    VkShaderModule shaderModule;
    VkDevice device = vulkanContext_ ? vulkanContext_->getDevice() : VK_NULL_HANDLE;
    if (device != VK_NULL_HANDLE) {
        if (vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create shader module!");
        }
    }
    return shaderModule;
}

} // namespace VulkanHIP
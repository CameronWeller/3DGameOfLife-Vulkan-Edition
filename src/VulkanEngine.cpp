#include "VulkanEngine.h"
#include <iostream>
#include <stdexcept>
#include <thread>
#include <chrono>
#include <memory>
#include <fstream>
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
#include "vulkan/resources/ShaderManager.h"

namespace VulkanHIP {

void VulkanEngine::init() {
    try {
        // Initialize window manager
        windowManager_ = std::make_unique<WindowManager>();
        windowManager_->init(WindowManager::WindowConfig{
            .width = 800,
            .height = 600,
            .title = "3D Game of Life - Vulkan Edition"
        });

        // Initialize Vulkan context
        vulkanContext_ = std::make_unique<VulkanContext>();
        std::vector<const char*> extensions;
        uint32_t glfwExtensionCount = 0;
        const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
        extensions.insert(extensions.end(), glfwExtensions, glfwExtensions + glfwExtensionCount);
        vulkanContext_->init(extensions);

        // Initialize memory manager
        memoryManager_ = std::make_unique<VulkanMemoryManager>(vulkanContext_.get());

        // Initialize save manager
        saveManager_ = std::make_unique<SaveManager>();

        // Initialize camera
        camera_ = std::make_unique<Camera>();
        camera_->setPosition(glm::vec3(0.0f, 0.0f, 5.0f));

        // Initialize grid
        grid_ = std::make_unique<Grid3D>(64, 64, 64);
        grid_->initialize();

        // Initialize shader manager
        shaderManager_ = std::make_unique<ShaderManager>(vulkanContext_.get());

        // Initialize image manager
        imageManager_ = std::make_unique<VulkanImageManager>(vulkanContext_.get(), memoryManager_.get());

        // Initialize swap chain
        swapChain_ = std::make_unique<VulkanSwapChain>(vulkanContext_.get(), windowManager_.get());

        // Initialize renderer
        renderer_ = std::make_unique<VulkanRenderer>(vulkanContext_.get(), swapChain_.get(), imageManager_.get());

        // Initialize voxel renderer
        voxelRenderer_ = std::make_unique<VoxelRenderer>(vulkanContext_.get(), swapChain_.get(), imageManager_.get());

        // Initialize framebuffer
        framebuffer_ = std::make_unique<VulkanFramebuffer>(vulkanContext_.get(), swapChain_.get());

        // Initialize compute
        compute_ = std::make_unique<VulkanCompute>(vulkanContext_.get(), memoryManager_.get());

        // Initialize ImGui
        imGui_ = std::make_unique<VulkanImGui>(vulkanContext_.get(), swapChain_.get());

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

            // Draw frame
            drawFrame();
        }
    } catch (const std::exception& e) {
        std::cerr << "Error in main loop: " << e.what() << std::endl;
        throw;
    }
}

void VulkanEngine::cleanup() {
    if (vulkanContext_) {
        vkDeviceWaitIdle(vulkanContext_->getDevice());
    }

    // Cleanup in reverse order of initialization
    imGui_.reset();
    compute_.reset();
    framebuffer_.reset();
    voxelRenderer_.reset();
    renderer_.reset();
    swapChain_.reset();
    imageManager_.reset();
    shaderManager_.reset();
    grid_.reset();
    camera_.reset();
    saveManager_.reset();
    memoryManager_.reset();
    vulkanContext_.reset();
    windowManager_.reset();

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

void VulkanEngine::drawFrame() {
    if (!vulkanContext_ || !windowManager_ || !grid_) return;
    
    // Update camera
    if (camera_) {
        camera_->update(0.016f); // ~60fps
    }
    
    // Update grid simulation
    if (grid_) {
        grid_->update();
    }
    
    // Begin frame
    VkCommandBuffer commandBuffer = vulkanContext_->beginSingleTimeCommands();
    
    // Record commands
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    
    VK_CHECK(vkBeginCommandBuffer(commandBuffer, &beginInfo));
    
    // Clear color
    VkClearValue clearColor = {{{0.0f, 0.0f, 0.0f, 1.0f}}};
    
    // Begin render pass
    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = swapChain_->getRenderPass();
    renderPassInfo.framebuffer = swapChain_->getCurrentFramebuffer();
    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = windowManager_->getWindowExtent();
    renderPassInfo.clearValueCount = 1;
    renderPassInfo.pClearValues = &clearColor;
    
    vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
    
    // Render grid
    if (voxelRenderer_) {
        voxelRenderer_->renderGrid(commandBuffer, camera_->getViewMatrix(), camera_->getProjectionMatrix());
    }
    
    // End render pass
    vkCmdEndRenderPass(commandBuffer);
    
    VK_CHECK(vkEndCommandBuffer(commandBuffer));
    
    // Submit command buffer
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;
    
    VK_CHECK(vkQueueSubmit(vulkanContext_->getGraphicsQueue(), 1, &submitInfo, VK_NULL_HANDLE));
    
    // Present
    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &swapChain_->getSwapChain();
    presentInfo.pImageIndices = &swapChain_->getCurrentImageIndex();
    
    VK_CHECK(vkQueuePresentKHR(vulkanContext_->getPresentQueue(), &presentInfo));
    
    // Wait for device idle (temporary, will be optimized later)
    vkDeviceWaitIdle(vulkanContext_->getDevice());
}

uint32_t VulkanEngine::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(vulkanContext_->getPhysicalDevice(), &memProperties);

    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }

    throw std::runtime_error("Failed to find suitable memory type!");
}

void VulkanEngine::renderPatternPreview(const std::string& patternPath) {
    // Placeholder implementation for pattern preview rendering
    // This would load a pattern and render it to a preview texture
    try {
        // Load pattern data from file
        // Create preview render target
        // Render pattern to preview
        std::cout << "Rendering pattern preview for: " << patternPath << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Failed to render pattern preview: " << e.what() << std::endl;
    }
}

void VulkanEngine::saveImageToFile(const std::string& filename) {
    // Placeholder implementation for saving rendered image to file
    // This would capture the current frame buffer and save it as an image
    try {
        // Get current frame buffer data
        // Convert to image format
        // Write to file
        std::cout << "Saving image to: " << filename << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Failed to save image to file: " << e.what() << std::endl;
    }
}

VkCommandBuffer VulkanEngine::beginSingleTimeCommands() {
    if (!vulkanContext_ || !memoryManager_) {
        throw std::runtime_error("Vulkan components not initialized");
    }
    
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = memoryManager_->getCommandPool();
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    if (vkAllocateCommandBuffers(vulkanContext_->getDevice(), &allocInfo, &commandBuffer) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate command buffer!");
    }

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
        throw std::runtime_error("Failed to begin command buffer!");
    }

    return commandBuffer;
}

void VulkanEngine::endSingleTimeCommands(VkCommandBuffer commandBuffer) {
    if (!vulkanContext_ || !memoryManager_) {
        throw std::runtime_error("Vulkan components not initialized");
    }
    
    if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
        throw std::runtime_error("Failed to end command buffer!");
    }

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    VkQueue graphicsQueue = vulkanContext_->getGraphicsQueue();
    if (vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE) != VK_SUCCESS) {
        throw std::runtime_error("Failed to submit command buffer!");
    }
    
    if (vkQueueWaitIdle(graphicsQueue) != VK_SUCCESS) {
        throw std::runtime_error("Failed to wait for queue idle!");
    }

    vkFreeCommandBuffers(vulkanContext_->getDevice(), memoryManager_->getCommandPool(), 1, &commandBuffer);
}

std::vector<char> VulkanEngine::readFile(const std::string& filename) {
    std::ifstream file(filename, std::ios::ate | std::ios::binary);

    if (!file.is_open()) {
        throw std::runtime_error("Failed to open file: " + filename);
    }

    size_t fileSize = static_cast<size_t>(file.tellg());
    std::vector<char> buffer(fileSize);

    file.seekg(0);
    file.read(buffer.data(), fileSize);
    file.close();

    return buffer;
}

} // namespace VulkanHIP
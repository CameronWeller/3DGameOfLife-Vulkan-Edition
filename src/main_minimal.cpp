// Minimal Vulkan Application - Uses only working components
#include <iostream>
#include <stdexcept>
#include <memory>

// Core engine includes
#include "core/Window.h"
#include "core/InputManager.h"

// Vulkan resources includes
#include "vulkan/VulkanContext.h"
#include "vulkan/VulkanInstance.h"
#include "vulkan/VulkanDevice.h"
#include "vulkan/VulkanSwapchain.h"
#include "vulkan/VulkanCommandBuffer.h"
#include "vulkan/VulkanFramebuffer.h"
#include "vulkan/VulkanRenderPass.h"
#include "vulkan/VulkanPipeline.h"

// Memory management
#include "memory/MemoryPool.h"
#include "memory/ResourceManager.h"

// Utilities
#include "utils/Logger.h"
#include "utils/Config.h"

using namespace VulkanHIP;

class MinimalVulkanApp {
public:
    MinimalVulkanApp() {
        Logger::info("Starting Minimal Vulkan Application");
    }

    ~MinimalVulkanApp() {
        cleanup();
    }

    void run() {
        try {
            initWindow();
            initVulkan();
            mainLoop();
        }
        catch (const std::exception& e) {
            Logger::error("Application error: {}", e.what());
            throw;
        }
    }

private:
    // Window
    std::unique_ptr<Window> window;
    
    // Vulkan core components
    std::shared_ptr<VulkanContext> vulkanContext;
    std::unique_ptr<VulkanInstance> instance;
    std::unique_ptr<VulkanDevice> device;
    std::unique_ptr<VulkanSwapchain> swapchain;
    std::unique_ptr<VulkanRenderPass> renderPass;
    std::unique_ptr<VulkanPipeline> pipeline;
    
    // Command buffers
    std::vector<std::unique_ptr<VulkanCommandBuffer>> commandBuffers;
    
    // Memory management
    std::unique_ptr<MemoryPool> memoryPool;
    
    void initWindow() {
        Logger::info("Initializing window...");
        
        WindowConfig windowConfig;
        windowConfig.width = 1280;
        windowConfig.height = 720;
        windowConfig.title = "Vulkan HIP Engine - Minimal Build";
        windowConfig.vsync = true;
        
        window = std::make_unique<Window>(windowConfig);
        
        Logger::info("Window created: {}x{}", windowConfig.width, windowConfig.height);
    }
    
    void initVulkan() {
        Logger::info("Initializing Vulkan...");
        
        // Create Vulkan context
        vulkanContext = std::make_shared<VulkanContext>();
        
        // Create instance
        VulkanInstanceConfig instanceConfig;
        instanceConfig.appName = "Minimal Vulkan App";
        instanceConfig.engineName = "VulkanHIP Engine";
        instanceConfig.enableValidation = true;
        
        instance = std::make_unique<VulkanInstance>(instanceConfig);
        vulkanContext->instance = instance->getInstance();
        
        // Create device
        VulkanDeviceConfig deviceConfig;
        deviceConfig.enableValidation = true;
        deviceConfig.preferredDeviceType = VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU;
        
        device = std::make_unique<VulkanDevice>(*instance, deviceConfig);
        vulkanContext->device = device->getDevice();
        vulkanContext->physicalDevice = device->getPhysicalDevice();
        vulkanContext->graphicsQueue = device->getGraphicsQueue();
        vulkanContext->presentQueue = device->getPresentQueue();
        
        // Create swapchain
        VulkanSwapchainConfig swapchainConfig;
        swapchainConfig.width = window->getWidth();
        swapchainConfig.height = window->getHeight();
        swapchainConfig.vsync = true;
        
        swapchain = std::make_unique<VulkanSwapchain>(
            *device, 
            window->getSurface(instance->getInstance()),
            swapchainConfig
        );
        
        // Create render pass
        renderPass = std::make_unique<VulkanRenderPass>(*device, swapchain->getImageFormat());
        
        // Create graphics pipeline (minimal - just clear color)
        VulkanPipelineConfig pipelineConfig;
        pipelineConfig.vertexShaderPath = "shaders/minimal.vert.spv";
        pipelineConfig.fragmentShaderPath = "shaders/minimal.frag.spv";
        pipelineConfig.renderPass = renderPass->getRenderPass();
        pipelineConfig.extent = swapchain->getExtent();
        
        pipeline = std::make_unique<VulkanPipeline>(*device, pipelineConfig);
        
        // Create command buffers
        createCommandBuffers();
        
        // Initialize memory pool
        MemoryPoolConfig memoryConfig;
        memoryConfig.deviceMemorySize = 256 * 1024 * 1024; // 256MB
        memoryConfig.hostMemorySize = 64 * 1024 * 1024;    // 64MB
        
        memoryPool = std::make_unique<MemoryPool>(*device, memoryConfig);
        
        Logger::info("Vulkan initialization complete");
    }
    
    void createCommandBuffers() {
        commandBuffers.clear();
        
        VulkanCommandBufferConfig cmdConfig;
        cmdConfig.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        cmdConfig.count = swapchain->getImageCount();
        
        for (size_t i = 0; i < swapchain->getImageCount(); i++) {
            auto cmdBuffer = std::make_unique<VulkanCommandBuffer>(*device, cmdConfig);
            
            // Record command buffer
            cmdBuffer->begin();
            
            // Begin render pass
            VkRenderPassBeginInfo renderPassInfo{};
            renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            renderPassInfo.renderPass = renderPass->getRenderPass();
            renderPassInfo.framebuffer = swapchain->getFramebuffer(i);
            renderPassInfo.renderArea.offset = {0, 0};
            renderPassInfo.renderArea.extent = swapchain->getExtent();
            
            // Clear to blue color
            VkClearValue clearColor = {{{0.1f, 0.2f, 0.4f, 1.0f}}};
            renderPassInfo.clearValueCount = 1;
            renderPassInfo.pClearValues = &clearColor;
            
            vkCmdBeginRenderPass(cmdBuffer->getCommandBuffer(), &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
            
            // Bind pipeline
            vkCmdBindPipeline(cmdBuffer->getCommandBuffer(), VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->getPipeline());
            
            // End render pass
            vkCmdEndRenderPass(cmdBuffer->getCommandBuffer());
            
            cmdBuffer->end();
            
            commandBuffers.push_back(std::move(cmdBuffer));
        }
    }
    
    void mainLoop() {
        Logger::info("Entering main loop...");
        
        while (!window->shouldClose()) {
            window->pollEvents();
            drawFrame();
        }
        
        // Wait for device to finish
        vkDeviceWaitIdle(device->getDevice());
    }
    
    void drawFrame() {
        // Acquire next image
        uint32_t imageIndex;
        VkResult result = swapchain->acquireNextImage(imageIndex);
        
        if (result == VK_ERROR_OUT_OF_DATE_KHR) {
            recreateSwapchain();
            return;
        }
        
        // Submit command buffer
        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        
        VkSemaphore waitSemaphores[] = {swapchain->getImageAvailableSemaphore()};
        VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = waitSemaphores;
        submitInfo.pWaitDstStageMask = waitStages;
        
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffers[imageIndex]->getCommandBuffer();
        
        VkSemaphore signalSemaphores[] = {swapchain->getRenderFinishedSemaphore()};
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = signalSemaphores;
        
        vkQueueSubmit(device->getGraphicsQueue(), 1, &submitInfo, VK_NULL_HANDLE);
        
        // Present
        swapchain->present(imageIndex);
    }
    
    void recreateSwapchain() {
        // Handle window resize
        int width = 0, height = 0;
        while (width == 0 || height == 0) {
            width = window->getWidth();
            height = window->getHeight();
            window->waitEvents();
        }
        
        vkDeviceWaitIdle(device->getDevice());
        
        // Recreate swapchain
        VulkanSwapchainConfig config;
        config.width = width;
        config.height = height;
        config.vsync = true;
        
        swapchain->recreate(config);
        
        // Recreate command buffers
        createCommandBuffers();
    }
    
    void cleanup() {
        Logger::info("Cleaning up resources...");
        
        if (device) {
            vkDeviceWaitIdle(device->getDevice());
        }
        
        // Cleanup in reverse order
        commandBuffers.clear();
        pipeline.reset();
        renderPass.reset();
        swapchain.reset();
        memoryPool.reset();
        device.reset();
        instance.reset();
        window.reset();
        
        Logger::info("Cleanup complete");
    }
};

int main() {
    try {
        // Initialize logger
        LoggerConfig logConfig;
        logConfig.logLevel = LogLevel::Debug;
        logConfig.logToFile = true;
        logConfig.logFilePath = "vulkan_minimal.log";
        Logger::init(logConfig);
        
        // Load configuration
        Config::load("config/app.json");
        
        // Run application
        MinimalVulkanApp app;
        app.run();
        
        Logger::info("Application exited successfully");
        return 0;
    }
    catch (const std::exception& e) {
        Logger::error("Fatal error: {}", e.what());
        return 1;
    }
} 
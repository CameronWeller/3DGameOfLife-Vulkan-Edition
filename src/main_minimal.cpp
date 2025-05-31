// Minimal Vulkan Application - Uses only confirmed working components
#include <iostream>
#include <stdexcept>
#include <memory>
#include <chrono>
#include <thread>
#include <GLFW/glfw3.h>  // Add GLFW header for extension functions

// Only include headers we know exist
#include "WindowManager.h"
#include "VulkanContext.h"
#include "DeviceManager.h"
// #include "VulkanMemoryManager.h"  // Temporarily disabled due to VMA linking issues
#include "Logger.h"

using namespace VulkanHIP;

class MinimalVulkanApp {
public:
    MinimalVulkanApp() {
        std::cout << "Starting Minimal Vulkan Application" << std::endl;
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
            std::cerr << "Application error: " << e.what() << std::endl;
            throw;
        }
    }

private:
    // Use references to singletons instead of unique_ptr
    WindowManager* windowManager = nullptr;
    VulkanContext* vulkanContext = nullptr;
    // std::unique_ptr<VulkanMemoryManager> memoryManager;  // Temporarily disabled
    
    void initWindow() {
        std::cout << "Initializing window..." << std::endl;
        
        // Get singleton instance
        windowManager = &WindowManager::getInstance();
        
        // Initialize window
        WindowManager::WindowConfig config{};
        config.width = 1280;
        config.height = 720;
        config.title = "Vulkan HIP Engine - Minimal Build";
        
        windowManager->init(config);
        
        std::cout << "Window created: " << config.width << "x" << config.height << std::endl;
    }
    
    void initVulkan() {
        std::cout << "Initializing Vulkan..." << std::endl;
        
        // Get singleton instance
        vulkanContext = &VulkanContext::getInstance();
        
        // Get required GLFW extensions for surface support
        uint32_t glfwExtensionCount = 0;
        const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
        
        std::vector<const char*> extensions;
        for (uint32_t i = 0; i < glfwExtensionCount; i++) {
            extensions.push_back(glfwExtensions[i]);
            std::cout << "Adding required extension: " << glfwExtensions[i] << std::endl;
        }
        
        vulkanContext->init(extensions);
        
        // Skip memory manager for now
        // memoryManager = std::make_unique<VulkanMemoryManager>(
        //     vulkanContext->getDevice(),
        //     vulkanContext->getPhysicalDevice()
        // );
        
        std::cout << "Vulkan initialization complete" << std::endl;
    }
    
    void mainLoop() {
        std::cout << "Entering main loop..." << std::endl;
        
        while (!windowManager->shouldClose()) {
            windowManager->pollEvents();
            
            // Simple frame timing
            std::this_thread::sleep_for(std::chrono::milliseconds(16)); // ~60fps
        }
    }
    
    void cleanup() {
        std::cout << "Cleaning up resources..." << std::endl;
        
        // Cleanup in reverse order
        // memoryManager.reset();  // Temporarily disabled
        
        if (vulkanContext) {
            vulkanContext->cleanup();
        }
        
        if (windowManager) {
            windowManager->cleanup();
        }
        
        std::cout << "Cleanup complete" << std::endl;
    }
};

int main() {
    try {
        // Initialize logger if available
        std::cout << "Starting Vulkan HIP Engine - Minimal Build" << std::endl;
        
        // Run application
        MinimalVulkanApp app;
        app.run();
        
        std::cout << "Application exited successfully" << std::endl;
        return 0;
    }
    catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    }
} 
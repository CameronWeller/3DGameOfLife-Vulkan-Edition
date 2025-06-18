#include "VulkanEngine.h"
#include "WindowManager.h"
#include "Logger.h"
#include <iostream>
#include <stdexcept>
#include <chrono>
#include <thread>
#include <GLFW/glfw3.h>

using namespace VulkanHIP;

// Global control flags
bool g_shouldClose = false;

int main() {
    try {
        // Initialize logger
        Logger logger;
        logger.log(LogLevel::Info, "Starting application");

        // Initialize Vulkan engine
        VulkanEngine engine;
        engine.init();
        
        // Set up key callback
        engine.getWindowManager()->setKeyCallback([](int key, int scancode, int action, int mods) {
            if (action != GLFW_PRESS) return;
            
            switch (key) {
                case GLFW_KEY_ESCAPE:
                    g_shouldClose = true;
                    break;
            }
        });
        
        logger.log(LogLevel::Info, "Controls:\n  ESC - Exit");
        
        // Create and initialize 3D Game of Life grid
        auto grid = std::make_unique<VulkanHIP::Grid3D>(32, 32, 32);
        grid->randomize(0.3f); // 30% density for initial pattern
        
        logger.log(LogLevel::Info, "3D Game of Life grid initialized (32x32x32, 30% density)");
        
        // Main render loop
        auto lastFrameTime = std::chrono::high_resolution_clock::now();
        auto lastUpdateTime = lastFrameTime;
        
        while (!g_shouldClose && !engine.getWindowManager()->shouldClose()) {
            auto currentTime = std::chrono::high_resolution_clock::now();
            
            // Handle input and window events
            engine.getWindowManager()->pollEvents();
            
            // Update 3D Game of Life simulation every 100ms
            auto updateDuration = std::chrono::duration_cast<std::chrono::milliseconds>(
                currentTime - lastUpdateTime).count();
            if (updateDuration >= 100) { // Update at 10 FPS
                grid->update();
                
                // Update voxel renderer with new grid state
                if (engine.getVoxelRenderer()) {
                    grid->updateVoxelRenderer(*engine.getVoxelRenderer());
                }
                
                lastUpdateTime = currentTime;
            }
            
            // Render frame
            engine.drawFrame();
            
            // Cap framerate to ~60fps when idle
            auto frameDuration = std::chrono::duration_cast<std::chrono::milliseconds>(
                currentTime - lastFrameTime).count();
            
            if (frameDuration < 16) { // 16ms ~= 60fps
                std::this_thread::sleep_for(std::chrono::milliseconds(16 - frameDuration));
            }
            
            lastFrameTime = currentTime;
        }
        
        // Wait for device idle before cleanup
        vkDeviceWaitIdle(engine.getVulkanContext()->getDevice());
        
        logger.log(LogLevel::Info, "Application shutting down");
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
} 
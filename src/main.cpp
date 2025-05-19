#include "VulkanEngine.h"
#include <iostream>
#include <stdexcept>
#include <chrono>
#include <thread>
#include <GLFW/glfw3.h>

// Forward declaration of callback function
void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);

// Global control flags
bool g_shouldClose = false;

int main() {
    try {
        // Initialize Vulkan engine
        VulkanEngine engine;
        engine.init();
        
        // Set up key callback
        GLFWwindow* window = engine.getWindowManager()->getWindow();
        glfwSetKeyCallback(window, keyCallback);
        
        std::cout << "Controls:\n"
                  << "  ESC - Exit\n" << std::endl;
        
        // Main render loop
        auto lastFrameTime = std::chrono::high_resolution_clock::now();
        
        while (!g_shouldClose && !glfwWindowShouldClose(window)) {
            // Handle input and window events
            glfwPollEvents();
            
            // Render frame
            engine.drawFrame();
            
            // Cap framerate to ~60fps when idle
            auto currentTime = std::chrono::high_resolution_clock::now();
            auto frameDuration = std::chrono::duration_cast<std::chrono::milliseconds>(
                currentTime - lastFrameTime).count();
            
            if (frameDuration < 16) { // 16ms ~= 60fps
                std::this_thread::sleep_for(std::chrono::milliseconds(16 - frameDuration));
            }
            
            lastFrameTime = std::chrono::high_resolution_clock::now();
        }
        
        // Wait for device idle before cleanup
        vkDeviceWaitIdle(engine.getVulkanContext()->getDevice());
        
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}

// Callback function for keyboard input
void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    if (action != GLFW_PRESS) return;
    
    switch (key) {
        case GLFW_KEY_ESCAPE:
            g_shouldClose = true;
            break;
    }
} 
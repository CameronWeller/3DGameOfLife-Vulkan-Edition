#include "VulkanEngine.h"
#include "Logger.h"
#include <iostream>
#include <stdexcept>

using namespace VulkanHIP;

int main() {
    try {
        // Initialize logger
        Logger logger;
        logger.log(LogLevel::Info, "Starting minimal Vulkan application");

        // Get VulkanEngine instance (singleton)
        VulkanEngine* engine = VulkanEngine::getInstance();
        
        logger.log(LogLevel::Info, "Initializing Vulkan engine...");
        engine->init();
        
        logger.log(LogLevel::Info, "Running Vulkan engine...");
        engine->run();
        
        logger.log(LogLevel::Info, "Cleaning up...");
        engine->cleanup();
        
        logger.log(LogLevel::Info, "Application completed successfully");
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
} 
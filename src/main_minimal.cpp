#include "VulkanEngine.h"
#include "Logger.h"
#include <iostream>
#include <stdexcept>

using namespace VulkanHIP;

int main() {
    try {
        // Initialize logger (singleton)
        Logger& logger = Logger::getInstance();
        logger.log(Logger::LogLevel::Info, "Starting minimal Vulkan application");

        // Get VulkanEngine instance (singleton)
        VulkanEngine* engine = VulkanEngine::getInstance();
        
        logger.log(Logger::LogLevel::Info, "Initializing Vulkan engine...");
        engine->init();
        
        logger.log(Logger::LogLevel::Info, "Running Vulkan engine...");
        engine->run();
        
        logger.log(Logger::LogLevel::Info, "Cleaning up...");
        engine->cleanup();
        
        logger.log(Logger::LogLevel::Info, "Application completed successfully");
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
} 
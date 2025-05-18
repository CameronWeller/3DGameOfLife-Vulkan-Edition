#include "VulkanEngine.h"
#include <iostream>

int main() {
    try {
        VulkanEngine engine;
        engine.init();
        engine.run();
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
} 
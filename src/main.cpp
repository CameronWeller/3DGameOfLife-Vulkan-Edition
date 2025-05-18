#include "VulkanEngine.h"
#include <iostream>
#include <stdexcept>

int main() {
    try {
        VulkanEngine engine;
        engine.init();
        engine.run();
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
} 
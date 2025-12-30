#include "SimplePatternLoader.h"
#include <fstream>
#include <random>
#include <algorithm>

namespace VulkanHIP {

Pattern3D SimplePatternLoader::loadBinary(const std::string& filename) {
    Pattern3D pattern;
    std::ifstream file(filename, std::ios::binary);
    
    if (!file.is_open()) {
        return pattern; // Return empty pattern on error
    }
    
    // Read dimensions
    file.read(reinterpret_cast<char*>(&pattern.width), sizeof(uint32_t));
    file.read(reinterpret_cast<char*>(&pattern.height), sizeof(uint32_t));
    file.read(reinterpret_cast<char*>(&pattern.depth), sizeof(uint32_t));
    
    // Read data
    uint32_t size = pattern.width * pattern.height * pattern.depth;
    pattern.data.resize(size);
    file.read(reinterpret_cast<char*>(pattern.data.data()), size * sizeof(uint32_t));
    
    return pattern;
}

bool SimplePatternLoader::saveBinary(const Pattern3D& pattern, const std::string& filename) {
    std::ofstream file(filename, std::ios::binary);
    
    if (!file.is_open()) {
        return false;
    }
    
    // Write dimensions
    file.write(reinterpret_cast<const char*>(&pattern.width), sizeof(uint32_t));
    file.write(reinterpret_cast<const char*>(&pattern.height), sizeof(uint32_t));
    file.write(reinterpret_cast<const char*>(&pattern.depth), sizeof(uint32_t));
    
    // Write data
    file.write(reinterpret_cast<const char*>(pattern.data.data()), pattern.data.size() * sizeof(uint32_t));
    
    return true;
}

Pattern3D SimplePatternLoader::createGlider3D() {
    Pattern3D pattern(5, 5, 5);
    
    // Create a simple 3D glider-like pattern
    pattern.setCell(1, 1, 1, true);
    pattern.setCell(2, 1, 1, true);
    pattern.setCell(3, 1, 1, true);
    pattern.setCell(2, 2, 1, true);
    pattern.setCell(1, 2, 2, true);
    
    return pattern;
}

Pattern3D SimplePatternLoader::createBlock3D(uint32_t size) {
    Pattern3D pattern(size, size, size);
    
    // Fill the block
    for (uint32_t x = 0; x < size; ++x) {
        for (uint32_t y = 0; y < size; ++y) {
            for (uint32_t z = 0; z < size; ++z) {
                pattern.setCell(x, y, z, true);
            }
        }
    }
    
    return pattern;
}

Pattern3D SimplePatternLoader::createRandom(uint32_t width, uint32_t height, uint32_t depth, float density) {
    Pattern3D pattern(width, height, depth);
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> dis(0.0f, 1.0f);
    
    for (uint32_t x = 0; x < width; ++x) {
        for (uint32_t y = 0; y < height; ++y) {
            for (uint32_t z = 0; z < depth; ++z) {
                if (dis(gen) < density) {
                    pattern.setCell(x, y, z, true);
                }
            }
        }
    }
    
    return pattern;
}

Pattern3D SimplePatternLoader::centerPattern(const Pattern3D& pattern, uint32_t targetWidth, uint32_t targetHeight, uint32_t targetDepth) {
    Pattern3D centered(targetWidth, targetHeight, targetDepth);
    
    uint32_t offsetX = (targetWidth - pattern.width) / 2;
    uint32_t offsetY = (targetHeight - pattern.height) / 2;
    uint32_t offsetZ = (targetDepth - pattern.depth) / 2;
    
    for (uint32_t x = 0; x < pattern.width; ++x) {
        for (uint32_t y = 0; y < pattern.height; ++y) {
            for (uint32_t z = 0; z < pattern.depth; ++z) {
                if (pattern.getCell(x, y, z)) {
                    centered.setCell(offsetX + x, offsetY + y, offsetZ + z, true);
                }
            }
        }
    }
    
    return centered;
}

} // namespace VulkanHIP


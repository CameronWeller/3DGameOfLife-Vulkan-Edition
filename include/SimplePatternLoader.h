#pragma once

#include <vector>
#include <string>
#include <cstdint>

namespace VulkanHIP {

struct Pattern3D {
    uint32_t width;
    uint32_t height;
    uint32_t depth;
    std::vector<uint32_t> data; // 1 = alive, 0 = dead
    
    Pattern3D() : width(0), height(0), depth(0) {}
    Pattern3D(uint32_t w, uint32_t h, uint32_t d) 
        : width(w), height(h), depth(d), data(w * h * d, 0) {}
    
    bool getCell(uint32_t x, uint32_t y, uint32_t z) const {
        if (x >= width || y >= height || z >= depth) return false;
        uint32_t index = z * width * height + y * width + x;
        return index < data.size() && data[index] == 1;
    }
    
    void setCell(uint32_t x, uint32_t y, uint32_t z, bool alive) {
        if (x >= width || y >= height || z >= depth) return;
        uint32_t index = z * width * height + y * width + x;
        if (index < data.size()) {
            data[index] = alive ? 1 : 0;
        }
    }
};

class SimplePatternLoader {
public:
    // Load pattern from binary file
    static Pattern3D loadBinary(const std::string& filename);
    
    // Save pattern to binary file
    static bool saveBinary(const Pattern3D& pattern, const std::string& filename);
    
    // Create default patterns
    static Pattern3D createGlider3D();
    static Pattern3D createBlock3D(uint32_t size = 3);
    static Pattern3D createRandom(uint32_t width, uint32_t height, uint32_t depth, float density = 0.3f);
    
    // Center pattern in a larger grid
    static Pattern3D centerPattern(const Pattern3D& pattern, uint32_t targetWidth, uint32_t targetHeight, uint32_t targetDepth);
};

} // namespace VulkanHIP


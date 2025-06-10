#include "PatternManager.h"
#include "Grid3D.h"
#include <fstream>
#include <filesystem>
#include <algorithm>
#include <stdexcept>
#include <random>
#include <iostream>

namespace PatternManager {

namespace fs = std::filesystem;

std::string getPatternFileExtension() {
    return ".3dlife";
}

bool validatePattern(const Pattern& pattern) {
    if (pattern.width == 0 || pattern.height == 0 || pattern.depth == 0) {
        return false;
    }
    
    if (pattern.cells.size() != pattern.width * pattern.height * pattern.depth) {
        return false;
    }
    
    return true;
}

std::string getPatternDirectory() {
    // Get the executable directory and create a patterns subdirectory
    fs::path exePath = fs::current_path();
    fs::path patternDir = exePath / "patterns";
    
    if (!fs::exists(patternDir)) {
        fs::create_directory(patternDir);
    }
    
    return patternDir.string();
}

bool savePattern(const std::string& filename, const Pattern& pattern) {
    if (!validatePattern(pattern)) {
        return false;
    }
    
    std::ofstream file(filename, std::ios::binary);
    if (!file) {
        return false;
    }
    
    // Write header
    PatternFileHeader header;
    header.version = PATTERN_FILE_VERSION;
    header.nameLength = static_cast<uint32_t>(pattern.name.length());
    header.descriptionLength = static_cast<uint32_t>(pattern.description.length());
    header.width = pattern.width;
    header.height = pattern.height;
    header.depth = pattern.depth;
    header.ruleSet = pattern.ruleSet;
    header.boundaryType = static_cast<uint32_t>(pattern.boundaryType);
    header.dataSize = static_cast<uint32_t>(pattern.cells.size());
    
    file.write(reinterpret_cast<const char*>(&header), sizeof(header));
    
    // Write strings
    file.write(pattern.name.c_str(), header.nameLength);
    file.write(pattern.description.c_str(), header.descriptionLength);
    
    // Write cell data
    file.write(reinterpret_cast<const char*>(pattern.cells.data()), pattern.cells.size());
    
    return file.good();
}

std::unique_ptr<Pattern> loadPattern(const std::string& filename) {
    std::ifstream file(filename, std::ios::binary);
    if (!file) {
        return nullptr;
    }
    
    // Read header
    PatternFileHeader header;
    file.read(reinterpret_cast<char*>(&header), sizeof(header));
    
    if (header.version != PATTERN_FILE_VERSION) {
        return nullptr;
    }
    
    // Read strings
    std::string name;
    name.resize(header.nameLength);
    std::string description;
    description.resize(header.descriptionLength);
    
    file.read(name.data(), header.nameLength);
    file.read(description.data(), header.descriptionLength);
    
    // Read cell data
    std::vector<bool> cells(header.dataSize);
    // Note: std::vector<bool> doesn't have data() method, need to read differently
    std::vector<char> tempData(header.dataSize);
    file.read(tempData.data(), header.dataSize);
    for (size_t i = 0; i < header.dataSize; ++i) {
        cells[i] = tempData[i] != 0;
    }
    
    // Create pattern
    auto pattern = std::make_unique<Pattern>(
        name, description,
        header.width, header.height, header.depth,
        cells,
        header.ruleSet,
        static_cast<GameRules::BoundaryType>(header.boundaryType)
    );
    
    if (!validatePattern(*pattern)) {
        return nullptr;
    }
    
    return pattern;
}

std::vector<Pattern> loadPatternDirectory(const std::string& directory) {
    std::vector<Pattern> patterns;
    
    for (const auto& entry : fs::directory_iterator(directory)) {
        if (entry.path().extension() == getPatternFileExtension()) {
            if (auto pattern = loadPattern(entry.path().string())) {
                patterns.push_back(std::move(*pattern));
            }
        }
    }
    
    return patterns;
}

std::vector<Pattern> getBuiltInPatterns() {
    std::vector<Pattern> patterns;
    
    // 3D Glider
    {
        std::vector<bool> cells(5 * 5 * 5, false);
        // Create a simple 3D glider pattern
        cells[getIndex(2, 2, 2, 5, 5)] = true;
        cells[getIndex(3, 2, 2, 5, 5)] = true;
        cells[getIndex(4, 2, 2, 5, 5)] = true;
        cells[getIndex(4, 3, 2, 5, 5)] = true;
        cells[getIndex(3, 4, 2, 5, 5)] = true;
        
        patterns.emplace_back(
            "3D Glider",
            "A simple 3D glider pattern that moves diagonally",
            5, 5, 5,
            cells,
            0,  // Use index 0 for RULE_5766
            0   // boundary type
        );
    }
    
    // 3D Block
    {
        std::vector<bool> cells(3 * 3 * 3, false);
        // Create a 2x2x2 block
        for (int z = 0; z < 2; z++) {
            for (int y = 0; y < 2; y++) {
                for (int x = 0; x < 2; x++) {
                    cells[getIndex(x, y, z, 3, 3)] = true;
                }
            }
        }
        
        patterns.emplace_back(
            "3D Block",
            "A stable 2x2x2 block pattern",
            3, 3, 3,
            cells,
            0,  // Use index 0 for RULE_5766
            0   // boundary type
        );
    }

    // 3D Pulsar
    {
        std::vector<bool> cells(7 * 7 * 7, false);
        // Create a 3D pulsar pattern that oscillates
        for (int z = 2; z < 5; z++) {
            for (int y = 2; y < 5; y++) {
                for (int x = 2; x < 5; x++) {
                    if ((x == 2 || x == 4) && (y == 2 || y == 4) && (z == 2 || z == 4)) {
                        cells[getIndex(x, y, z, 7, 7)] = true;
                    }
                }
            }
        }
        
        patterns.emplace_back(
            "3D Pulsar",
            "A 3D oscillating pattern that pulses between states",
            7, 7, 7,
            cells,
            6,  // Use index 6 for RULE_4556 (oscillator)
            0   // boundary type
        );
    }

    // 3D Spaceship
    {
        std::vector<bool> cells(6 * 6 * 6, false);
        // Create a 3D spaceship pattern
        // Core
        cells[getIndex(2, 2, 2, 6, 6)] = true;
        cells[getIndex(3, 2, 2, 6, 6)] = true;
        cells[getIndex(2, 3, 2, 6, 6)] = true;
        cells[getIndex(3, 3, 2, 6, 6)] = true;
        // Wings
        cells[getIndex(1, 2, 3, 6, 6)] = true;
        cells[getIndex(4, 2, 3, 6, 6)] = true;
        cells[getIndex(2, 1, 3, 6, 6)] = true;
        cells[getIndex(3, 1, 3, 6, 6)] = true;
        cells[getIndex(2, 4, 3, 6, 6)] = true;
        cells[getIndex(3, 4, 3, 6, 6)] = true;
        
        patterns.emplace_back(
            "3D Spaceship",
            "A complex 3D spaceship pattern that moves through space",
            6, 6, 6,
            cells,
            2,  // Use index 2 for RULE_2333 (growth)
            0   // boundary type
        );
    }

    // 3D Crystal
    {
        std::vector<bool> cells(8 * 8 * 8, false);
        // Create a crystal-like pattern that grows
        for (int z = 3; z < 5; z++) {
            for (int y = 3; y < 5; y++) {
                for (int x = 3; x < 5; x++) {
                    cells[getIndex(x, y, z, 8, 8)] = true;
                }
            }
        }
        // Add crystal branches
        cells[getIndex(2, 3, 3, 8, 8)] = true;
        cells[getIndex(5, 3, 3, 8, 8)] = true;
        cells[getIndex(3, 2, 3, 8, 8)] = true;
        cells[getIndex(3, 5, 3, 8, 8)] = true;
        cells[getIndex(3, 3, 2, 8, 8)] = true;
        cells[getIndex(3, 3, 5, 8, 8)] = true;
        
        patterns.emplace_back(
            "3D Crystal",
            "A growing crystal-like pattern that expands outward",
            8, 8, 8,
            cells,
            3,  // Use index 3 for RULE_3444 (stable growth)
            0   // boundary type
        );
    }

    // 3D Random Soup
    {
        std::vector<bool> cells(10 * 10 * 10, false);
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<float> dis(0.0f, 1.0f);
        
        // Create a random pattern with 30% density
        for (size_t i = 0; i < cells.size(); i++) {
            cells[i] = dis(gen) < 0.3f;
        }
        
        patterns.emplace_back(
            "3D Random Soup",
            "A random pattern that often leads to interesting emergent behavior",
            10, 10, 10,
            cells,
            0,  // Use index 0 for RULE_5766 (classic)
            0   // boundary type
        );
    }
    
    return patterns;
}

uint32_t getIndex(uint32_t x, uint32_t y, uint32_t z, uint32_t width, uint32_t height) {
    return z * width * height + y * width + x;
}

} // namespace PatternManager 
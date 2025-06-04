#pragma once

#include <string>
#include <vector>
#include <memory>
#include <filesystem>
#include <fstream>
#include <ctime>
#include <glm/glm.hpp>
#include "Grid3D.h"
#include "GameRules.h"

namespace PatternManager {

struct Pattern {
    std::string name;
    std::string description;
    std::string author;
    std::string version;
    uint32_t ruleSet;
    glm::uvec3 dimensions;
    std::vector<bool> data;
    uint32_t boundaryType;
    
    // Metadata
    std::time_t creationTime;
    std::time_t modificationTime;
    uint32_t population;
    uint32_t generation;
    std::vector<std::string> tags;

    Pattern(const std::string& n, const std::string& desc,
            uint32_t w, uint32_t h, uint32_t d,
            const std::vector<bool>& c,
            uint32_t rs, uint32_t bt,
            uint32_t pop = 0, uint32_t gen = 0)
        : name(n), description(desc), 
          ruleSet(rs), dimensions(w, h, d), data(c), boundaryType(bt),
          population(pop), generation(gen) {
        creationTime = std::time(nullptr);
        modificationTime = creationTime;
    }
    
    // Alternative constructor for backward compatibility
    Pattern(const std::string& n, const std::string& desc,
            const std::string& author, const std::string& version,
            uint32_t ruleSet, const glm::uvec3& dims,
            const std::vector<bool>& c,
            uint32_t pop = 0, uint32_t gen = 0)
        : name(n), description(desc), author(author), version(version),
          ruleSet(ruleSet), dimensions(dims), data(c), boundaryType(0),
          population(pop), generation(gen) {
        creationTime = std::time(nullptr);
        modificationTime = creationTime;
    }
};

// Pattern file format version
constexpr uint32_t PATTERN_FILE_VERSION = 1;

// Pattern file header structure
struct PatternFileHeader {
    uint32_t version;
    uint32_t width;
    uint32_t height;
    uint32_t depth;
    uint32_t ruleSet;
    uint32_t boundaryType;
    uint32_t nameLength;
    uint32_t descriptionLength;
    uint32_t dataSize;
    char name[64];
    char description[256];
    char author[64];
    char version_str[16];
};

// Pattern management functions
bool savePattern(const std::string& filename, const Pattern& pattern);
std::unique_ptr<Pattern> loadPattern(const std::string& filename);
std::vector<Pattern> loadPatternDirectory(const std::string& directory);

// Helper functions
std::string getPatternFileExtension();
bool validatePattern(const Pattern& pattern);
std::string getPatternDirectory();

// Built-in patterns
std::vector<Pattern> getBuiltInPatterns();

// Utility functions
uint32_t getIndex(uint32_t x, uint32_t y, uint32_t z, uint32_t width, uint32_t height);

// Internal helper functions
bool validatePatternFile(const std::filesystem::path& path);
void loadBuiltInPatterns();

} // namespace PatternManager 
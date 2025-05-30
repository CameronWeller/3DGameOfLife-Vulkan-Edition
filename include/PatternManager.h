#pragma once

#include <string>
#include <vector>
#include <memory>
#include "Grid3D.h"
#include "GameRules.h"

namespace PatternManager {

struct Pattern {
    std::string name;
    std::string description;
    uint32_t width;
    uint32_t height;
    uint32_t depth;
    std::vector<bool> cells;
    std::string ruleSetName;  // Name of the rule set (e.g., "5766", "4555")
    GameRules::BoundaryType boundaryType;
    
    Pattern(const std::string& n, const std::string& desc,
            uint32_t w, uint32_t h, uint32_t d,
            const std::vector<bool>& c,
            const std::string& rsName = "5766",  // Default to classic 5766 rule
            GameRules::BoundaryType bt = GameRules::BoundaryType::TOROIDAL)
        : name(n), description(desc),
          width(w), height(h), depth(d),
          cells(c), ruleSetName(rsName), boundaryType(bt) {}
};

// Pattern file format version
constexpr uint32_t PATTERN_FILE_VERSION = 1;

// Pattern file header structure
struct PatternFileHeader {
    uint32_t version;
    uint32_t nameLength;
    uint32_t descriptionLength;
    uint32_t width;
    uint32_t height;
    uint32_t depth;
    uint32_t ruleSetNameLength;  // Length of rule set name string
    uint32_t boundaryType;
    uint32_t dataSize;
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

} // namespace PatternManager 
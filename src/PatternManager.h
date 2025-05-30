#pragma once

#include <string>
#include <vector>
#include <memory>
#include "Grid3D.h"

namespace PatternManager {

struct Pattern {
    std::string name;
    std::string description;
    uint32_t width;
    uint32_t height;
    uint32_t depth;
    std::vector<bool> cells;
    uint32_t ruleSet;  // Maps to Grid3D::RuleSet
    GameRules::BoundaryType boundaryType;
    
    Pattern(const std::string& n, const std::string& desc,
            uint32_t w, uint32_t h, uint32_t d,
            const std::vector<bool>& c,
            uint32_t rs = static_cast<uint32_t>(Grid3D::RuleSet::CLASSIC),
            GameRules::BoundaryType bt = GameRules::BoundaryType::TOROIDAL)
        : name(n), description(desc),
          width(w), height(h), depth(d),
          cells(c), ruleSet(rs), boundaryType(bt) {}
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
    uint32_t ruleSet;  // Maps to Grid3D::RuleSet
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
#pragma once

#include <array>
#include <string>
#include <functional>
#include <vector>

namespace GameRules {

// Rule set definitions
struct RuleSet {
    std::string name;
    std::array<uint8_t, 2> birthRange;  // Range of neighbors for birth
    std::array<uint8_t, 2> survivalRange;  // Range of neighbors for survival
    std::string description;
    std::string category;  // Category of the rule (e.g., "Classic", "Growth", "Stable")
    
    RuleSet(const std::string& n, uint8_t bMin, uint8_t bMax, uint8_t sMin, uint8_t sMax, 
            const std::string& desc = "", const std::string& cat = "Custom")
        : name(n), birthRange{bMin, bMax}, survivalRange{sMin, sMax}, description(desc), category(cat) {}
};

// Classic 3D rules
const RuleSet RULE_5766("5766", 5, 7, 6, 6, 
    "Classic 3D rule: Born with 5-7 neighbors, survives with 6 neighbors",
    "Classic");
const RuleSet RULE_4555("4555", 4, 5, 5, 5,
    "Alternative 3D rule: Born with 4-5 neighbors, survives with 5 neighbors",
    "Classic");

// Growth rules (tend to expand)
const RuleSet RULE_2333("2333", 2, 3, 3, 3,
    "Growth rule: Born with 2-3 neighbors, survives with 3 neighbors. Tends to expand rapidly.",
    "Growth");
const RuleSet RULE_3444("3444", 3, 4, 4, 4,
    "Stable growth rule: Born with 3-4 neighbors, survives with 4 neighbors. Balanced expansion.",
    "Growth");

// Dense rules (tend to contract)
const RuleSet RULE_6777("6777", 6, 7, 7, 7,
    "Dense rule: Born with 6-7 neighbors, survives with 7 neighbors. Forms dense clusters.",
    "Dense");
const RuleSet RULE_7888("7888", 7, 8, 8, 8,
    "Very dense rule: Born with 7-8 neighbors, survives with 8 neighbors. Forms very dense structures.",
    "Dense");

// Oscillator rules (tend to form repeating patterns)
const RuleSet RULE_4556("4556", 4, 5, 5, 6,
    "Oscillator rule: Born with 4-5 neighbors, survives with 5-6 neighbors. Favors oscillating patterns.",
    "Oscillator");
const RuleSet RULE_5667("5667", 5, 6, 6, 7,
    "Complex oscillator rule: Born with 5-6 neighbors, survives with 6-7 neighbors. Complex oscillations.",
    "Oscillator");

// Helper function to check if a number is within a range
inline bool isInRange(uint8_t value, const std::array<uint8_t, 2>& range) {
    return value >= range[0] && value <= range[1];
}

// Function to determine next state based on current state and neighbor count
inline bool getNextState(bool currentState, uint8_t neighborCount, const RuleSet& rules) {
    if (currentState) {
        return isInRange(neighborCount, rules.survivalRange);
    } else {
        return isInRange(neighborCount, rules.birthRange);
    }
}

// Boundary condition types
enum class BoundaryType {
    TOROIDAL,  // Wrap around edges
    FIXED,     // Fixed boundary (always dead)
    INFINITE,  // Infinite grid (treat as dead)
    MIRROR     // Mirror boundary (reflect at edges)
};

// Get all available rule sets
inline std::vector<RuleSet> getAllRuleSets() {
    return {
        RULE_5766,
        RULE_4555,
        RULE_2333,
        RULE_3444,
        RULE_6777,
        RULE_7888,
        RULE_4556,
        RULE_5667
    };
}

// Get rule sets by category
inline std::vector<RuleSet> getRuleSetsByCategory(const std::string& category) {
    std::vector<RuleSet> result;
    for (const auto& rule : getAllRuleSets()) {
        if (rule.category == category) {
            result.push_back(rule);
        }
    }
    return result;
}

// Get rule set by name
inline const RuleSet* getRuleSetByName(const std::string& name) {
    for (const auto& rule : getAllRuleSets()) {
        if (rule.name == name) {
            return &rule;
        }
    }
    return nullptr;
}

// Get boundary type name
inline std::string getBoundaryTypeName(BoundaryType type) {
    switch (type) {
        case BoundaryType::TOROIDAL: return "Toroidal";
        case BoundaryType::FIXED: return "Fixed";
        case BoundaryType::INFINITE: return "Infinite";
        case BoundaryType::MIRROR: return "Mirror";
        default: return "Unknown";
    }
}

// Get boundary type from name
inline BoundaryType getBoundaryTypeFromName(const std::string& name) {
    if (name == "Toroidal") return BoundaryType::TOROIDAL;
    if (name == "Fixed") return BoundaryType::FIXED;
    if (name == "Infinite") return BoundaryType::INFINITE;
    if (name == "Mirror") return BoundaryType::MIRROR;
    return BoundaryType::TOROIDAL; // Default
}

// Get all available categories
inline std::vector<std::string> getAllCategories() {
    return {"Classic", "Growth", "Dense", "Oscillator", "Custom"};
}

} // namespace GameRules 
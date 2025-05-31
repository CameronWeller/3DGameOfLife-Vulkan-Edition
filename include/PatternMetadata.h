#pragma once
#include <string>
#include <vector>
#include <chrono>
#include <glm/glm.hpp>

struct PatternMetadata {
    std::string name;
    std::string description;
    std::string author;
    std::string version;
    std::string category;
    std::vector<glm::vec3> cells;
    glm::vec3 center;
    float scale;
    bool isPeriodic;
    int period;
    
    // Extended metadata for UI compatibility
    std::string ruleSet;
    glm::ivec3 gridSize{64, 64, 64};
    float voxelSize{1.0f};
    std::chrono::system_clock::time_point creationTime;
    std::chrono::system_clock::time_point modificationTime;
    size_t population{0};
    int generation{0};
    std::vector<std::string> tags;
}; 
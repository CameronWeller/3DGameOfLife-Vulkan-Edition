#pragma once
#include <string>
#include <vector>
#include <chrono>
#include <glm/glm.hpp>

struct PatternMetadata {
    std::string name;
    std::string description;
    std::string author;
    std::string category;
    std::vector<glm::vec3> cells;
    glm::vec3 center;
    float scale;
    bool isPeriodic;
    int period;
    
    // Additional fields referenced in the code
    std::string version;
    std::string ruleSet;
    glm::ivec3 gridSize;
    float voxelSize;
    std::chrono::system_clock::time_point creationTime;
    std::chrono::system_clock::time_point modificationTime;
    size_t population;
    int generation;
    std::vector<std::string> tags;
}; 
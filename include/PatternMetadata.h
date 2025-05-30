#pragma once
#include <string>
#include <vector>
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
}; 
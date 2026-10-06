#pragma once

// Lesson content for the in-game tutorial: text plus a small scene per lesson
// (rule, live cells, outlined cells, camera). No graphics or ImGui here, so the
// CPU tests (tests/PatternsTest.cpp) check every scene's claims against the rule
// reference.

#include <cstddef>
#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "life/Patterns.h"

namespace gol3d::tutorial {

// Outline colors for highlighted cells. The values are box color ids in
// shaders/life3d_boxes.vert minus FIRST_MARK_COLOR_ID.
enum class Mark { Neighbor, Born, Dies, Survives };
constexpr int FIRST_MARK_COLOR_ID = 3;

struct MarkedCell {
    PatternCell cell;
    Mark mark;
};

struct LegendEntry {
    Mark mark;
    std::string text;
};

struct Lesson {
    std::string title;
    std::vector<std::string> paragraphs;
    std::vector<LegendEntry> legend; // what each outline color means
    std::string tryThis;
    size_t rule = 0; // index into lifeRules()
    Pattern cells;
    std::vector<MarkedCell> marks;
    glm::vec3 eye{0.0f}; // camera position (the player flies)
    glm::vec3 lookAt{0.0f};
};

const std::vector<Lesson>& lessons();

// Index of the rule with this name in lifeRules(); throws if it is missing.
size_t ruleIndexNamed(const std::string& name);

// Camera angles in the game's convention (yaw 0 looks along +x, yaw 90 along +z,
// positive pitch looks up), aimed so lookAt lands left of the lesson panel.
void lookAngles(const Lesson& lesson, float& yawDegrees, float& pitchDegrees);

} // namespace gol3d::tutorial

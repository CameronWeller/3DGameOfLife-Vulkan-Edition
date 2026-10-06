#pragma once

// Player-adjustable options, stored as "key:value" lines like Minecraft's
// options.txt. Unknown keys and malformed values are ignored, so an old or
// hand-edited file never stops the game from starting.

#include <filesystem>

namespace gol3d {

struct Settings {
    // Valid ranges, shared by the loader and the settings menu.
    static constexpr float MIN_FOV = 30.0f, MAX_FOV = 110.0f;
    static constexpr int MIN_SENSITIVITY = 10, MAX_SENSITIVITY = 300;
    static constexpr int MIN_RENDER_DISTANCE = 64, MAX_RENDER_DISTANCE = 1024;
    static constexpr int MAX_GUI_SCALE = 4;
    static constexpr int MIN_SIM_BUDGET = 2, MAX_SIM_BUDGET = 40;

    float fov = 70.0f;          // vertical field of view, degrees
    int sensitivity = 100;      // mouse look, percent of 0.15 degrees per pixel
    bool invertY = false;       // mouse up looks down
    int renderDistance = 256;   // blocks; fog ends here
    int guiScale = 0;           // menu scale, 0 = automatic
    bool checkUpdates = true;   // ask GitHub for a newer release at startup
    bool smoothLighting = true; // ambient occlusion at block corners
    bool animate = true;        // births grow in and deaths shrink away at slow speeds
    int simBudget = 8;          // most milliseconds of simulation per frame (the governor's budget)
};

// Missing files give the defaults; out-of-range values are clamped.
Settings loadSettings(const std::filesystem::path& file);
// Creates the folder if needed. Failures are silent: settings are a convenience.
void saveSettings(const Settings& settings, const std::filesystem::path& file);

} // namespace gol3d

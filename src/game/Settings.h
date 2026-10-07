#pragma once

// Player-adjustable options, stored as "key:value" lines like Minecraft's
// options.txt. Unknown keys and malformed values are ignored, so an old or
// hand-edited file never stops the game from starting.

#include <filesystem>

namespace gol3d {

// What the settings menu changes and options.txt keeps.
struct Settings {
    // Valid ranges, shared by the loader and the settings menu.
    static constexpr float MIN_FOV = 30.0f;
    static constexpr float MAX_FOV = 110.0f;
    static constexpr int MIN_SENSITIVITY = 10;
    static constexpr int MAX_SENSITIVITY = 300;
    static constexpr int MIN_RENDER_DISTANCE = 64;
    static constexpr int MAX_RENDER_DISTANCE = 1024;
    static constexpr int AUTOMATIC_GUI_SCALE = 0; // pick a scale from the window size
    static constexpr int MAX_GUI_SCALE = 4;
    static constexpr int MIN_SIM_BUDGET = 2;
    static constexpr int MAX_SIM_BUDGET = 40;

    float fov = 70.0f;                  // vertical field of view, degrees
    int sensitivity = 100;              // mouse look, percent of 0.15 degrees per pixel
    bool invertY = false;               // mouse up looks down
    int renderDistance = 256;           // blocks; fog ends here
    int guiScale = AUTOMATIC_GUI_SCALE; // menu scale
    bool checkUpdates = true;           // ask GitHub for a newer release at startup
    bool smoothLighting = true;         // ambient occlusion at block corners
    bool animate = true;                // births grow in and deaths shrink away at slow speeds
    int simBudget = 8;                  // the governor's budget: ms of simulation per frame
};

// Missing files give the defaults; out-of-range values are clamped.
Settings loadSettings(const std::filesystem::path& file);
// Creates the folder if needed. Failures are silent: settings are a convenience.
void saveSettings(const Settings& settings, const std::filesystem::path& file);

} // namespace gol3d

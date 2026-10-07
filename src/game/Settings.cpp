// Reading and writing options.txt. The key names here are the file format:
// renaming one forgets that setting in every existing file.

#include "game/Settings.h"

#include <algorithm>
#include <fstream>
#include <stdexcept>
#include <string>

namespace gol3d {
namespace {

const char* boolText(bool value) {
    return value ? "true" : "false";
}

// Applies one "key:value" line; std::stoi and std::stof throw for unparsable
// numbers. A flag only changes from its default when the value spells out the
// other choice ("true" for invertY, "false" for the rest), so a garbled flag
// keeps its default.
void applySetting(Settings& settings, const std::string& key, const std::string& value) {
    if (key == "fov") {
        settings.fov = std::clamp(std::stof(value), Settings::MIN_FOV, Settings::MAX_FOV);
    } else if (key == "sensitivity") {
        settings.sensitivity =
            std::clamp(std::stoi(value), Settings::MIN_SENSITIVITY, Settings::MAX_SENSITIVITY);
    } else if (key == "invertY") {
        settings.invertY = value == "true";
    } else if (key == "renderDistance") {
        settings.renderDistance = std::clamp(std::stoi(value), Settings::MIN_RENDER_DISTANCE,
                                             Settings::MAX_RENDER_DISTANCE);
    } else if (key == "guiScale") {
        settings.guiScale =
            std::clamp(std::stoi(value), Settings::AUTOMATIC_GUI_SCALE, Settings::MAX_GUI_SCALE);
    } else if (key == "checkUpdates") {
        settings.checkUpdates = value != "false";
    } else if (key == "smoothLighting") {
        settings.smoothLighting = value != "false";
    } else if (key == "animate") {
        settings.animate = value != "false";
    } else if (key == "simBudget") {
        settings.simBudget =
            std::clamp(std::stoi(value), Settings::MIN_SIM_BUDGET, Settings::MAX_SIM_BUDGET);
    }
}

} // namespace

Settings loadSettings(const std::filesystem::path& file) {
    Settings settings;
    std::ifstream in(file);
    std::string line;
    while (std::getline(in, line)) {
        size_t colon = line.find(':');
        if (colon == std::string::npos) continue;
        try {
            applySetting(settings, line.substr(0, colon), line.substr(colon + 1));
        } catch (const std::exception&) { // NOLINT(bugprone-empty-catch)
            // A malformed number keeps the default.
        }
    }
    return settings;
}

void saveSettings(const Settings& settings, const std::filesystem::path& file) {
    std::error_code ignored;
    std::filesystem::create_directories(file.parent_path(), ignored);
    std::ofstream out(file);
    out << "fov:" << settings.fov << "\n"
        << "sensitivity:" << settings.sensitivity << "\n"
        << "invertY:" << boolText(settings.invertY) << "\n"
        << "renderDistance:" << settings.renderDistance << "\n"
        << "guiScale:" << settings.guiScale << "\n"
        << "checkUpdates:" << boolText(settings.checkUpdates) << "\n"
        << "smoothLighting:" << boolText(settings.smoothLighting) << "\n"
        << "animate:" << boolText(settings.animate) << "\n"
        << "simBudget:" << settings.simBudget << "\n";
}

} // namespace gol3d

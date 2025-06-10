#pragma once

#include <string>
#include <vector>
#include <chrono>
#include <filesystem>

namespace App {

enum class State {
    None,           // Initial/No state
    Menu,           // Main menu state
    Running,        // Game/simulation running
    SavePicker,     // Save file selection
    Loading,        // Loading state
    Exiting         // Cleanup and exit
};

struct SaveInfo {
    std::string filename;
    std::string displayName;
    std::string name;
    std::string description;
    std::string author;
    std::string version;
    std::chrono::system_clock::time_point lastModified;
    std::chrono::system_clock::time_point creationTime;  // Renamed from timestamp
    size_t voxelCount;
    size_t size;
    std::string thumbnailPath;  // Optional: path to save preview image
    bool created;
};

struct MenuState {
    bool showSavePicker = false;
    bool showNewProjectDialog = false;
    bool showSettings = false;
    std::vector<SaveInfo> saveFiles;
    int selectedSaveIndex = -1;
};

} // namespace App 
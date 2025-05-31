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
    std::chrono::system_clock::time_point lastModified;
    size_t voxelCount;
    std::string thumbnailPath;  // Optional: path to save preview image
    std::chrono::system_clock::time_point timestamp;
    size_t size;
    bool created;
    
    // Additional fields referenced in the code
    std::string name;
    std::string description;
    std::string author;
    std::string version;
};

struct MenuState {
    bool showSavePicker = false;
    bool showNewProjectDialog = false;
    bool showSettings = false;
    std::vector<SaveInfo> saveFiles;
    int selectedSaveIndex = -1;
};

} // namespace App 
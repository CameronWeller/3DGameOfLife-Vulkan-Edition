#include "SaveManager.h"
#include <fstream>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

SaveManager::SaveManager() {
    // Set default save directory to %LOCALAPPDATA%/cpp-vulkan-hip-engine/saves
    auto appData = std::filesystem::path(std::getenv("LOCALAPPDATA"));
    saveDirectory_ = appData / "cpp-vulkan-hip-engine" / "saves";
    createSaveDirectory();
}

SaveManager::~SaveManager() = default;

bool SaveManager::saveCurrentState(const std::string& filename, const VoxelData& voxelData) {
    try {
        auto savePath = saveDirectory_ / filename;
        if (!savePath.has_extension()) {
            savePath.replace_extension(".json");
        }

        // Create save data structure
        json saveData = {
            {"version", "1.0"},
            {"timestamp", std::chrono::system_clock::now().time_since_epoch().count()},
            {"voxelCount", voxelData.getVoxelCount()},
            {"camera", {
                {"position", {0.0f, 0.0f, 0.0f}},
                {"rotation", {0.0f, 0.0f, 0.0f}}
            }},
            {"voxels", voxelData.toJson()["voxels"]}
        };

        // Write to file
        std::ofstream file(savePath);
        if (!file.is_open()) {
            return false;
        }
        file << saveData.dump(4);
        file.close();

        lastSaveFile_ = filename;
        return true;
    }
    catch (const std::exception&) {
        return false;
    }
}

bool SaveManager::loadSaveFile(const std::string& filename, VoxelData& voxelData) {
    try {
        auto savePath = saveDirectory_ / filename;
        if (!savePath.has_extension()) {
            savePath.replace_extension(".json");
        }

        if (!std::filesystem::exists(savePath)) {
            return false;
        }

        std::ifstream file(savePath);
        if (!file.is_open()) {
            return false;
        }

        json saveData;
        file >> saveData;
        file.close();

        // Validate save file structure
        if (!saveData.contains("version") || !saveData.contains("voxels")) {
            return false;
        }

        // Load voxel data
        auto loadedVoxelData = VoxelData::fromJson(saveData);
        if (!loadedVoxelData) {
            return false;
        }

        // Copy voxels to the provided voxelData
        voxelData.clear();
        for (const auto& voxel : loadedVoxelData->getVoxels()) {
            voxelData.addVoxel(voxel);
        }

        lastSaveFile_ = filename;
        return true;
    }
    catch (const std::exception&) {
        return false;
    }
}

bool SaveManager::deleteSaveFile(const std::string& filename) {
    try {
        auto savePath = saveDirectory_ / filename;
        if (!savePath.has_extension()) {
            savePath.replace_extension(".json");
        }

        if (!std::filesystem::exists(savePath)) {
            return false;
        }

        return std::filesystem::remove(savePath);
    }
    catch (const std::exception&) {
        return false;
    }
}

std::vector<App::SaveInfo> SaveManager::getSaveFiles() {
    std::vector<App::SaveInfo> saves;
    try {
        for (const auto& entry : std::filesystem::directory_iterator(saveDirectory_)) {
            if (entry.path().extension() == ".json") {
                saves.push_back(createSaveInfo(entry.path()));
            }
        }
    }
    catch (const std::exception&) {
        // Return empty vector on error
    }
    return saves;
}

std::string SaveManager::getLastSaveFile() const {
    return lastSaveFile_;
}

bool SaveManager::hasSaveFiles() const {
    try {
        return !std::filesystem::is_empty(saveDirectory_);
    }
    catch (const std::exception&) {
        return false;
    }
}

void SaveManager::setSaveDirectory(const std::filesystem::path& path) {
    saveDirectory_ = path;
    createSaveDirectory();
}

std::filesystem::path SaveManager::getSaveDirectory() const {
    return saveDirectory_;
}

bool SaveManager::createSaveDirectory() {
    try {
        if (!std::filesystem::exists(saveDirectory_)) {
            return std::filesystem::create_directories(saveDirectory_);
        }
        return true;
    }
    catch (const std::exception&) {
        return false;
    }
}

bool SaveManager::validateSaveFile(const std::filesystem::path& path) const {
    try {
        std::ifstream file(path);
        if (!file.is_open()) {
            return false;
        }

        json saveData;
        file >> saveData;
        file.close();

        return saveData.contains("version") && saveData.contains("voxels");
    }
    catch (const std::exception&) {
        return false;
    }
}

App::SaveInfo SaveManager::createSaveInfo(const std::filesystem::path& path) const {
    App::SaveInfo info;
    info.filename = path.filename().string();
    info.displayName = path.stem().string();
    info.lastModified = std::chrono::clock_cast<std::chrono::system_clock>(
        std::filesystem::last_write_time(path)
    );
    
    try {
        std::ifstream file(path);
        if (file.is_open()) {
            json saveData;
            file >> saveData;
            info.voxelCount = saveData.value("voxelCount", 0);
        }
    }
    catch (const std::exception&) {
        info.voxelCount = 0;
    }

    return info;
}

std::string SaveManager::generateSaveFileName() const {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << "save_" << std::put_time(std::localtime(&time), "%Y%m%d_%H%M%S");
    return ss.str();
} 
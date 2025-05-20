#include "SaveManager.h"
#include <filesystem>
#include <fstream>
#include <chrono>
#include <ctime>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <stdexcept>
#include <nlohmann/json.hpp>

namespace fs = std::filesystem;

namespace VulkanHIP {

SaveManager::SaveManager() {
    try {
        // Get the user's home directory
        const char* homeDir = getenv("USERPROFILE");
        if (!homeDir) {
            homeDir = getenv("HOME");
        }
        if (!homeDir) {
            throw SaveError(SaveError::Type::Unknown, "Could not determine home directory");
        }

        // Create the saves directory
        saveDirectory_ = fs::path(homeDir) / "VulkanEngine" / "saves";
        if (!createSaveDirectory()) {
            throw SaveError(SaveError::Type::FileAccessDenied, 
                          "Failed to create save directory: " + saveDirectory_.string());
        }
    } catch (const SaveError& e) {
        handleSaveError(e);
        throw;
    } catch (const std::exception& e) {
        throw SaveError(SaveError::Type::Unknown, std::string("Unexpected error: ") + e.what());
    }
}

SaveManager::~SaveManager() {
    try {
        // Cleanup old autosaves on exit
        cleanupOldAutoSaves(5); // Keep last 5 autosaves
    } catch (...) {
        // Log error but don't throw from destructor
    }
}

void SaveManager::handleSaveError(const SaveError& error) {
    setLastError(error.details);
    switch (error.type) {
        case SaveError::Type::FileNotFound:
            std::cerr << "File not found: " << error.details << std::endl;
            break;
        case SaveError::Type::FileAccessDenied:
            std::cerr << "Access denied: " << error.details << std::endl;
            break;
        case SaveError::Type::FileCorrupted:
            std::cerr << "File corrupted: " << error.details << std::endl;
            break;
        case SaveError::Type::InvalidData:
            std::cerr << "Invalid data: " << error.details << std::endl;
            break;
        case SaveError::Type::OutOfMemory:
            std::cerr << "Out of memory: " << error.details << std::endl;
            break;
        default:
            std::cerr << "Unknown error: " << error.details << std::endl;
    }
}

bool SaveManager::saveCurrentState(const std::string& filename, const VoxelData& voxelData) {
    if (filename.empty()) {
        setLastError("Invalid filename");
        return false;
    }

    try {
        std::string path = (saveDirectory_ / filename).string();
        std::ofstream file(path, std::ios::binary);
        if (!file) {
            throw SaveError(SaveError::Type::FileAccessDenied, 
                          "Failed to open file for writing: " + path);
        }

        // Write voxel data
        file.write(reinterpret_cast<const char*>(&voxelData), sizeof(VoxelData));
        if (file.fail()) {
            file.close();
            fs::remove(path); // Clean up failed save
            throw SaveError(SaveError::Type::FileCorrupted, 
                          "Failed to write voxel data to file: " + path);
        }

        lastSaveFile_ = filename;
        clearLastError();
        return true;
    } catch (const SaveError& e) {
        handleSaveError(e);
        return false;
    } catch (const std::exception& e) {
        handleSaveError(SaveError(SaveError::Type::Unknown, 
                                std::string("Unexpected error during save: ") + e.what()));
        return false;
    }
}

bool SaveManager::loadSaveFile(const std::string& filename, VoxelData& voxelData) {
    if (filename.empty()) {
        setLastError("Invalid filename");
        return false;
    }

    try {
        std::string path = (saveDirectory_ / filename).string();
        std::ifstream file(path, std::ios::binary);
        if (!file) {
            throw SaveError(SaveError::Type::FileNotFound, 
                          "Failed to open save file: " + path);
        }

        // Read voxel data
        file.read(reinterpret_cast<char*>(&voxelData), sizeof(VoxelData));
        if (file.fail()) {
            throw SaveError(SaveError::Type::FileCorrupted, 
                          "Failed to read voxel data from file: " + path);
        }

        lastSaveFile_ = filename;
        clearLastError();
        return true;
    } catch (const SaveError& e) {
        handleSaveError(e);
        return false;
    } catch (const std::exception& e) {
        handleSaveError(SaveError(SaveError::Type::Unknown, 
                                std::string("Unexpected error during load: ") + e.what()));
        return false;
    }
}

bool SaveManager::deleteSaveFile(const std::string& filename) {
    try {
        std::string path = (saveDirectory_ / filename).string();
        if (!fs::exists(path)) {
            throw SaveError(SaveError::Type::FileNotFound, 
                          "Save file does not exist: " + path);
        }

        if (!fs::remove(path)) {
            throw SaveError(SaveError::Type::FileAccessDenied, 
                          "Failed to delete save file: " + path);
        }

        if (lastSaveFile_ == filename) {
            lastSaveFile_.clear();
        }

        clearLastError();
        return true;
    } catch (const SaveError& e) {
        handleSaveError(e);
        return false;
    } catch (const std::exception& e) {
        handleSaveError(SaveError(SaveError::Type::Unknown, 
                                std::string("Unexpected error during delete: ") + e.what()));
        return false;
    }
}

std::vector<App::SaveInfo> SaveManager::getSaveFiles() {
    std::vector<App::SaveInfo> saves;
    try {
        for (const auto& entry : fs::directory_iterator(saveDirectory_)) {
            if (entry.is_regular_file() && entry.path().extension() == ".save") {
                if (validateSaveFile(entry.path())) {
                    saves.push_back(createSaveInfo(entry.path()));
                }
            }
        }
        std::sort(saves.begin(), saves.end(),
            [](const App::SaveInfo& a, const App::SaveInfo& b) {
                return a.timestamp > b.timestamp;
            });
    } catch (const std::exception&) {
        // Return empty vector on error
    }
    return saves;
}

std::string SaveManager::getLastSaveFile() const {
    return lastSaveFile_;
}

bool SaveManager::hasSaveFiles() const {
    try {
        for (const auto& entry : fs::directory_iterator(saveDirectory_)) {
            if (entry.is_regular_file() && entry.path().extension() == ".save") {
                return true;
            }
        }
    } catch (const std::exception&) {
        // Return false on error
    }
    return false;
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
        fs::create_directories(saveDirectory_);
        return true;
    } catch (...) {
        return false;
    }
}

bool SaveManager::validateSaveFile(const std::filesystem::path& path) const {
    try {
        if (!fs::exists(path) || !fs::is_regular_file(path)) {
            return false;
        }

        std::ifstream file(path, std::ios::binary);
        if (!file) {
            return false;
        }

        // Check file size
        file.seekg(0, std::ios::end);
        size_t size = file.tellg();
        if (size != sizeof(VoxelData)) {
            return false;
        }

        // Check file permissions
        auto perms = fs::status(path).permissions();
        if ((perms & fs::perms::owner_read) == fs::perms::none) {
            return false;
        }

        return true;
    } catch (const std::exception&) {
        return false;
    }
}

App::SaveInfo SaveManager::createSaveInfo(const std::filesystem::path& path) const {
    App::SaveInfo info;
    try {
        info.filename = path.filename().string();
        auto lastWriteTime = fs::last_write_time(path);
        // Convert last_write_time to time_t (portable way)
        auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
            lastWriteTime - decltype(lastWriteTime)::clock::now() + std::chrono::system_clock::now()
        );
        info.timestamp = sctp;
        info.size = fs::file_size(path);
        info.created = true;
    } catch (const std::exception&) {
        info.filename = path.filename().string();
        info.timestamp = {};
        info.size = 0;
        info.created = false;
    }
    return info;
}

std::string SaveManager::generateSaveFileName() const {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    std::tm tm;
#ifdef _WIN32
    localtime_s(&tm, &time);
#else
    localtime_r(&time, &tm);
#endif

    std::stringstream ss;
    ss << "save_" << std::put_time(&tm, "%Y%m%d_%H%M%S") << ".save";
    return ss.str();
}

std::string SaveManager::getSavePath(const std::string& filename) const {
    return (saveDirectory_ / filename).string();
}

std::vector<std::string> SaveManager::listSaves() const {
    std::vector<std::string> saves;
    try {
        for (const auto& entry : fs::directory_iterator(saveDirectory_)) {
            if (entry.is_regular_file() && entry.path().extension() == ".save") {
                saves.push_back(entry.path().filename().string());
            }
        }
    } catch (const std::exception&) {
        // Return empty vector on error
    }
    return saves;
}

void SaveManager::cleanupOldAutoSaves(int maxAutoSaves) {
    try {
        std::vector<fs::directory_entry> autoSaves;
        for (const auto& entry : fs::directory_iterator(saveDirectory_)) {
            if (entry.is_regular_file() && 
                entry.path().extension() == ".save" && 
                entry.path().filename().string().find("autosave_") == 0) {
                autoSaves.push_back(entry);
            }
        }

        // Sort by last write time, newest first
        std::sort(autoSaves.begin(), autoSaves.end(),
            [](const auto& a, const auto& b) {
                return fs::last_write_time(a) > fs::last_write_time(b);
            });

        // Remove old autosaves
        for (size_t i = maxAutoSaves; i < autoSaves.size(); ++i) {
            fs::remove(autoSaves[i]);
        }
    } catch (const std::exception&) {
        // Log error but don't throw
    }
}

} // namespace VulkanHIP 
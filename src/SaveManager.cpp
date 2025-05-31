#include "SaveManager.h"
#include "VulkanEngine.h"
#include "VulkanContext.h"
#include "VulkanMemoryManager.h"
#include "Logger.h"
#include <vulkan/vulkan.h>
#include <filesystem>
#include <fstream>
#include <chrono>
#include <ctime>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <stdexcept>
#include <iostream>
#include <nlohmann/json.hpp>

namespace fs = std::filesystem;
using json = nlohmann::json;

namespace VulkanHIP {

// Helper function to get default pattern directory
std::filesystem::path getDefaultPatternDirectory() {
    // Get the executable directory and create a patterns subdirectory
    std::filesystem::path exePath = std::filesystem::current_path();
    std::filesystem::path patternDir = exePath / "patterns";
    
    // Create directory if it doesn't exist
    try {
        std::filesystem::create_directories(patternDir);
    } catch (const std::exception&) {
        // Fall back to current directory if we can't create patterns directory
        patternDir = exePath;
    }
    
    return patternDir;
}

SaveManager::SaveManager() 
    : patternDirectory_(getDefaultPatternDirectory()),
      previewDirectory_(patternDirectory_ / "previews"),
      engine_(nullptr) {
    createPatternDirectory();
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

bool SaveManager::savePattern(const std::string& filename, const VoxelData& voxelData, const PatternMetadata& metadata) {
    if (filename.empty()) {
        setLastError("Invalid filename");
        return false;
    }

    try {
        std::string path = (patternDirectory_ / filename).string();
        std::ofstream file(path);
        if (!file) {
            throw SaveError(SaveError::Type::FileAccessDenied, 
                          "Failed to open file for writing: " + path);
        }

        // Create JSON object
        json patternJson;
        patternJson["metadata"] = serializeMetadata(metadata);
        patternJson["voxelData"] = serializeVoxelData(voxelData);

        // Write JSON to file
        file << std::setw(4) << patternJson;
        if (file.fail()) {
            file.close();
            fs::remove(path); // Clean up failed save
            throw SaveError(SaveError::Type::FileCorrupted, 
                          "Failed to write pattern data to file: " + path);
        }

        lastPatternFile_ = filename;
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

bool SaveManager::loadPattern(const std::string& filename, VoxelData& voxelData, PatternMetadata& metadata) {
    if (filename.empty()) {
        setLastError("Invalid filename");
        return false;
    }

    try {
        std::string path = (patternDirectory_ / filename).string();
        std::ifstream file(path);
        if (!file) {
            throw SaveError(SaveError::Type::FileNotFound, 
                          "Failed to open pattern file: " + path);
        }

        // Read JSON from file
        json patternJson;
        file >> patternJson;

        // Parse metadata and voxel data
        if (!deserializeMetadata(patternJson["metadata"], metadata)) {
            throw SaveError(SaveError::Type::InvalidData, 
                          "Failed to parse pattern metadata: " + path);
        }
        if (!deserializeVoxelData(patternJson["voxelData"], voxelData)) {
            throw SaveError(SaveError::Type::InvalidData, 
                          "Failed to parse voxel data: " + path);
        }

        lastPatternFile_ = filename;
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

bool SaveManager::deletePattern(const std::string& filename) {
    try {
        std::string path = (patternDirectory_ / filename).string();
        if (!fs::exists(path)) {
            throw SaveError(SaveError::Type::FileNotFound, 
                          "Pattern file does not exist: " + path);
        }

        if (!fs::remove(path)) {
            throw SaveError(SaveError::Type::FileAccessDenied, 
                          "Failed to delete pattern file: " + path);
        }

        // Also delete preview if it exists
        std::string previewPath = getPreviewPath(filename);
        if (fs::exists(previewPath)) {
            fs::remove(previewPath);
        }

        if (lastPatternFile_ == filename) {
            lastPatternFile_.clear();
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

std::vector<App::SaveInfo> SaveManager::getPatternFiles() {
    std::vector<App::SaveInfo> patterns;
    try {
        for (const auto& entry : fs::directory_iterator(patternDirectory_)) {
            if (entry.is_regular_file() && entry.path().extension() == ".pattern") {
                if (validatePatternFile(entry.path())) {
                    patterns.push_back(createSaveInfo(entry.path()));
                }
            }
        }
        std::sort(patterns.begin(), patterns.end(),
            [](const App::SaveInfo& a, const App::SaveInfo& b) {
                return a.timestamp > b.timestamp;
            });
    } catch (const std::exception&) {
        // Return empty vector on error
    }
    return patterns;
}

std::string SaveManager::getLastPatternFile() const {
    return lastPatternFile_;
}

bool SaveManager::hasPatternFiles() const {
    try {
        for (const auto& entry : fs::directory_iterator(patternDirectory_)) {
            if (entry.is_regular_file() && entry.path().extension() == ".pattern") {
                return true;
            }
        }
    } catch (const std::exception&) {
        // Return false on error
    }
    return false;
}

void SaveManager::setPatternDirectory(const std::filesystem::path& path) {
    patternDirectory_ = path;
    previewDirectory_ = path / "previews";
    createPatternDirectory();
}

std::filesystem::path SaveManager::getPatternDirectory() const {
    return patternDirectory_;
}

bool SaveManager::createPatternDirectory() {
    try {
        fs::create_directories(patternDirectory_);
        fs::create_directories(previewDirectory_);
        return true;
    } catch (...) {
        return false;
    }
}

bool SaveManager::validatePatternFile(const std::filesystem::path& path) const {
    try {
        if (!fs::exists(path) || !fs::is_regular_file(path)) {
            return false;
        }

        std::ifstream file(path);
        if (!file) {
            return false;
        }

        // Try to parse JSON
        json patternJson;
        try {
            file >> patternJson;
        } catch (...) {
            return false;
        }

        // Check required fields
        if (!patternJson.contains("metadata") || !patternJson.contains("voxelData")) {
            return false;
        }

        // Check file permissions
        auto perms = fs::status(path).permissions();
        if ((perms & fs::perms::owner_read) == fs::perms::none) {
            return false;
        }

        return true;
    } catch (...) {
        return false;
    }
}

App::SaveInfo SaveManager::createSaveInfo(const std::filesystem::path& path) const {
    App::SaveInfo info;
    info.filename = path.filename().string();
    info.timestamp = fs::last_write_time(path).time_since_epoch().count();
    
    // Try to read metadata for additional info
    try {
        std::ifstream file(path);
        json patternJson;
        file >> patternJson;
        
        if (patternJson.contains("metadata")) {
            const auto& metadata = patternJson["metadata"];
            info.name = metadata.value("name", info.filename);
            info.description = metadata.value("description", "");
            info.author = metadata.value("author", "");
            info.version = metadata.value("version", "1.0");
            info.creationTime = std::chrono::system_clock::from_time_t(metadata.value("creationTime", 0));
        }
    } catch (...) {
        // Use default values if metadata can't be read
    }
    
    return info;
}

std::string SaveManager::generatePatternFileName() const {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << "pattern_" << std::put_time(std::localtime(&time), "%Y%m%d_%H%M%S") << ".pattern";
    return ss.str();
}

void SaveManager::generatePreview(const std::string& filename, const VoxelData& voxelData) {
    try {
        // For now, create a simple placeholder preview
        // In a full implementation, this would render the voxel data to an image
        std::string previewPath = getPreviewPath(filename);
        
        // Create a simple text file as a placeholder preview
        std::ofstream previewFile(previewPath);
        if (previewFile.is_open()) {
            previewFile << "Preview for: " << filename << std::endl;
            previewFile << "Voxel count: " << voxelData.getVoxelCount() << std::endl;
            previewFile << "Dimensions: " << voxelData.dimensions.x << "x" 
                       << voxelData.dimensions.y << "x" << voxelData.dimensions.z << std::endl;
            previewFile.close();
        }
        
    } catch (const std::exception& e) {
        std::cerr << "Error generating preview: " << e.what() << std::endl;
    }
}

bool SaveManager::hasPreview(const std::string& filename) const {
    std::string previewPath = getPreviewPath(filename);
    return fs::exists(previewPath);
}

std::string SaveManager::getPreviewPath(const std::string& filename) const {
    std::string baseName = fs::path(filename).stem().string();
    return (previewDirectory_ / (baseName + ".png")).string();
}

nlohmann::json SaveManager::serializeVoxelData(const VoxelData& voxelData) const {
    json j;
    
    // Serialize grid dimensions
    j["dimensions"] = {
        voxelData.dimensions.x,
        voxelData.dimensions.y,
        voxelData.dimensions.z
    };
    
    // Serialize voxel data as a flat array of booleans
    std::vector<bool> flatData;
    flatData.reserve(voxelData.dimensions.x * voxelData.dimensions.y * voxelData.dimensions.z);
    
    for (int z = 0; z < voxelData.dimensions.z; z++) {
        for (int y = 0; y < voxelData.dimensions.y; y++) {
            for (int x = 0; x < voxelData.dimensions.x; x++) {
                flatData.push_back(voxelData.getVoxel(x, y, z));
            }
        }
    }
    
    // Convert bool vector to uint8_t vector for better JSON compatibility
    std::vector<uint8_t> packedData;
    packedData.reserve((flatData.size() + 7) / 8);
    
    for (size_t i = 0; i < flatData.size(); i += 8) {
        uint8_t byte = 0;
        for (size_t bit = 0; bit < 8 && (i + bit) < flatData.size(); bit++) {
            if (flatData[i + bit]) {
                byte |= (1 << bit);
            }
        }
        packedData.push_back(byte);
    }
    
    j["data"] = packedData;
    return j;
}

bool SaveManager::deserializeVoxelData(const nlohmann::json& json, VoxelData& voxelData) const {
    try {
        // Parse dimensions
        auto dims = json["dimensions"].get<std::vector<int>>();
        if (dims.size() != 3) {
            return false;
        }
        
        voxelData.dimensions = glm::ivec3(dims[0], dims[1], dims[2]);
        
        // Parse packed data
        auto packedData = json["data"].get<std::vector<uint8_t>>();
        
        // Calculate expected size
        size_t totalVoxels = dims[0] * dims[1] * dims[2];
        size_t expectedPackedSize = (totalVoxels + 7) / 8;
        
        if (packedData.size() != expectedPackedSize) {
            return false;
        }
        
        // Unpack data into voxel grid
        for (int z = 0; z < dims[2]; z++) {
            for (int y = 0; y < dims[1]; y++) {
                for (int x = 0; x < dims[0]; x++) {
                    size_t index = z * dims[1] * dims[0] + y * dims[0] + x;
                    size_t byteIndex = index / 8;
                    size_t bitIndex = index % 8;
                    
                    bool value = (packedData[byteIndex] & (1 << bitIndex)) != 0;
                    voxelData.setVoxel(x, y, z, value);
                }
            }
        }
        
        return true;
    } catch (...) {
        return false;
    }
}

nlohmann::json SaveManager::serializeMetadata(const PatternMetadata& metadata) const {
    json j;
    j["name"] = metadata.name;
    j["description"] = metadata.description;
    j["author"] = metadata.author;
    j["version"] = metadata.version;
    j["ruleSet"] = metadata.ruleSet;
    j["gridSize"] = {metadata.gridSize.x, metadata.gridSize.y, metadata.gridSize.z};
    j["voxelSize"] = metadata.voxelSize;
    j["creationTime"] = metadata.creationTime;
    j["modificationTime"] = metadata.modificationTime;
    j["population"] = metadata.population;
    j["generation"] = metadata.generation;
    j["tags"] = metadata.tags;
    return j;
}

bool SaveManager::deserializeMetadata(const nlohmann::json& json, PatternMetadata& metadata) const {
    try {
        metadata.name = json["name"].get<std::string>();
        metadata.description = json["description"].get<std::string>();
        metadata.author = json["author"].get<std::string>();
        metadata.version = json["version"].get<std::string>();
        metadata.ruleSet = json["ruleSet"].get<std::string>();
        
        auto gridSize = json["gridSize"].get<std::vector<float>>();
        metadata.gridSize = glm::vec3(gridSize[0], gridSize[1], gridSize[2]);
        
        metadata.voxelSize = json["voxelSize"].get<float>();
        metadata.creationTime = json["creationTime"].get<std::time_t>();
        metadata.modificationTime = json["modificationTime"].get<std::time_t>();
        metadata.population = json["population"].get<uint32_t>();
        metadata.generation = json["generation"].get<uint32_t>();
        metadata.tags = json["tags"].get<std::vector<std::string>>();
        
        return true;
    } catch (...) {
        return false;
    }
}

void SaveManager::cleanupOldAutoSaves(int maxAutoSaves) {
    try {
        std::vector<fs::directory_entry> autoSaves;
        for (const auto& entry : fs::directory_iterator(patternDirectory_)) {
            if (entry.is_regular_file() && 
                entry.path().extension() == ".pattern" && 
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
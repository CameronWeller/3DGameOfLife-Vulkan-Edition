#pragma once

#include "AppState.h"
#include "VoxelData.h"
#include <filesystem>
#include <string>
#include <vector>
#include <memory>
#include <nlohmann/json.hpp>

namespace VulkanHIP {

// Forward declaration
class VulkanEngine;

struct PatternMetadata {
    std::string name;
    std::string description;
    std::string author;
    std::string version;
    std::string ruleSet;
    glm::vec3 gridSize;
    float voxelSize;
    std::time_t creationTime;
    std::time_t modificationTime;
    uint32_t population;
    uint32_t generation;
    std::vector<std::string> tags;
};

class SaveManager {
public:
    SaveManager(VulkanEngine* engine = nullptr);
    ~SaveManager();

    // Error handling
    struct SaveError : public std::runtime_error {
        enum class Type {
            FileNotFound,
            FileAccessDenied,
            FileCorrupted,
            InvalidData,
            OutOfMemory,
            Unknown
        };
        
        Type type;
        std::string details;
        
        SaveError(Type t, const std::string& msg) 
            : std::runtime_error(msg), type(t), details(msg) {}
    };

    // Save file operations
    bool savePattern(const std::string& filename, const VoxelData& voxelData, const PatternMetadata& metadata);
    bool loadPattern(const std::string& filename, VoxelData& voxelData, PatternMetadata& metadata);
    bool deletePattern(const std::string& filename);
    
    // Pattern file management
    std::vector<App::SaveInfo> getPatternFiles();
    std::string getLastPatternFile() const;
    bool hasPatternFiles() const;
    
    // Pattern directory management
    void setPatternDirectory(const std::filesystem::path& path);
    std::filesystem::path getPatternDirectory() const;
    bool createPatternDirectory();

    // Additional utility functions
    std::string getPatternPath(const std::string& filename) const;
    std::vector<std::string> listPatterns() const;
    void cleanupOldAutoSaves(int maxAutoSaves = 5);
    
    // Pattern preview
    void generatePreview(const std::string& filename, const VoxelData& voxelData);
    bool hasPreview(const std::string& filename) const;
    std::string getPreviewPath(const std::string& filename) const;

    // Error handling
    std::string getLastError() const { return lastError_; }
    void clearLastError() { lastError_.clear(); }

private:
    VulkanEngine* engine_;
    std::filesystem::path patternDirectory_;
    std::filesystem::path previewDirectory_;
    std::string lastPatternFile_;
    std::string lastError_;
    
    // Helper functions
    bool validatePatternFile(const std::filesystem::path& path) const;
    App::SaveInfo createSaveInfo(const std::filesystem::path& path) const;
    std::string generatePatternFileName() const;
    void setLastError(const std::string& error) { lastError_ = error; }
    void handleSaveError(const SaveError& error);
    
    // JSON serialization
    nlohmann::json serializeVoxelData(const VoxelData& voxelData) const;
    bool deserializeVoxelData(const nlohmann::json& json, VoxelData& voxelData) const;
    nlohmann::json serializeMetadata(const PatternMetadata& metadata) const;
    bool deserializeMetadata(const nlohmann::json& json, PatternMetadata& metadata) const;
};

} // namespace VulkanHIP 
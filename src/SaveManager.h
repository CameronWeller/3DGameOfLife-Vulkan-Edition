#pragma once

#include "AppState.h"
#include "VoxelData.h"
#include <filesystem>
#include <string>
#include <vector>
#include <memory>

namespace VulkanHIP {

class SaveManager {
public:
    SaveManager();
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
    bool saveCurrentState(const std::string& filename, const VoxelData& voxelData);
    bool loadSaveFile(const std::string& filename, VoxelData& voxelData);
    bool deleteSaveFile(const std::string& filename);
    
    // Save file management
    std::vector<App::SaveInfo> getSaveFiles();
    std::string getLastSaveFile() const;
    bool hasSaveFiles() const;
    
    // Save directory management
    void setSaveDirectory(const std::filesystem::path& path);
    std::filesystem::path getSaveDirectory() const;
    bool createSaveDirectory();

    // Additional utility functions
    std::string getSavePath(const std::string& filename) const;
    std::vector<std::string> listSaves() const;
    void cleanupOldAutoSaves(int maxAutoSaves = 5);

    // Error handling
    std::string getLastError() const { return lastError_; }
    void clearLastError() { lastError_.clear(); }

private:
    std::filesystem::path saveDirectory_;
    std::string lastSaveFile_;
    std::string lastError_;
    
    // Helper functions
    bool validateSaveFile(const std::filesystem::path& path) const;
    App::SaveInfo createSaveInfo(const std::filesystem::path& path) const;
    std::string generateSaveFileName() const;
    void setLastError(const std::string& error) { lastError_ = error; }
    void handleSaveError(const SaveError& error);
};

} // namespace VulkanHIP 
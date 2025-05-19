#pragma once

#include "AppState.h"
#include "VoxelData.h"
#include <filesystem>
#include <string>
#include <vector>
#include <memory>

class SaveManager {
public:
    SaveManager();
    ~SaveManager();

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

private:
    std::filesystem::path saveDirectory_;
    std::string lastSaveFile_;
    
    // Helper functions
    bool validateSaveFile(const std::filesystem::path& path) const;
    App::SaveInfo createSaveInfo(const std::filesystem::path& path) const;
    std::string generateSaveFileName() const;
}; 
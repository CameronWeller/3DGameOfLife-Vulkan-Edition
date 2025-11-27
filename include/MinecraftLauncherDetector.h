#pragma once

#include <string>
#include <vector>
#include <filesystem>
#include <optional>

namespace UXTesting {

struct MinecraftInstallation {
    std::string launcherPath;
    std::string launcherType;  // "Official", "Bedrock", "ThirdParty"
    std::string version;
    std::string installationPath;
    std::string javaPath;
    std::string worldsPath;
    bool isBedrock;
    bool isValid;
};

class MinecraftLauncherDetector {
public:
    MinecraftLauncherDetector();
    ~MinecraftLauncherDetector() = default;
    
    // Detection methods
    std::vector<MinecraftInstallation> detectAllInstallations();
    std::optional<MinecraftInstallation> detectOfficialLauncher();
    std::optional<MinecraftInstallation> detectBedrockEdition();
    std::optional<MinecraftInstallation> detectThirdPartyLaunchers();
    
    // Utility methods
    bool isValidInstallation(const MinecraftInstallation& installation);
    std::string getDefaultLauncherPath();
    std::vector<std::string> getCommonInstallationPaths();
    std::vector<std::string> getThirdPartyLauncherPaths();
    
    // Configuration
    void setSearchPaths(const std::vector<std::string>& paths);
    void setCustomLauncherPath(const std::string& path);
    
private:
    std::vector<std::string> searchPaths_;
    std::string customLauncherPath_;
    
    // Helper methods
    bool checkFileExists(const std::string& path);
    bool checkDirectoryExists(const std::string& path);
    std::string findExecutableInDirectory(const std::string& directory, const std::string& executableName);
    std::vector<std::string> getRegistryInstallationPaths();
    std::string getAppDataPath();
    std::string getProgramFilesPath();
    std::string getProgramFilesX86Path();
    
    // Platform-specific methods
    std::vector<std::string> getWindowsInstallationPaths();
    std::vector<std::string> getMacInstallationPaths();
    std::vector<std::string> getLinuxInstallationPaths();
    
    // Validation methods
    bool validateOfficialLauncher(const std::string& path);
    bool validateBedrockLauncher(const std::string& path);
    bool validateThirdPartyLauncher(const std::string& path);
    
    // Installation info extraction
    std::string extractVersionFromPath(const std::string& path);
    std::string findJavaPath();
    std::string findWorldsPath(const std::string& launcherPath);
};

} // namespace UXTesting 
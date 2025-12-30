#include "MinecraftLauncherDetector.h"
#include <iostream>
#include <algorithm>
#include <fstream>
#include <sstream>

#ifdef _WIN32
#include <windows.h>
#include <shlobj.h>
#include <winreg.h>
#endif

namespace UXTesting {

MinecraftLauncherDetector::MinecraftLauncherDetector() {
    // Initialize default search paths
    searchPaths_ = getCommonInstallationPaths();
}

std::vector<MinecraftInstallation> MinecraftLauncherDetector::detectAllInstallations() {
    std::vector<MinecraftInstallation> installations;
    
    // Detect official launcher
    auto official = detectOfficialLauncher();
    if (official.has_value()) {
        installations.push_back(official.value());
    }
    
    // Detect Bedrock edition
    auto bedrock = detectBedrockEdition();
    if (bedrock.has_value()) {
        installations.push_back(bedrock.value());
    }
    
    // Detect third-party launchers
    auto thirdParty = detectThirdPartyLaunchers();
    if (thirdParty.has_value()) {
        installations.push_back(thirdParty.value());
    }
    
    return installations;
}

std::optional<MinecraftInstallation> MinecraftLauncherDetector::detectOfficialLauncher() {
    std::vector<std::string> possiblePaths = {
        "C:\\Program Files (x86)\\Minecraft Launcher\\MinecraftLauncher.exe",
        "C:\\Program Files\\Minecraft Launcher\\MinecraftLauncher.exe",
        getAppDataPath() + "\\Local\\Programs\\Minecraft Launcher\\MinecraftLauncher.exe"
    };
    
    // Add custom path if set
    if (!customLauncherPath_.empty()) {
        possiblePaths.insert(possiblePaths.begin(), customLauncherPath_);
    }
    
    for (const auto& path : possiblePaths) {
        if (validateOfficialLauncher(path)) {
            MinecraftInstallation installation;
            installation.launcherPath = path;
            installation.launcherType = "Official";
            installation.isBedrock = false;
            installation.isValid = true;
            installation.version = extractVersionFromPath(path);
            installation.installationPath = std::filesystem::path(path).parent_path().string();
            installation.javaPath = findJavaPath();
            installation.worldsPath = findWorldsPath(path);
            
            return installation;
        }
    }
    
    return std::nullopt;
}

std::optional<MinecraftInstallation> MinecraftLauncherDetector::detectBedrockEdition() {
    std::vector<std::string> possiblePaths = {
        "C:\\Program Files\\WindowsApps\\Microsoft.MinecraftUWP_*\\Minecraft.Windows.exe",
        "C:\\Program Files\\Minecraft for Windows\\Minecraft.Windows.exe"
    };
    
    for (const auto& path : possiblePaths) {
        if (validateBedrockLauncher(path)) {
            MinecraftInstallation installation;
            installation.launcherPath = path;
            installation.launcherType = "Bedrock";
            installation.isBedrock = true;
            installation.isValid = true;
            installation.version = "Bedrock";
            installation.installationPath = std::filesystem::path(path).parent_path().string();
            installation.worldsPath = getAppDataPath() + "\\Packages\\Microsoft.MinecraftUWP_*\\LocalState\\games\\com.mojang\\minecraftWorlds";
            
            return installation;
        }
    }
    
    return std::nullopt;
}

std::optional<MinecraftInstallation> MinecraftLauncherDetector::detectThirdPartyLaunchers() {
    std::vector<std::string> thirdPartyPaths = getThirdPartyLauncherPaths();
    
    for (const auto& path : thirdPartyPaths) {
        if (validateThirdPartyLauncher(path)) {
            MinecraftInstallation installation;
            installation.launcherPath = path;
            installation.launcherType = "ThirdParty";
            installation.isBedrock = false;
            installation.isValid = true;
            installation.version = extractVersionFromPath(path);
            installation.installationPath = std::filesystem::path(path).parent_path().string();
            installation.javaPath = findJavaPath();
            installation.worldsPath = findWorldsPath(path);
            
            return installation;
        }
    }
    
    return std::nullopt;
}

bool MinecraftLauncherDetector::isValidInstallation(const MinecraftInstallation& installation) {
    return installation.isValid && 
           checkFileExists(installation.launcherPath) &&
           !installation.launcherPath.empty();
}

std::string MinecraftLauncherDetector::getDefaultLauncherPath() {
    auto official = detectOfficialLauncher();
    if (official.has_value()) {
        return official->launcherPath;
    }
    
    auto thirdParty = detectThirdPartyLaunchers();
    if (thirdParty.has_value()) {
        return thirdParty->launcherPath;
    }
    
    return "";
}

std::vector<std::string> MinecraftLauncherDetector::getCommonInstallationPaths() {
    std::vector<std::string> paths;
    
#ifdef _WIN32
    paths = getWindowsInstallationPaths();
#elif defined(__APPLE__)
    paths = getMacInstallationPaths();
#else
    paths = getLinuxInstallationPaths();
#endif
    
    return paths;
}

std::vector<std::string> MinecraftLauncherDetector::getThirdPartyLauncherPaths() {
    std::vector<std::string> paths;
    
    // Common third-party launchers
    std::vector<std::string> launchers = {
        "MultiMC.exe",
        "ATLauncher.exe",
        "CurseForge.exe",
        "TechnicLauncher.exe",
        "FTBLauncher.exe"
    };
    
    std::vector<std::string> searchDirs = {
        getProgramFilesPath(),
        getProgramFilesX86Path(),
        getAppDataPath() + "\\Local",
        getAppDataPath() + "\\Roaming"
    };
    
    for (const auto& dir : searchDirs) {
        for (const auto& launcher : launchers) {
            std::string fullPath = findExecutableInDirectory(dir, launcher);
            if (!fullPath.empty()) {
                paths.push_back(fullPath);
            }
        }
    }
    
    return paths;
}

void MinecraftLauncherDetector::setSearchPaths(const std::vector<std::string>& paths) {
    searchPaths_ = paths;
}

void MinecraftLauncherDetector::setCustomLauncherPath(const std::string& path) {
    customLauncherPath_ = path;
}

bool MinecraftLauncherDetector::checkFileExists(const std::string& path) {
    return std::filesystem::exists(path) && std::filesystem::is_regular_file(path);
}

bool MinecraftLauncherDetector::checkDirectoryExists(const std::string& path) {
    return std::filesystem::exists(path) && std::filesystem::is_directory(path);
}

std::string MinecraftLauncherDetector::findExecutableInDirectory(const std::string& directory, const std::string& executableName) {
    if (!checkDirectoryExists(directory)) {
        return "";
    }
    
    try {
        for (const auto& entry : std::filesystem::recursive_directory_iterator(directory)) {
            if (entry.is_regular_file() && entry.path().filename() == executableName) {
                return entry.path().string();
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Error searching directory " << directory << ": " << e.what() << std::endl;
    }
    
    return "";
}

std::vector<std::string> MinecraftLauncherDetector::getRegistryInstallationPaths() {
    std::vector<std::string> paths;
    
#ifdef _WIN32
    HKEY hKey;
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        char subKeyName[256];
        DWORD subKeyNameSize = sizeof(subKeyName);
        DWORD index = 0;
        
        while (RegEnumKeyExA(hKey, index, subKeyName, &subKeyNameSize, nullptr, nullptr, nullptr, nullptr) == ERROR_SUCCESS) {
            HKEY hSubKey;
            if (RegOpenKeyExA(hKey, subKeyName, 0, KEY_READ, &hSubKey) == ERROR_SUCCESS) {
                char displayName[256];
                DWORD displayNameSize = sizeof(displayName);
                DWORD type = REG_SZ;
                
                if (RegQueryValueExA(hSubKey, "DisplayName", nullptr, &type, (LPBYTE)displayName, &displayNameSize) == ERROR_SUCCESS) {
                    std::string name(displayName);
                    if (name.find("Minecraft") != std::string::npos) {
                        char installLocation[512];
                        DWORD installLocationSize = sizeof(installLocation);
                        
                        if (RegQueryValueExA(hSubKey, "InstallLocation", nullptr, &type, (LPBYTE)installLocation, &installLocationSize) == ERROR_SUCCESS) {
                            paths.push_back(std::string(installLocation));
                        }
                    }
                }
                RegCloseKey(hSubKey);
            }
            subKeyNameSize = sizeof(subKeyName);
            index++;
        }
        RegCloseKey(hKey);
    }
#endif
    
    return paths;
}

std::string MinecraftLauncherDetector::getAppDataPath() {
#ifdef _WIN32
    char path[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathA(nullptr, CSIDL_APPDATA, nullptr, 0, path))) {
        return std::string(path);
    }
#endif
    return "";
}

std::string MinecraftLauncherDetector::getProgramFilesPath() {
#ifdef _WIN32
    char path[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathA(nullptr, CSIDL_PROGRAM_FILES, nullptr, 0, path))) {
        return std::string(path);
    }
#endif
    return "C:\\Program Files";
}

std::string MinecraftLauncherDetector::getProgramFilesX86Path() {
#ifdef _WIN32
    char path[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathA(nullptr, CSIDL_PROGRAM_FILESX86, nullptr, 0, path))) {
        return std::string(path);
    }
#endif
    return "C:\\Program Files (x86)";
}

std::vector<std::string> MinecraftLauncherDetector::getWindowsInstallationPaths() {
    std::vector<std::string> paths;
    
    // Common Windows installation paths
    paths.push_back("C:\\Program Files (x86)\\Minecraft Launcher");
    paths.push_back("C:\\Program Files\\Minecraft Launcher");
    paths.push_back(getAppDataPath() + "\\Local\\Programs\\Minecraft Launcher");
    paths.push_back(getAppDataPath() + "\\Roaming\\.minecraft");
    
    // Add registry paths
    auto registryPaths = getRegistryInstallationPaths();
    paths.insert(paths.end(), registryPaths.begin(), registryPaths.end());
    
    return paths;
}

std::vector<std::string> MinecraftLauncherDetector::getMacInstallationPaths() {
    std::vector<std::string> paths;
    paths.push_back("/Applications/Minecraft.app");
    paths.push_back(std::string(getenv("HOME")) + "/Library/Application Support/minecraft");
    return paths;
}

std::vector<std::string> MinecraftLauncherDetector::getLinuxInstallationPaths() {
    std::vector<std::string> paths;
    paths.push_back("/usr/share/minecraft");
    paths.push_back("/opt/minecraft");
    paths.push_back(std::string(getenv("HOME")) + "/.minecraft");
    return paths;
}

bool MinecraftLauncherDetector::validateOfficialLauncher(const std::string& path) {
    if (!checkFileExists(path)) {
        return false;
    }
    
    // Check if it's the official Minecraft launcher
    std::string filename = std::filesystem::path(path).filename().string();
    return filename == "MinecraftLauncher.exe" || filename == "Minecraft.exe";
}

bool MinecraftLauncherDetector::validateBedrockLauncher(const std::string& path) {
    if (!checkFileExists(path)) {
        return false;
    }
    
    std::string filename = std::filesystem::path(path).filename().string();
    return filename == "Minecraft.Windows.exe" || filename == "MinecraftUWP.exe";
}

bool MinecraftLauncherDetector::validateThirdPartyLauncher(const std::string& path) {
    if (!checkFileExists(path)) {
        return false;
    }
    
    std::string filename = std::filesystem::path(path).filename().string();
    std::vector<std::string> validLaunchers = {
        "MultiMC.exe", "ATLauncher.exe", "CurseForge.exe", 
        "TechnicLauncher.exe", "FTBLauncher.exe"
    };
    
    return std::find(validLaunchers.begin(), validLaunchers.end(), filename) != validLaunchers.end();
}

std::string MinecraftLauncherDetector::extractVersionFromPath(const std::string& path) {
    // Try to extract version from path or executable properties
    std::filesystem::path filePath(path);
    
    // Check if version is in the path
    std::string pathStr = filePath.string();
    size_t versionPos = pathStr.find("1.");
    if (versionPos != std::string::npos) {
        size_t endPos = pathStr.find_first_not_of("0123456789.", versionPos);
        if (endPos != std::string::npos) {
            return pathStr.substr(versionPos, endPos - versionPos);
        }
    }
    
    return "latest";
}

std::string MinecraftLauncherDetector::findJavaPath() {
    std::vector<std::string> javaPaths = {
        "C:\\Program Files\\Java",
        "C:\\Program Files (x86)\\Java",
        "C:\\Program Files\\Eclipse Adoptium",
        "C:\\Program Files\\Eclipse Foundation"
    };
    
    for (const auto& javaPath : javaPaths) {
        if (checkDirectoryExists(javaPath)) {
            std::string javaExe = findExecutableInDirectory(javaPath, "java.exe");
            if (!javaExe.empty()) {
                return javaExe;
            }
        }
    }
    
    return "";
}

std::string MinecraftLauncherDetector::findWorldsPath(const std::string& launcherPath) {
    // Default Minecraft worlds path
    std::string defaultPath = getAppDataPath() + "\\Roaming\\.minecraft\\saves";
    
    if (checkDirectoryExists(defaultPath)) {
        return defaultPath;
    }
    
    // Try to find worlds relative to launcher
    std::filesystem::path launcherDir = std::filesystem::path(launcherPath).parent_path();
    std::string relativePath = (launcherDir / "saves").string();
    
    if (checkDirectoryExists(relativePath)) {
        return relativePath;
    }
    
    return defaultPath;
}

} // namespace UXTesting 
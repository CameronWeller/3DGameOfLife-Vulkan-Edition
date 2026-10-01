#pragma once

// Update checks against this repository's GitHub releases.
//
// GitHub cannot push to installed copies, so the game asks the releases API at
// startup. Where the install format allows it, the update is downloaded,
// verified against the SHA-256 digest GitHub publishes for the asset, and
// applied: the Windows installer runs silently and relaunches the game; an
// AppImage is replaced in place. Other formats (deb, rpm, Arch, dmg, archives)
// open the release page, since their package manager or the user installs them.
// Downloads go through the system's curl and only from this repository's
// release URLs.

#include <cstdint>
#include <filesystem>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

namespace gol3d {

inline constexpr const char* RELEASES_API_LATEST =
    "https://api.github.com/repos/CameronWeller/3DGameOfLife-Vulkan-Edition/releases/latest";
inline constexpr const char* RELEASES_PAGE = "https://github.com/CameronWeller/3DGameOfLife-Vulkan-Edition/releases";
inline constexpr const char* RELEASE_DOWNLOAD_PREFIX =
    "https://github.com/CameronWeller/3DGameOfLife-Vulkan-Edition/releases/download/";

struct ReleaseAsset {
    std::string name;
    std::string url;
    std::string sha256; // lowercase hex, empty if GitHub gave no digest
};

struct ReleaseInfo {
    std::string version; // "0.1.12" (tag without the leading v)
    std::string pageUrl;
    std::vector<ReleaseAsset> assets;
};

enum class InstallMethod { OpenPage, WindowsInstaller, AppImage };

// Parses the JSON of GET /repos/{owner}/{repo}/releases/latest.
std::optional<ReleaseInfo> parseLatestRelease(const std::string& json);
// True when `candidate` ("0.1.12") is a higher dotted version than `current`.
bool isNewerVersion(const std::string& candidate, const std::string& current);
std::string sha256Hex(const std::string& bytes);
std::optional<std::string> sha256OfFile(const std::filesystem::path& file);
bool isTrustedDownloadUrl(const std::string& url);
bool isReleasePageUrl(const std::string& url);
// The asset this build installs from, e.g. "gol3d-0.1.12-windows-x64.exe".
std::string assetNameFor(const std::string& version, InstallMethod method);
InstallMethod detectInstallMethod(const std::filesystem::path& exeDir);
bool openInBrowser(const std::string& url);

class Updater {
public:
    enum class State { Idle, Checking, UpToDate, Available, Downloading, Ready, Failed };

    Updater(std::string currentVersion, std::filesystem::path exeDir, std::string feedUrl = RELEASES_API_LATEST);
    ~Updater(); // waits for a running check or download
    Updater(const Updater&) = delete;
    Updater& operator=(const Updater&) = delete;

    void checkAsync();
    // Downloads and verifies the update; for an AppImage also swaps it in place.
    void installAsync();
    // Windows: starts the verified installer, which relaunches the game when done.
    // Returns true when the game should now quit.
    bool launchInstaller();

    State state() const;
    ReleaseInfo release() const;
    std::string error() const;
    InstallMethod method() const { return method_; }
    const std::string& currentVersion() const { return currentVersion_; }
    // The AppImage to exec after an in-place update.
    std::filesystem::path appImagePath() const { return appImage_; }

private:
    void setState(State state, const std::string& error = {});
    void runCheck();
    void runInstall();
    void join();

    std::string currentVersion_;
    std::filesystem::path exeDir_;
    std::string feedUrl_;
    InstallMethod method_;
    std::filesystem::path appImage_;
    std::filesystem::path downloadedInstaller_;

    mutable std::mutex mutex_;
    State state_ = State::Idle;
    ReleaseInfo release_;
    std::string error_;
    std::thread worker_;
};

} // namespace gol3d

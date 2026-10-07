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
inline constexpr const char* RELEASES_PAGE =
    "https://github.com/CameronWeller/3DGameOfLife-Vulkan-Edition/releases";
inline constexpr const char* RELEASE_DOWNLOAD_PREFIX =
    "https://github.com/CameronWeller/3DGameOfLife-Vulkan-Edition/releases/download/";

// One downloadable file of a release.
struct ReleaseAsset {
    std::string name;
    std::string url;
    std::string sha256; // lowercase hex, empty if GitHub gave no digest
};

// The parts of a GitHub release the updater uses.
struct ReleaseInfo {
    std::string version; // "0.1.12" (tag without the leading v)
    std::string pageUrl;
    std::vector<ReleaseAsset> assets;
};

// How this copy of the game can update itself.
enum class InstallMethod {
    OpenPage,         // show the release page; the user or a package manager installs
    WindowsInstaller, // run the downloaded NSIS installer silently
    AppImage,         // replace the AppImage file in place
};

// Parses the JSON of GET /repos/{owner}/{repo}/releases/latest.
std::optional<ReleaseInfo> parseLatestRelease(const std::string& json);
// True when `candidate` ("0.1.12") is a higher dotted version than `current`.
bool isNewerVersion(const std::string& candidate, const std::string& current);
// True for this repository's release download URLs made only of plain URL
// characters, the only URLs the updater downloads from.
bool isTrustedDownloadUrl(const std::string& url);
// True for this repository's release pages, the only URLs opened in a browser.
bool isReleasePageUrl(const std::string& url);
// The asset this build installs from, e.g. "gol3d-0.1.12-windows-x64.exe".
std::string assetNameFor(const std::string& version, InstallMethod method);
InstallMethod detectInstallMethod(const std::filesystem::path& exeDir);
// Opens a release page (and nothing else) in the default browser.
bool openInBrowser(const std::string& url);

// Checks for and installs updates on a worker thread, so the network never
// stalls a frame; the menus poll state() to show progress.
//
// The states move like this:
//   checkAsync():   -> Checking -> UpToDate | Available | Failed
//   installAsync(): Available | Failed -> Downloading -> Ready | Failed
// A new check may start from any state but Checking and Downloading. Both
// calls come from the main thread only, and at most one worker runs at a time.
class Updater {
public:
    enum class State { Idle, Checking, UpToDate, Available, Downloading, Ready, Failed };

    Updater(std::string currentVersion, std::filesystem::path exeDir,
            std::string feedUrl = RELEASES_API_LATEST);
    ~Updater(); // waits for a running check or download
    Updater(const Updater&) = delete;
    Updater& operator=(const Updater&) = delete;

    // Asks GitHub for the latest release, unless a check or download is running.
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

    // Fixed at construction, so both threads read them freely.
    std::string currentVersion_;
    std::filesystem::path exeDir_;
    std::string feedUrl_;
    InstallMethod method_;
    std::filesystem::path appImage_;
    // Written by the worker before it sets State::Ready, and only read after
    // that (the mutex in setState and state() orders the two).
    std::filesystem::path downloadedInstaller_;

    // Guards state_, release_ and error_, which both threads use.
    mutable std::mutex mutex_;
    State state_ = State::Idle;
    ReleaseInfo release_;
    std::string error_;
    std::thread worker_;
};

} // namespace gol3d

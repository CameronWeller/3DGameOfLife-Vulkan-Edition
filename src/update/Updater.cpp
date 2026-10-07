// The self-updater: reads GitHub's release JSON, runs curl to download, checks
// the SHA-256 digest, and applies the update for the install formats that allow
// it. See Updater.h for the overall flow and the trust rules.

#include "update/Updater.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX // keep windows.h from defining min and max macros
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <shellapi.h> // after windows.h, which it needs
#else
#include <sys/wait.h>
#endif

#include "update/Sha256.h"

namespace gol3d {
namespace {

// curl's --max-time for each request, in seconds: the release check is small
// and should give up quickly; a download may be slow.
constexpr const char* CHECK_TIMEOUT_SECONDS = "15";
constexpr const char* DOWNLOAD_TIMEOUT_SECONDS = "900";

// ------------------------------------------------------------ reading JSON
//
// A deliberately small reader for the few fields the updater needs from
// GitHub's release JSON. It understands strings (with backslash escapes),
// objects and arrays well enough to skip over them; it is not a general parser.

constexpr const char* JSON_WHITESPACE = " \t\r\n";

// The index just past the string literal whose opening quote is at `quote`, or
// npos when the string never ends.
size_t endOfString(const std::string& json, size_t quote) {
    for (size_t i = quote + 1; i < json.size(); ++i) {
        if (json[i] == '\\') {
            ++i; // skip the escaped character, which may be a quote
        } else if (json[i] == '"') {
            return i + 1;
        }
    }
    return std::string::npos;
}

// The contents of the string literal json[quote, end), with each backslash
// escape replaced by the character after the backslash. That is right for \"
// \\ and \/, which are all the fields read here can contain.
std::string decodeString(const std::string& json, size_t quote, size_t end) {
    std::string out;
    // `end` is one past the closing quote, so the contents end at end - 1.
    for (size_t i = quote + 1; i + 1 < end; ++i) {
        if (json[i] == '\\' && i + 2 < end) ++i;
        out += json[i];
    }
    return out;
}

// The string value of "key" in `object`, looking only at the object's own keys
// (keys of nested objects are skipped).
std::optional<std::string> stringField(const std::string& object, const std::string& key) {
    const std::string quotedKey = "\"" + key + "\"";
    int depth = 0; // 1 inside `object` itself
    for (size_t i = 0; i < object.size();) {
        const char c = object[i];
        if (c != '"') {
            if (c == '{' || c == '[') ++depth;
            if (c == '}' || c == ']') --depth;
            ++i;
            continue;
        }
        // Strings are skipped whole, so braces inside them are not counted.
        const size_t end = endOfString(object, i);
        if (end == std::string::npos) return std::nullopt;
        const bool isKey = depth == 1 && end == i + quotedKey.size() &&
                           object.compare(i, quotedKey.size(), quotedKey) == 0;
        if (!isKey) {
            i = end;
            continue;
        }
        // "key" : "value"
        size_t colon = object.find_first_not_of(JSON_WHITESPACE, end);
        if (colon == std::string::npos || object[colon] != ':') return std::nullopt;
        size_t valueStart = object.find_first_not_of(JSON_WHITESPACE, colon + 1);
        if (valueStart == std::string::npos || object[valueStart] != '"') return std::nullopt;
        size_t valueEnd = endOfString(object, valueStart);
        if (valueEnd == std::string::npos) return std::nullopt;
        return decodeString(object, valueStart, valueEnd);
    }
    return std::nullopt;
}

// The objects directly inside the array value of "key", as JSON text.
std::vector<std::string> objectsInArray(const std::string& json, const std::string& key) {
    std::vector<std::string> objects;
    size_t keyPosition = json.find("\"" + key + "\"");
    if (keyPosition == std::string::npos) return objects;
    size_t open = json.find('[', keyPosition);
    if (open == std::string::npos) return objects;
    int depth = 0; // of objects inside the array
    size_t objectStart = 0;
    for (size_t i = open + 1; i < json.size();) {
        const char c = json[i];
        if (c == '"') {
            i = endOfString(json, i);
            if (i == std::string::npos) break;
            continue;
        }
        if (c == '{') {
            if (depth == 0) objectStart = i;
            ++depth;
        } else if (c == '}') {
            --depth;
            if (depth == 0) objects.push_back(json.substr(objectStart, i - objectStart + 1));
        } else if (c == ']' && depth == 0) {
            break; // the end of the array
        }
        ++i;
    }
    return objects;
}

// ----------------------------------------------------------- running commands

struct CommandResult {
    int exitCode = -1; // -1 when the command could not be run
    std::string output;
};

#if defined(_WIN32)
// UTF-8 to the UTF-16 that the wide Windows API takes.
std::wstring widen(const std::string& text) {
    if (text.empty()) return {};
    // With a length of -1 the conversion includes the terminating null, which
    // the size counts and resize() then drops.
    int size = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, nullptr, 0);
    std::wstring out(size, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, out.data(), size);
    out.resize(size - 1);
    return out;
}

// Quotes an argument for CreateProcessW's command line. Escaping only the
// quotes is enough for what is passed here (fixed curl options, release URLs
// and temp-file paths, none of which end in a backslash); it is not the full
// CommandLineToArgvW escaping.
std::wstring quoteArgument(const std::wstring& arg) {
    std::wstring out = L"\"";
    for (wchar_t c : arg) {
        if (c == L'"') out += L'\\';
        out += c;
    }
    return out + L"\"";
}

std::wstring joinCommandLine(const std::vector<std::string>& args) {
    std::wstring commandLine;
    for (const std::string& arg : args) {
        if (!commandLine.empty()) commandLine += L' ';
        commandLine += quoteArgument(widen(arg));
    }
    return commandLine;
}

// Runs without a console window (the game is a GUI app) and captures stdout
// and stderr through a pipe.
CommandResult runCommand(const std::vector<std::string>& args) {
    CommandResult result;
    // The pipe handles are created inheritable so the child can write to them;
    // the read end is then made private to this process.
    SECURITY_ATTRIBUTES security{
        .nLength = sizeof(security), .lpSecurityDescriptor = nullptr, .bInheritHandle = TRUE};
    HANDLE readPipe = nullptr;
    HANDLE writePipe = nullptr;
    if (!CreatePipe(&readPipe, &writePipe, &security, 0)) return result;
    SetHandleInformation(readPipe, HANDLE_FLAG_INHERIT, 0);
    std::wstring commandLine = joinCommandLine(args);
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdOutput = writePipe;
    startup.hStdError = writePipe;
    PROCESS_INFORMATION process{};
    BOOL started = CreateProcessW(nullptr, commandLine.data(), nullptr, nullptr, TRUE,
                                  CREATE_NO_WINDOW, nullptr, nullptr, &startup, &process);
    // Close our copy of the write end, so ReadFile reports the end of the
    // output once the child exits and closes its copy.
    CloseHandle(writePipe);
    if (started) {
        char buffer[4096];
        DWORD read = 0;
        while (ReadFile(readPipe, buffer, sizeof(buffer), &read, nullptr) && read > 0) {
            result.output.append(buffer, read);
        }
        WaitForSingleObject(process.hProcess, INFINITE);
        DWORD code = 1;
        GetExitCodeProcess(process.hProcess, &code);
        result.exitCode = static_cast<int>(code);
        CloseHandle(process.hProcess);
        CloseHandle(process.hThread);
    }
    CloseHandle(readPipe);
    return result;
}
#else
// Single-quotes an argument for the POSIX shell: inside single quotes nothing
// is special, and a single quote itself is written as '\''.
std::string shellQuote(const std::string& arg) {
    std::string out = "'";
    for (char c : arg) {
        if (c == '\'') {
            out += "'\\''";
        } else {
            out += c;
        }
    }
    return out + "'";
}

// Runs through the shell (popen) with every argument quoted, and captures
// stdout; stderr is discarded.
CommandResult runCommand(const std::vector<std::string>& args) {
    CommandResult result;
    std::string command;
    for (const std::string& arg : args) {
        if (!command.empty()) command += ' ';
        command += shellQuote(arg);
    }
    command += " 2>/dev/null";
    FILE* pipe = popen(command.c_str(), "r");
    if (!pipe) return result;
    char buffer[4096];
    size_t read = 0;
    while ((read = fread(buffer, 1, sizeof(buffer), pipe)) > 0) {
        result.output.append(buffer, read);
    }
    int status = pclose(pipe);
    result.exitCode = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
    return result;
}
#endif

// curl flags: -f fails on HTTP errors instead of saving the error page, -sS is
// quiet except for errors, -L follows redirects (release downloads redirect to
// GitHub's file servers).
bool download(const std::string& url, const std::filesystem::path& to) {
    CommandResult result = runCommand(
        {"curl", "-fsSL", "--max-time", DOWNLOAD_TIMEOUT_SECONDS, "-o", to.string(), url});
    return result.exitCode == 0;
}

// "1.2.10" -> {1, 2, 10}; nothing for anything but dot-separated numbers.
std::optional<std::vector<long>> parseDottedVersion(const std::string& text) {
    std::vector<long> parts;
    std::stringstream stream(text);
    std::string part;
    auto isDigit = [](char c) { return c >= '0' && c <= '9'; };
    while (std::getline(stream, part, '.')) {
        if (part.empty() || !std::all_of(part.begin(), part.end(), isDigit)) {
            return std::nullopt;
        }
        parts.push_back(std::stol(part));
    }
    if (parts.empty()) return std::nullopt;
    return parts;
}

// Letters, digits and . - _ / : +, which is all a release asset URL needs. No
// spaces, quotes or shell metacharacters can get through.
bool isPlainUrlCharacter(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '.' ||
           c == '-' || c == '_' || c == '/' || c == ':' || c == '+';
}

// Anything visible except quotes and backslashes, so the URL cannot break out
// of the quoting when it is passed to a command.
bool isSafePageUrlCharacter(char c) {
    return c > ' ' && c != '"' && c != '\'' && c != '\\';
}

// Where an update downloads to. An AppImage downloads next to itself, so the
// final rename stays on one filesystem and is atomic.
std::filesystem::path downloadTarget(InstallMethod method, const std::filesystem::path& appImage,
                                     const std::string& assetName) {
    if (method == InstallMethod::AppImage) {
        return std::filesystem::path(appImage.string() + ".download");
    }
    std::error_code ignored;
    return std::filesystem::temp_directory_path(ignored) / assetName;
}

// Makes the downloaded AppImage executable (rwxr-xr-x) and renames it over the
// running one. The running copy keeps working: it holds the old file open.
// Removes the download and returns false on failure.
bool replaceAppImage(const std::filesystem::path& downloaded,
                     const std::filesystem::path& appImage) {
    using std::filesystem::perms;
    constexpr perms EXECUTABLE = perms::owner_all | perms::group_read | perms::group_exec |
                                 perms::others_read | perms::others_exec;
    std::error_code ec;
    std::filesystem::permissions(downloaded, EXECUTABLE, ec);
    std::filesystem::rename(downloaded, appImage, ec);
    if (ec) {
        std::filesystem::remove(downloaded, ec);
        return false;
    }
    return true;
}

} // namespace

// ---------------------------------------------------------- release checks

std::optional<ReleaseInfo> parseLatestRelease(const std::string& json) {
    std::optional<std::string> tag = stringField(json, "tag_name");
    if (!tag || tag->empty()) return std::nullopt;
    ReleaseInfo info;
    info.version = (*tag)[0] == 'v' ? tag->substr(1) : *tag;
    info.pageUrl = stringField(json, "html_url").value_or(RELEASES_PAGE);
    for (const std::string& object : objectsInArray(json, "assets")) {
        ReleaseAsset asset;
        asset.name = stringField(object, "name").value_or("");
        asset.url = stringField(object, "browser_download_url").value_or("");
        // GitHub gives the digest as "sha256:<hex>".
        std::string digest = stringField(object, "digest").value_or("");
        const std::string sha256Prefix = "sha256:";
        if (digest.starts_with(sha256Prefix)) asset.sha256 = digest.substr(sha256Prefix.size());
        if (!asset.name.empty() && !asset.url.empty()) info.assets.push_back(asset);
    }
    return info;
}

bool isNewerVersion(const std::string& candidate, const std::string& current) {
    std::optional<std::vector<long>> candidateParts = parseDottedVersion(candidate);
    std::optional<std::vector<long>> currentParts = parseDottedVersion(current);
    if (!candidateParts || !currentParts) return false;
    // Compare as equal-length lists, so 1.0 and 1.0.0 are the same version.
    size_t length = std::max(candidateParts->size(), currentParts->size());
    candidateParts->resize(length, 0);
    currentParts->resize(length, 0);
    return *candidateParts > *currentParts; // std::vector compares element by element
}

bool isTrustedDownloadUrl(const std::string& url) {
    if (!url.starts_with(RELEASE_DOWNLOAD_PREFIX)) return false;
    return std::all_of(url.begin(), url.end(), isPlainUrlCharacter);
}

bool isReleasePageUrl(const std::string& url) {
    return url.starts_with(RELEASES_PAGE) &&
           std::all_of(url.begin(), url.end(), isSafePageUrlCharacter);
}

std::string assetNameFor(const std::string& version, InstallMethod method) {
#if defined(_M_ARM64) || defined(__aarch64__)
    const bool arm = true;
#else
    const bool arm = false;
#endif
    switch (method) {
        case InstallMethod::WindowsInstaller:
            return "gol3d-" + version + "-windows-" + (arm ? "arm64" : "x64") + ".exe";
        case InstallMethod::AppImage:
            return "gol3d-" + version + "-linux-" + (arm ? "aarch64" : "x86_64") + ".AppImage";
        case InstallMethod::OpenPage:
            return {};
    }
    return {};
}

InstallMethod detectInstallMethod([[maybe_unused]] const std::filesystem::path& exeDir) {
#if defined(_WIN32)
    std::error_code ignored;
    // The NSIS installer leaves an uninstaller next to the game.
    if (std::filesystem::exists(exeDir / "Uninstall.exe", ignored)) {
        return InstallMethod::WindowsInstaller;
    }
#elif defined(__linux__)
    // The AppImage runtime sets APPIMAGE to the path of the running AppImage.
    const char* appImage = std::getenv("APPIMAGE");
    if (appImage && *appImage) return InstallMethod::AppImage;
#endif
    return InstallMethod::OpenPage;
}

bool openInBrowser(const std::string& url) {
    if (!isReleasePageUrl(url)) return false;
#if defined(_WIN32)
    // ShellExecute returns a value above 32 on success (a Win16 convention).
    HINSTANCE result =
        ShellExecuteW(nullptr, L"open", widen(url).c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    return reinterpret_cast<INT_PTR>(result) > 32;
#elif defined(__APPLE__)
    return runCommand({"open", url}).exitCode == 0;
#else
    return runCommand({"xdg-open", url}).exitCode == 0;
#endif
}

// ------------------------------------------------------------------ Updater

Updater::Updater(std::string currentVersion, std::filesystem::path exeDir, std::string feedUrl)
    : currentVersion_(std::move(currentVersion)),
      exeDir_(std::move(exeDir)),
      feedUrl_(std::move(feedUrl)),
      method_(detectInstallMethod(exeDir_)) {
    if (method_ == InstallMethod::AppImage) appImage_ = std::getenv("APPIMAGE");
}

Updater::~Updater() {
    join();
}

void Updater::join() {
    if (worker_.joinable()) worker_.join();
}

void Updater::setState(State state, const std::string& error) {
    std::lock_guard<std::mutex> lock(mutex_);
    state_ = state;
    error_ = error;
}

Updater::State Updater::state() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return state_;
}

ReleaseInfo Updater::release() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return release_;
}

std::string Updater::error() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return error_;
}

void Updater::checkAsync() {
    State current = state();
    if (current == State::Checking || current == State::Downloading) return;
    // Any earlier worker has already set its final state; joining it lets a
    // new thread take its place.
    join();
    setState(State::Checking);
    worker_ = std::thread([this] { runCheck(); });
}

void Updater::runCheck() {
    CommandResult result = runCommand({"curl", "-fsSL", "--max-time", CHECK_TIMEOUT_SECONDS, "-H",
                                       "Accept: application/vnd.github+json", "-H",
                                       "User-Agent: gol3d/" + currentVersion_, feedUrl_});
    if (result.exitCode != 0) {
        setState(State::Failed, "Could not reach GitHub to check for updates.");
        return;
    }
    std::optional<ReleaseInfo> info = parseLatestRelease(result.output);
    if (!info) {
        setState(State::Failed, "The update server sent an unexpected reply.");
        return;
    }
    {
        std::lock_guard<std::mutex> lock(mutex_);
        release_ = *info;
    }
    setState(isNewerVersion(info->version, currentVersion_) ? State::Available : State::UpToDate);
}

void Updater::installAsync() {
    State current = state();
    if (current != State::Available && current != State::Failed) return;
    if (release().version.empty()) return; // the failure was the check itself
    join();
    setState(State::Downloading);
    worker_ = std::thread([this] { runInstall(); });
}

// Picks this build's asset, downloads it, checks its SHA-256 and, for an
// AppImage, swaps it in. Every failure discards the download.
void Updater::runInstall() {
    ReleaseInfo info = release();
    std::string wanted = assetNameFor(info.version, method_);
    auto asset =
        std::find_if(info.assets.begin(), info.assets.end(),
                     [&](const ReleaseAsset& candidate) { return candidate.name == wanted; });
    if (wanted.empty() || asset == info.assets.end()) {
        setState(State::Failed, "This release has no " +
                                    (wanted.empty() ? std::string("installable package") : wanted) +
                                    ".");
        return;
    }
    constexpr size_t SHA256_HEX_DIGITS = 64;
    if (!isTrustedDownloadUrl(asset->url) || asset->sha256.size() != SHA256_HEX_DIGITS) {
        setState(State::Failed,
                 "The download is not from this project's releases or has no checksum.");
        return;
    }

    std::filesystem::path target = downloadTarget(method_, appImage_, wanted);
    std::error_code ignored;
    if (!download(asset->url, target)) {
        std::filesystem::remove(target, ignored);
        setState(State::Failed, "The download failed.");
        return;
    }
    std::optional<std::string> digest = sha256OfFile(target);
    if (!digest || *digest != asset->sha256) {
        std::filesystem::remove(target, ignored);
        setState(State::Failed,
                 "The download did not match its SHA-256 checksum and was discarded.");
        return;
    }

    if (method_ == InstallMethod::AppImage) {
        if (!replaceAppImage(target, appImage_)) {
            setState(State::Failed, "Could not replace the AppImage (is its folder writable?).");
            return;
        }
    } else {
        downloadedInstaller_ = target;
    }
    setState(State::Ready);
}

bool Updater::launchInstaller() {
#if defined(_WIN32)
    bool ready = method_ == InstallMethod::WindowsInstaller && state() == State::Ready &&
                 !downloadedInstaller_.empty();
    if (!ready) return false;
    // A small batch script outlives the game: it runs the installer (argument
    // 1) silently with NSIS's /S switch, waits for it, then starts the updated
    // game (argument 2). `start`'s first quoted argument is a window title.
    constexpr const char* UPDATE_SCRIPT =
        "@echo off\r\nstart \"\" /wait \"%~1\" /S\r\nstart \"\" \"%~2\"\r\n";
    std::error_code ignored;
    std::filesystem::path script =
        std::filesystem::temp_directory_path(ignored) / "gol3d-update.cmd";
    {
        std::ofstream out(script);
        out << UPDATE_SCRIPT;
    }
    wchar_t exe[MAX_PATH];
    GetModuleFileNameW(nullptr, exe, MAX_PATH);
    // cmd /c removes one pair of quotes around the whole command, hence the
    // extra outer pair.
    std::wstring commandLine = L"cmd.exe /c \"\"" + script.wstring() + L"\" \"" +
                               downloadedInstaller_.wstring() + L"\" \"" + std::wstring(exe) +
                               L"\"\"";
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process{};
    if (!CreateProcessW(nullptr, commandLine.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW,
                        nullptr, nullptr, &startup, &process)) {
        setState(State::Failed, "Could not start the installer.");
        return false;
    }
    CloseHandle(process.hProcess);
    CloseHandle(process.hThread);
    return true;
#else
    return false;
#endif
}

} // namespace gol3d

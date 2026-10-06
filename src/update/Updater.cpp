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
    int exitCode = -1;
    std::string output;
};

#if defined(_WIN32)
std::wstring widen(const std::string& text) {
    if (text.empty()) return {};
    int size = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, nullptr, 0);
    std::wstring out(size, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, out.data(), size);
    out.resize(size - 1);
    return out;
}

std::wstring quoteArgument(const std::wstring& arg) {
    std::wstring out = L"\"";
    for (wchar_t c : arg) {
        if (c == L'"') out += L'\\';
        out += c;
    }
    return out + L"\"";
}

// Runs without a console window (the game is a GUI app) and captures stdout.
CommandResult runCommand(const std::vector<std::string>& args) {
    CommandResult result;
    SECURITY_ATTRIBUTES security{sizeof(security), nullptr, TRUE};
    HANDLE readPipe = nullptr, writePipe = nullptr;
    if (!CreatePipe(&readPipe, &writePipe, &security, 0)) return result;
    SetHandleInformation(readPipe, HANDLE_FLAG_INHERIT, 0);
    std::wstring commandLine;
    for (const std::string& arg : args) {
        commandLine += (commandLine.empty() ? L"" : L" ") + quoteArgument(widen(arg));
    }
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdOutput = writePipe;
    startup.hStdError = writePipe;
    PROCESS_INFORMATION process{};
    BOOL started = CreateProcessW(nullptr, commandLine.data(), nullptr, nullptr, TRUE,
                                  CREATE_NO_WINDOW, nullptr, nullptr, &startup, &process);
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

CommandResult runCommand(const std::vector<std::string>& args) {
    CommandResult result;
    std::string command;
    for (const std::string& arg : args) {
        command += (command.empty() ? "" : " ") + shellQuote(arg);
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

bool download(const std::string& url, const std::filesystem::path& to) {
    return runCommand({"curl", "-fsSL", "--max-time", "900", "-o", to.string(), url}).exitCode == 0;
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
        std::string digest = stringField(object, "digest").value_or("");
        const std::string sha256Prefix = "sha256:";
        if (digest.rfind(sha256Prefix, 0) == 0) asset.sha256 = digest.substr(sha256Prefix.size());
        if (!asset.name.empty() && !asset.url.empty()) info.assets.push_back(asset);
    }
    return info;
}

bool isNewerVersion(const std::string& candidate, const std::string& current) {
    // "1.2.10" -> {1, 2, 10}; nothing for anything but dot-separated numbers.
    auto parse = [](const std::string& text) -> std::optional<std::vector<long>> {
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
    };
    std::optional<std::vector<long>> candidateParts = parse(candidate);
    std::optional<std::vector<long>> currentParts = parse(current);
    if (!candidateParts || !currentParts) return false;
    // Compare as equal-length lists, so 1.0 and 1.0.0 are the same version.
    size_t length = std::max(candidateParts->size(), currentParts->size());
    candidateParts->resize(length, 0);
    currentParts->resize(length, 0);
    return *candidateParts > *currentParts; // std::vector compares element by element
}

bool isTrustedDownloadUrl(const std::string& url) {
    if (url.rfind(RELEASE_DOWNLOAD_PREFIX, 0) != 0) return false;
    // Only plain path characters: the URL is handed to curl and never to a shell unquoted.
    return std::all_of(url.begin(), url.end(), [](char c) {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
               c == '.' || c == '-' || c == '_' || c == '/' || c == ':' || c == '+';
    });
}

bool isReleasePageUrl(const std::string& url) {
    return url.rfind(RELEASES_PAGE, 0) == 0 && std::all_of(url.begin(), url.end(), [](char c) {
               return c > ' ' && c != '"' && c != '\'' && c != '\\';
           });
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

InstallMethod detectInstallMethod(const std::filesystem::path& exeDir) {
#if defined(_WIN32)
    std::error_code ec;
    // The NSIS installer leaves an uninstaller next to the game.
    if (std::filesystem::exists(exeDir / "Uninstall.exe", ec)) {
        return InstallMethod::WindowsInstaller;
    }
#elif defined(__linux__)
    const char* appImage = std::getenv("APPIMAGE");
    if (appImage && *appImage) return InstallMethod::AppImage;
#endif
    (void)exeDir;
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
    join();
    setState(State::Checking);
    worker_ = std::thread([this] { runCheck(); });
}

void Updater::runCheck() {
    CommandResult result = runCommand({"curl", "-fsSL", "--max-time", "15", "-H",
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
    if (state() != State::Available && state() != State::Failed) return;
    if (release().version.empty()) return;
    join();
    setState(State::Downloading);
    worker_ = std::thread([this] { runInstall(); });
}

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

    // An AppImage downloads next to itself so the final rename is atomic.
    std::error_code ec;
    std::filesystem::path target = method_ == InstallMethod::AppImage
                                       ? std::filesystem::path(appImage_.string() + ".download")
                                       : std::filesystem::temp_directory_path(ec) / wanted;
    if (!download(asset->url, target)) {
        std::filesystem::remove(target, ec);
        setState(State::Failed, "The download failed.");
        return;
    }
    std::optional<std::string> digest = sha256OfFile(target);
    if (!digest || *digest != asset->sha256) {
        std::filesystem::remove(target, ec);
        setState(State::Failed,
                 "The download did not match its SHA-256 checksum and was discarded.");
        return;
    }

    if (method_ == InstallMethod::AppImage) {
        std::filesystem::permissions(
            target,
            std::filesystem::perms::owner_all | std::filesystem::perms::group_read |
                std::filesystem::perms::group_exec | std::filesystem::perms::others_read |
                std::filesystem::perms::others_exec,
            ec);
        std::filesystem::rename(target, appImage_, ec); // atomic replace in the same folder
        if (ec) {
            std::filesystem::remove(target, ec);
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
    // A small script waits for the silent install, then starts the updated game.
    std::error_code ec;
    std::filesystem::path script = std::filesystem::temp_directory_path(ec) / "gol3d-update.cmd";
    {
        std::ofstream out(script);
        out << "@echo off\r\nstart \"\" /wait \"%~1\" /S\r\nstart \"\" \"%~2\"\r\n";
    }
    wchar_t exe[MAX_PATH];
    GetModuleFileNameW(nullptr, exe, MAX_PATH);
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

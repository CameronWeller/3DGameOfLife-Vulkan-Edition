#include "Updater.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>

#if defined(_WIN32)
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#else
#include <sys/wait.h>
#endif

namespace gol3d {
namespace {

// ------------------------------------------------------------------ SHA-256

struct Sha256 {
    std::array<uint32_t, 8> h = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
                                 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
    std::array<uint8_t, 64> block{};
    size_t used = 0;
    uint64_t totalBits = 0;

    static uint32_t rotr(uint32_t x, int n) { return (x >> n) | (x << (32 - n)); }

    void compress() {
        static constexpr std::array<uint32_t, 64> k = {
            0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
            0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
            0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
            0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
            0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
            0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
            0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
            0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};
        std::array<uint32_t, 64> w{};
        for (int i = 0; i < 16; ++i) {
            w[i] = uint32_t(block[i * 4]) << 24 | uint32_t(block[i * 4 + 1]) << 16 | uint32_t(block[i * 4 + 2]) << 8 |
                   uint32_t(block[i * 4 + 3]);
        }
        for (int i = 16; i < 64; ++i) {
            uint32_t s0 = rotr(w[i - 15], 7) ^ rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
            uint32_t s1 = rotr(w[i - 2], 17) ^ rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
            w[i] = w[i - 16] + s0 + w[i - 7] + s1;
        }
        uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4], f = h[5], g = h[6], hh = h[7];
        for (int i = 0; i < 64; ++i) {
            uint32_t t1 = hh + (rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25)) + ((e & f) ^ (~e & g)) + k[i] + w[i];
            uint32_t t2 = (rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22)) + ((a & b) ^ (a & c) ^ (b & c));
            hh = g; g = f; f = e; e = d + t1; d = c; c = b; b = a; a = t1 + t2;
        }
        h[0] += a; h[1] += b; h[2] += c; h[3] += d; h[4] += e; h[5] += f; h[6] += g; h[7] += hh;
    }

    void update(const uint8_t* data, size_t size) {
        for (size_t i = 0; i < size; ++i) {
            block[used++] = data[i];
            if (used == 64) {
                compress();
                used = 0;
            }
        }
        totalBits += uint64_t(size) * 8;
    }

    std::string finish() {
        uint64_t bits = totalBits;
        uint8_t one = 0x80, zero = 0;
        update(&one, 1);
        while (used != 56) update(&zero, 1);
        for (int i = 7; i >= 0; --i) {
            uint8_t byte = uint8_t(bits >> (i * 8));
            update(&byte, 1);
        }
        static const char* hex = "0123456789abcdef";
        std::string out;
        for (uint32_t word : h) {
            for (int shift = 28; shift >= 0; shift -= 4) out += hex[(word >> shift) & 0xF];
        }
        return out;
    }
};

// ------------------------------------------------------------- tiny JSON read

size_t skipString(const std::string& json, size_t pos) { // pos at opening quote; returns index after closing quote
    for (size_t i = pos + 1; i < json.size(); ++i) {
        if (json[i] == '\\') ++i;
        else if (json[i] == '"') return i + 1;
    }
    return std::string::npos;
}

// Value of "key": "..." at the top level of `object` (nested objects are skipped).
std::optional<std::string> stringField(const std::string& object, const std::string& key) {
    int depth = 0;
    const std::string quoted = "\"" + key + "\"";
    for (size_t i = 0; i < object.size();) {
        char c = object[i];
        if (c == '"') {
            size_t end = skipString(object, i);
            if (end == std::string::npos) return std::nullopt;
            if (depth == 1 && object.compare(i, quoted.size(), quoted) == 0 && end == i + quoted.size()) {
                size_t colon = object.find_first_not_of(" \t\r\n", end);
                if (colon == std::string::npos || object[colon] != ':') return std::nullopt;
                size_t value = object.find_first_not_of(" \t\r\n", colon + 1);
                if (value == std::string::npos || object[value] != '"') return std::nullopt;
                size_t valueEnd = skipString(object, value);
                if (valueEnd == std::string::npos) return std::nullopt;
                std::string raw = object.substr(value + 1, valueEnd - value - 2), out;
                for (size_t k = 0; k < raw.size(); ++k) {
                    if (raw[k] == '\\' && k + 1 < raw.size()) out += raw[++k];
                    else out += raw[k];
                }
                return out;
            }
            i = end;
            continue;
        }
        if (c == '{' || c == '[') ++depth;
        if (c == '}' || c == ']') --depth;
        ++i;
    }
    return std::nullopt;
}

// The top-level objects inside the array value of "key".
std::vector<std::string> objectsInArray(const std::string& json, const std::string& key) {
    std::vector<std::string> objects;
    size_t keyPos = json.find("\"" + key + "\"");
    if (keyPos == std::string::npos) return objects;
    size_t open = json.find('[', keyPos);
    if (open == std::string::npos) return objects;
    int depth = 0;
    size_t start = 0;
    for (size_t i = open + 1; i < json.size();) {
        char c = json[i];
        if (c == '"') {
            i = skipString(json, i);
            if (i == std::string::npos) break;
            continue;
        }
        if (c == '{') {
            if (depth++ == 0) start = i;
        } else if (c == '}') {
            if (--depth == 0) objects.push_back(json.substr(start, i - start + 1));
        } else if (c == ']' && depth == 0) {
            break;
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
    for (const std::string& arg : args) commandLine += (commandLine.empty() ? L"" : L" ") + quoteArgument(widen(arg));
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdOutput = writePipe;
    startup.hStdError = writePipe;
    PROCESS_INFORMATION process{};
    BOOL started = CreateProcessW(nullptr, commandLine.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr,
                                  nullptr, &startup, &process);
    CloseHandle(writePipe);
    if (started) {
        char buffer[4096];
        DWORD read = 0;
        while (ReadFile(readPipe, buffer, sizeof(buffer), &read, nullptr) && read > 0) result.output.append(buffer, read);
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
std::string shellQuote(const std::string& arg) {
    std::string out = "'";
    for (char c : arg) {
        if (c == '\'') out += "'\\''";
        else out += c;
    }
    return out + "'";
}

CommandResult runCommand(const std::vector<std::string>& args) {
    CommandResult result;
    std::string command;
    for (const std::string& arg : args) command += (command.empty() ? "" : " ") + shellQuote(arg);
    command += " 2>/dev/null";
    FILE* pipe = popen(command.c_str(), "r");
    if (!pipe) return result;
    char buffer[4096];
    size_t read;
    while ((read = fread(buffer, 1, sizeof(buffer), pipe)) > 0) result.output.append(buffer, read);
    int status = pclose(pipe);
    result.exitCode = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
    return result;
}
#endif

bool download(const std::string& url, const std::filesystem::path& to) {
    return runCommand({"curl", "-fsSL", "--max-time", "900", "-o", to.string(), url}).exitCode == 0;
}

} // namespace

// ------------------------------------------------------------------ helpers

std::string sha256Hex(const std::string& bytes) {
    Sha256 sha;
    sha.update(reinterpret_cast<const uint8_t*>(bytes.data()), bytes.size());
    return sha.finish();
}

std::optional<std::string> sha256OfFile(const std::filesystem::path& file) {
    std::ifstream in(file, std::ios::binary);
    if (!in) return std::nullopt;
    Sha256 sha;
    std::array<char, 65536> buffer{};
    while (in) {
        in.read(buffer.data(), buffer.size());
        sha.update(reinterpret_cast<const uint8_t*>(buffer.data()), static_cast<size_t>(in.gcount()));
    }
    return sha.finish();
}

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
        if (digest.rfind("sha256:", 0) == 0) asset.sha256 = digest.substr(7);
        if (!asset.name.empty() && !asset.url.empty()) info.assets.push_back(asset);
    }
    return info;
}

bool isNewerVersion(const std::string& candidate, const std::string& current) {
    auto parse = [](const std::string& text) -> std::optional<std::vector<long>> {
        std::vector<long> parts;
        std::stringstream stream(text);
        std::string part;
        while (std::getline(stream, part, '.')) {
            if (part.empty() || !std::all_of(part.begin(), part.end(), [](char c) { return c >= '0' && c <= '9'; })) {
                return std::nullopt;
            }
            parts.push_back(std::stol(part));
        }
        if (parts.empty()) return std::nullopt;
        return parts;
    };
    auto a = parse(candidate), b = parse(current);
    if (!a || !b) return false;
    size_t n = std::max(a->size(), b->size());
    a->resize(n, 0);
    b->resize(n, 0);
    return *a > *b;
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
    return url.rfind(RELEASES_PAGE, 0) == 0 &&
           std::all_of(url.begin(), url.end(), [](char c) { return c > ' ' && c != '"' && c != '\'' && c != '\\'; });
}

std::string assetNameFor(const std::string& version, InstallMethod method) {
#if defined(_M_ARM64) || defined(__aarch64__)
    const bool arm = true;
#else
    const bool arm = false;
#endif
    switch (method) {
        case InstallMethod::WindowsInstaller: return "gol3d-" + version + "-windows-" + (arm ? "arm64" : "x64") + ".exe";
        case InstallMethod::AppImage: return "gol3d-" + version + "-linux-" + (arm ? "aarch64" : "x86_64") + ".AppImage";
        case InstallMethod::OpenPage: return {};
    }
    return {};
}

InstallMethod detectInstallMethod(const std::filesystem::path& exeDir) {
#if defined(_WIN32)
    std::error_code ec;
    if (std::filesystem::exists(exeDir / "Uninstall.exe", ec)) return InstallMethod::WindowsInstaller; // NSIS install
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
    return reinterpret_cast<INT_PTR>(ShellExecuteW(nullptr, L"open", widen(url).c_str(), nullptr, nullptr, SW_SHOWNORMAL)) > 32;
#elif defined(__APPLE__)
    return runCommand({"open", url}).exitCode == 0;
#else
    return runCommand({"xdg-open", url}).exitCode == 0;
#endif
}

// ------------------------------------------------------------------ Updater

Updater::Updater(std::string currentVersion, std::filesystem::path exeDir, std::string feedUrl)
    : currentVersion_(std::move(currentVersion)), exeDir_(std::move(exeDir)), feedUrl_(std::move(feedUrl)),
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
    CommandResult result = runCommand({"curl", "-fsSL", "--max-time", "15", "-H", "Accept: application/vnd.github+json",
                                       "-H", "User-Agent: gol3d/" + currentVersion_, feedUrl_});
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
    auto asset = std::find_if(info.assets.begin(), info.assets.end(), [&](const ReleaseAsset& a) { return a.name == wanted; });
    if (wanted.empty() || asset == info.assets.end()) {
        setState(State::Failed, "This release has no " + (wanted.empty() ? std::string("installable package") : wanted) + ".");
        return;
    }
    if (!isTrustedDownloadUrl(asset->url) || asset->sha256.size() != 64) {
        setState(State::Failed, "The download is not from this project's releases or has no checksum.");
        return;
    }

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
        setState(State::Failed, "The download did not match its SHA-256 checksum and was discarded.");
        return;
    }

    if (method_ == InstallMethod::AppImage) {
        std::filesystem::permissions(target, std::filesystem::perms::owner_all | std::filesystem::perms::group_read |
                                                 std::filesystem::perms::group_exec | std::filesystem::perms::others_read |
                                                 std::filesystem::perms::others_exec, ec);
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
    if (method_ != InstallMethod::WindowsInstaller || state() != State::Ready || downloadedInstaller_.empty()) return false;
    // A small script waits for the silent install, then starts the updated game.
    std::error_code ec;
    std::filesystem::path script = std::filesystem::temp_directory_path(ec) / "gol3d-update.cmd";
    {
        std::ofstream out(script);
        out << "@echo off\r\nstart \"\" /wait \"%~1\" /S\r\nstart \"\" \"%~2\"\r\n";
    }
    wchar_t exe[MAX_PATH];
    GetModuleFileNameW(nullptr, exe, MAX_PATH);
    std::wstring commandLine = L"cmd.exe /c \"\"" + script.wstring() + L"\" \"" + downloadedInstaller_.wstring() +
                               L"\" \"" + std::wstring(exe) + L"\"\"";
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process{};
    if (!CreateProcessW(nullptr, commandLine.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr,
                        &startup, &process)) {
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

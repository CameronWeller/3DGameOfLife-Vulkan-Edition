// The per-platform answers to "where is the executable" and "where do the
// player's files go"; see Paths.h for the folders themselves.

#include "platform/Paths.h"

#include <cstdint>
#include <cstdlib>
#include <ctime>
#include <iomanip>
#include <sstream>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX // keep windows.h from defining min and max macros
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#endif

namespace gol3d {
namespace {

// The environment variable as a path, or `fallback` when it is unset or empty.
std::filesystem::path envPathOr(const char* name, const std::filesystem::path& fallback) {
    const char* value = std::getenv(name);
    return (value && *value) ? std::filesystem::path(value) : fallback;
}

#if !defined(_WIN32)
std::filesystem::path homeDirectory() {
    const char* home = std::getenv("HOME");
    return home ? std::filesystem::path(home) : std::filesystem::path(".");
}
#endif

// The running executable as the operating system reports it, or an empty path.
std::filesystem::path runningExecutablePath() {
#if defined(_WIN32)
    wchar_t buffer[MAX_PATH];
    DWORD length = GetModuleFileNameW(nullptr, buffer, MAX_PATH);
    // A length of MAX_PATH means the path was cut short.
    if (length > 0 && length < MAX_PATH) return std::filesystem::path(std::wstring(buffer, length));
    return {};
#elif defined(__APPLE__)
    uint32_t size = 0;
    _NSGetExecutablePath(nullptr, &size); // asks for the buffer size
    std::string buffer(size, '\0');
    if (_NSGetExecutablePath(buffer.data(), &size) != 0) return {};
    std::error_code ignored;
    return std::filesystem::canonical(buffer.c_str(), ignored);
#else
    std::error_code ignored;
    return std::filesystem::read_symlink("/proc/self/exe", ignored);
#endif
}

} // namespace

std::filesystem::path executableDirectory(const char* argv0) {
    std::filesystem::path exe = runningExecutablePath();
    std::error_code ignored;
    if (exe.empty()) exe = std::filesystem::absolute(argv0, ignored);
    return exe.parent_path();
}

std::filesystem::path findShaderDirectory(const std::filesystem::path& exeDir) {
    const std::filesystem::path candidates[] = {
        exeDir / "shaders",
        exeDir / ".." / "share" / "gol3d" / "shaders",
        exeDir / ".." / "Resources" / "shaders",
    };
    std::error_code ignored;
    for (const std::filesystem::path& dir : candidates) {
        // Any one shader identifies the folder; the build installs them together.
        if (std::filesystem::exists(dir / "life3d_step.comp.spv", ignored)) return dir;
    }
    return "shaders"; // relative to the working directory, as a last resort
}

std::filesystem::path userDataDirectory() {
#if defined(_WIN32)
    const std::filesystem::path base = envPathOr("APPDATA", ".");
#elif defined(__APPLE__)
    const std::filesystem::path base = homeDirectory() / "Library" / "Application Support";
#else
    const std::filesystem::path base =
        envPathOr("XDG_DATA_HOME", homeDirectory() / ".local" / "share");
#endif
    std::filesystem::path dir = base / "gol3d";
    std::error_code ignored;
    std::filesystem::create_directories(dir, ignored);
    return dir;
}

std::filesystem::path settingsFilePath() {
#if defined(_WIN32)
    const std::filesystem::path base = envPathOr("APPDATA", ".");
#else
    const std::filesystem::path base = envPathOr("XDG_CONFIG_HOME", homeDirectory() / ".config");
#endif
    return base / "gol3d" / "options.txt";
}

std::string saveFilePath() {
    return (userDataDirectory() / "world.life3d").string();
}

std::string newScreenshotPath() {
    std::time_t now = std::time(nullptr);
    std::ostringstream name;
    name << "life3d-" << std::put_time(std::localtime(&now), "%Y%m%d-%H%M%S") << ".png";
    std::filesystem::path dir = userDataDirectory() / "screenshots";
    std::error_code ignored;
    std::filesystem::create_directories(dir, ignored);
    return (dir / name.str()).string();
}

} // namespace gol3d

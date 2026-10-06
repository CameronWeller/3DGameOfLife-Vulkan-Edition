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

// The value of an environment variable, or nullptr when it is unset or empty.
const char* nonEmptyEnv(const char* name) {
    const char* value = std::getenv(name);
    return (value && *value) ? value : nullptr;
}

std::filesystem::path homeDirectory() {
    const char* home = std::getenv("HOME");
    return home ? std::filesystem::path(home) : std::filesystem::path(".");
}

} // namespace

std::filesystem::path executableDirectory(const char* argv0) {
    std::error_code ignored;
    std::filesystem::path exe;
#if defined(_WIN32)
    wchar_t buffer[MAX_PATH];
    DWORD length = GetModuleFileNameW(nullptr, buffer, MAX_PATH);
    if (length > 0 && length < MAX_PATH) exe = std::filesystem::path(std::wstring(buffer, length));
#elif defined(__APPLE__)
    uint32_t size = 0;
    _NSGetExecutablePath(nullptr, &size); // asks for the buffer size
    std::string buffer(size, '\0');
    if (_NSGetExecutablePath(buffer.data(), &size) == 0) {
        exe = std::filesystem::canonical(buffer.c_str(), ignored);
    }
#else
    exe = std::filesystem::read_symlink("/proc/self/exe", ignored);
#endif
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
        if (std::filesystem::exists(dir / "life3d_step.comp.spv", ignored)) return dir;
    }
    return "shaders"; // relative to the working directory, as a last resort
}

std::filesystem::path userDataDirectory() {
    std::filesystem::path base;
#if defined(_WIN32)
    const char* appData = nonEmptyEnv("APPDATA");
    base = appData ? std::filesystem::path(appData) : std::filesystem::path(".");
#elif defined(__APPLE__)
    base = homeDirectory() / "Library" / "Application Support";
#else
    const char* xdgData = nonEmptyEnv("XDG_DATA_HOME");
    base = xdgData ? std::filesystem::path(xdgData) : homeDirectory() / ".local" / "share";
#endif
    std::filesystem::path dir = base / "gol3d";
    std::error_code ignored;
    std::filesystem::create_directories(dir, ignored);
    return dir;
}

std::filesystem::path settingsFilePath() {
#if defined(_WIN32)
    const char* appData = nonEmptyEnv("APPDATA");
    std::filesystem::path base =
        appData ? std::filesystem::path(appData) : std::filesystem::path(".");
#else
    const char* xdgConfig = nonEmptyEnv("XDG_CONFIG_HOME");
    std::filesystem::path base =
        xdgConfig ? std::filesystem::path(xdgConfig) : homeDirectory() / ".config";
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

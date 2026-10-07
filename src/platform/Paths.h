#pragma once

// Where the game finds its files and keeps the player's:
//   - Shaders: next to the executable (build tree, Windows), in
//     <prefix>/share/gol3d/shaders (Linux packages), or in Contents/Resources/shaders
//     (macOS app bundle).
//   - Saves and screenshots: $XDG_DATA_HOME/gol3d (default ~/.local/share/gol3d) on
//     Linux, ~/Library/Application Support/gol3d on macOS, %APPDATA%\gol3d on Windows.
//   - Settings (options.txt): $XDG_CONFIG_HOME/gol3d (default ~/.config/gol3d) on
//     Linux and macOS, %APPDATA%\gol3d on Windows.

#include <filesystem>
#include <string>

namespace gol3d {

// The folder holding the running executable (argv0 is the fallback when the OS
// cannot say).
std::filesystem::path executableDirectory(const char* argv0);

// The folder with the compiled .spv shaders: next to the executable in a build
// tree or a Windows install, under share/gol3d on Linux, in Resources inside the
// macOS app bundle.
std::filesystem::path findShaderDirectory(const std::filesystem::path& exeDir);

// Per-user folder for saves and screenshots, created if missing. Installed
// copies may start in a read-only working directory, so nothing is written there.
std::filesystem::path userDataDirectory();

// The settings file (not created here).
std::filesystem::path settingsFilePath();

// Where Ctrl+S saves the world and Ctrl+O loads it.
std::string saveFilePath();

// A new file name in the screenshots folder, e.g. life3d-20261005-142501.png.
std::string newScreenshotPath();

} // namespace gol3d

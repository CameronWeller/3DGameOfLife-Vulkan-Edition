// 3D Game of Life (gol3d): a Minecraft-style world where every block can be a
// cell of a 3D life-like automaton, simulated on the GPU with Vulkan.
//
// Start reading at docs/ARCHITECTURE.md, then game/Game.h.

#include <exception>
#include <iostream>

#include "game/CommandLine.h"
#include "game/Game.h"
#include "gpu/VulkanLoader.h"
#include "platform/Paths.h"

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX // keep windows.h from defining min and max macros
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <unistd.h>
#endif

int main(int argc, char** argv) {
    using namespace gol3d;
    try {
        ParsedCommandLine commandLine = parseCommandLine(argc, argv);
        switch (commandLine.action) {
            case ParsedCommandLine::Action::PrintHelp:
                printUsage();
                return 0;
            case ParsedCommandLine::Action::PrintVersion:
                std::cout << "gol3d " << GOL3D_VERSION_STRING << std::endl;
                return 0;
            case ParsedCommandLine::Action::Run:
                break;
        }

        const std::filesystem::path exeDir = executableDirectory(argv[0]);
        initVulkanLoader(exeDir);
        std::filesystem::path restartPath;
        int exitCode = 0;
        {
            // Scoped so the game shuts down completely before a restart.
            Game game(commandLine.options, findShaderDirectory(exeDir), exeDir);
            exitCode = game.run();
            restartPath = game.restartPath();
        }
#if !defined(_WIN32)
        if (!restartPath.empty()) {
            // An updated AppImage replaced this one in place; start it.
            execl(restartPath.c_str(), restartPath.c_str(), static_cast<char*>(nullptr));
            std::cerr << "Could not restart " << restartPath << std::endl;
        }
#endif
        return exitCode;
    } catch (const std::exception& error) {
        std::cerr << "Fatal error: " << error.what() << std::endl;
#if defined(_WIN32)
        // Release builds have no console window, so show the error.
        MessageBoxA(nullptr, error.what(), "3D Life", MB_OK | MB_ICONERROR);
#endif
        return 1;
    }
}

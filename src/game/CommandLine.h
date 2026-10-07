#pragma once

// Command-line options. Besides the player-facing ones (--rule, --seed, --load
// ...), the game takes scripted input (--pos, --look, --place ...) and exit
// conditions (--frames, --screenshot, --verify, --bench). Tests and the README
// media scripts use those to drive the real game without a person at the keys.

#include <cstdint>
#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "world/ChunkLayout.h"

namespace gol3d {

// One step of scripted input, applied in order before the first frame.
struct ScriptAction {
    enum Kind {
        Position, // vector = eye position
        Look,     // vector.x = yaw, vector.y = pitch (degrees)
        Place,    // place the selected stamp at the crosshair
        Break,    // remove the block at the crosshair
        Slot,     // number = hotbar slot, 1-based; 0 = empty hand
        Resize,   // vector.xy = window size
        Push,     // vector = movement to try, with collision
        Rotate,   // number = quarter turns around the placement surface
        Tilt,     // number = quarter turns around the world x axis
        Material, // number = a CellKind value
    };
    Kind kind;
    glm::vec3 vector{0.0f};
    int number = 0;
};

// Everything the command line can set, initialized to what applies when an
// option is not given.
struct Options {
    size_t rule = 0;   // index into lifeRules(); 0 is Life 5766
    uint32_t seed = 0; // random seed for soups (parseCommandLine picks one unless --seed)
    uint32_t chunkLimit = DEFAULT_CHUNK_LIMIT; // most chunks the world may use
    bool empty = false;                        // start without the rule's seed soup
    bool run = false;                          // start with the simulation running
    bool chunkBorders = false;
    bool debugOverlay = false;
    bool hideHud = false;
    int renderDistance = 0;   // render distance for this run only (0 = the setting)
    uint32_t warmupSteps = 0; // generations to advance before the first frame
    int speedExponent = 0;    // starting speed: 2^N generations per second
    bool fly = false;
    std::string loadPath;
    std::string savePath; // save the world here on exit
    std::string menu; // menu to open at start: pause, settings, inventory, newworld, tutorial[:N]
    std::string updateFeed; // releases JSON URL to check instead of GitHub (file:// works)

    // Test and capture hooks.
    std::string screenshotPath;    // save a PNG of the last frame
    uint32_t exitAfterFrames = 0;  // 0 = run until the window is closed
    bool verify = false;           // compare the GPU with the CPU reference and exit
    uint64_t benchGenerations = 0; // time this many generations at full speed and exit
    std::vector<ScriptAction> script;
};

// What main() should do: run the game with `options`, or print and exit.
struct ParsedCommandLine {
    enum class Action { Run, PrintHelp, PrintVersion };
    Action action = Action::Run;
    Options options;
};

// Throws std::runtime_error with a message for the player on bad input.
// Parsing stops at --help or --version.
ParsedCommandLine parseCommandLine(int argc, const char* const* argv);

// Prints the --help text to stdout.
void printUsage();

} // namespace gol3d

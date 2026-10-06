#include "game/CommandLine.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <random>
#include <sstream>
#include <stdexcept>

#include "game/Settings.h"
#include "life/CellTypes.h"
#include "life/LifeRules.h"

namespace gol3d {
namespace {

// Walks argv; value() consumes the argument after the current option.
class ArgumentReader {
public:
    ArgumentReader(int argc, const char* const* argv) : argc_(argc), argv_(argv) {}

    bool done() const { return index_ >= argc_; }
    std::string next() { return argv_[index_++]; }

    std::string value() {
        if (index_ >= argc_) {
            throw std::runtime_error(std::string("Missing value for ") + argv_[index_ - 1]);
        }
        return argv_[index_++];
    }

private:
    int argc_;
    const char* const* argv_;
    int index_ = 1; // argv[0] is the program
};

// "1,2,3" -> (1, 2, 3); the text must hold exactly `components` numbers.
glm::vec3 parseVector(const std::string& text, int components) {
    glm::vec3 vector(0.0f);
    std::stringstream stream(text);
    std::string part;
    int count = 0;
    while (std::getline(stream, part, ',') && count < 3) {
        vector[count++] = std::stof(part);
    }
    if (count != components) {
        throw std::runtime_error("Expected " + std::to_string(components) +
                                 " comma-separated numbers: " + text);
    }
    return vector;
}

// "stone" -> CellKind::Stone, matching names case-insensitively.
CellKind parseMaterial(const std::string& name) {
    for (const CellType& type : cellTypes()) {
        std::string lowerName = type.name;
        for (char& ch : lowerName) {
            ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
        }
        if (lowerName == name) return type.kind;
    }
    throw std::runtime_error("--material must be life, stone or ember");
}

// Options about the world and the session. Each parse*Option function returns
// false when `arg` is not one of its options.
bool parseWorldOption(const std::string& arg, ArgumentReader& args, Options& options) {
    if (arg == "--rule") {
        size_t rule = std::stoul(args.value());
        if (rule < 1 || rule > lifeRules().size()) {
            throw std::runtime_error("--rule is out of range");
        }
        options.rule = rule - 1;
    } else if (arg == "--seed") {
        options.seed = static_cast<uint32_t>(std::stoul(args.value()));
    } else if (arg == "--chunks") {
        uint32_t chunks = static_cast<uint32_t>(std::stoul(args.value()));
        options.chunkLimit = std::clamp<uint32_t>(chunks, MIN_CHUNK_LIMIT, MAX_CHUNK_SLOTS);
    } else if (arg == "--empty") {
        options.empty = true;
    } else if (arg == "--run") {
        options.run = true;
    } else if (arg == "--steps") {
        options.warmupSteps = static_cast<uint32_t>(std::stoul(args.value()));
    } else if (arg == "--speed") {
        double speed = std::stod(args.value());
        if (speed <= 0.0) throw std::runtime_error("--speed must be positive");
        options.speedExponent = static_cast<int>(std::lround(std::log2(speed)));
    } else if (arg == "--load") {
        options.loadPath = args.value();
    } else if (arg == "--save") {
        options.savePath = args.value();
    } else if (arg == "--fly") {
        options.fly = true;
    } else if (arg == "--menu") {
        options.menu = args.value();
    } else if (arg == "--update-feed") {
        options.updateFeed = args.value();
    } else {
        return false;
    }
    return true;
}

// Options about what is shown.
bool parseViewOption(const std::string& arg, ArgumentReader& args, Options& options) {
    if (arg == "--borders") {
        options.chunkBorders = true;
    } else if (arg == "--debug") {
        options.debugOverlay = true;
    } else if (arg == "--hide-hud") {
        options.hideHud = true;
    } else if (arg == "--view") {
        options.renderDistance = std::clamp(std::stoi(args.value()), Settings::MIN_RENDER_DISTANCE,
                                            Settings::MAX_RENDER_DISTANCE);
    } else {
        return false;
    }
    return true;
}

// Test and capture hooks.
bool parseTestOption(const std::string& arg, ArgumentReader& args, Options& options) {
    if (arg == "--screenshot") {
        options.screenshotPath = args.value();
    } else if (arg == "--frames") {
        options.exitAfterFrames = static_cast<uint32_t>(std::stoul(args.value()));
    } else if (arg == "--verify") {
        options.verify = true;
    } else if (arg == "--bench") {
        options.benchGenerations = std::stoull(args.value());
    } else {
        return false;
    }
    return true;
}

// Scripted input, kept in order.
bool parseScriptOption(const std::string& arg, ArgumentReader& args, Options& options) {
    using Kind = ScriptAction::Kind;
    auto addVector = [&](Kind kind, glm::vec3 vector = glm::vec3(0.0f)) {
        options.script.push_back(ScriptAction{kind, vector, 0});
    };
    auto addNumber = [&](Kind kind, int number) {
        options.script.push_back(ScriptAction{kind, glm::vec3(0.0f), number});
    };
    if (arg == "--pos") {
        addVector(Kind::Position, parseVector(args.value(), 3));
    } else if (arg == "--look") {
        addVector(Kind::Look, parseVector(args.value(), 2));
    } else if (arg == "--place") {
        addVector(Kind::Place);
    } else if (arg == "--break") {
        addVector(Kind::Break);
    } else if (arg == "--resize") {
        addVector(Kind::Resize, parseVector(args.value(), 2));
    } else if (arg == "--push") {
        addVector(Kind::Push, parseVector(args.value(), 3));
    } else if (arg == "--slot") {
        addNumber(Kind::Slot, std::stoi(args.value()));
    } else if (arg == "--rotate") {
        addNumber(Kind::Rotate, std::stoi(args.value()));
    } else if (arg == "--tilt") {
        addNumber(Kind::Tilt, std::stoi(args.value()));
    } else if (arg == "--material") {
        addNumber(Kind::Material, static_cast<int>(parseMaterial(args.value())));
    } else {
        return false;
    }
    return true;
}

} // namespace

ParsedCommandLine parseCommandLine(int argc, const char* const* argv) {
    ParsedCommandLine parsed;
    Options& options = parsed.options;
    options.seed = std::random_device{}();
    ArgumentReader args(argc, argv);

    while (!args.done()) {
        const std::string arg = args.next();
        if (arg == "--help" || arg == "-h") {
            parsed.action = ParsedCommandLine::Action::PrintHelp;
            return parsed;
        }
        if (arg == "--version") {
            parsed.action = ParsedCommandLine::Action::PrintVersion;
            return parsed;
        }
        bool known = parseWorldOption(arg, args, options) || parseViewOption(arg, args, options) ||
                     parseTestOption(arg, args, options) || parseScriptOption(arg, args, options);
        if (!known) throw std::runtime_error("Unknown option: " + arg);
    }

    // A screenshot is taken of the last frame, so a run that saves one must end.
    if (!options.screenshotPath.empty() && options.exitAfterFrames == 0) {
        options.exitAfterFrames = 3;
    }
    return parsed;
}

void printUsage() {
    // Options and what they do, printed in two columns.
    struct OptionHelp {
        std::string option;
        std::string description;
    };
    const std::string ruleRange = "1-" + std::to_string(lifeRules().size());
    const std::string chunkRange =
        std::to_string(MIN_CHUNK_LIMIT) + "-" + std::to_string(MAX_CHUNK_SLOTS);
    const std::string viewRange = std::to_string(Settings::MIN_RENDER_DISTANCE) + "-" +
                                  std::to_string(Settings::MAX_RENDER_DISTANCE);
    const std::vector<OptionHelp> options = {
        {"--rule N", "starting rule " + ruleRange + " (default 1, Life 5766)"},
        {"--seed N", "random seed for soups"},
        {"--chunks N", "most chunks the world may use, " + chunkRange + " (default " +
                           std::to_string(DEFAULT_CHUNK_LIMIT) + "; 32^3 cells each)"},
        {"--empty", "start with an empty world"},
        {"--run", "start with the simulation running"},
        {"--steps N", "advance N generations before the first frame"},
        {"--speed N", "start at N generations per second (rounded to a power of two)"},
        {"--bench N", "advance N generations as fast as possible, print the rate and exit"},
        {"--borders", "show chunk borders (F3+G in game)"},
        {"--debug", "show the debug overlay (F3 in game)"},
        {"--hide-hud", "start with the HUD hidden (F1 in game)"},
        {"--view N", "render distance in blocks for this run, " + viewRange},
        {"--screenshot PATH", "save a PNG of the last frame (implies --frames 3)"},
        {"--frames N", "exit after N frames"},
        {"--verify", "check the GPU against the CPU reference and exit"},
        {"--load PATH", "open a saved world (Ctrl+S saves world.life3d in the user data folder)"},
        {"--fly", "start flying instead of walking"},
        {"--menu NAME", "open pause, settings, inventory, newworld or tutorial[:LESSON] at start"},
        {"--update-feed URL", "check this releases JSON for updates (testing)"},
        {"--version", "print the version and exit"},
        {"--save PATH", "save the world to PATH on exit"},
    };
    const std::vector<OptionHelp> scriptOptions = {
        {"--push DX,DY,DZ", "move the player with collision"},
        {"--rotate N", "rotate the stamp N quarter turns (like pressing E N times)"},
        {"--tilt N", "tilt the stamp N quarter turns around x (like pressing C N times)"},
        {"--material NAME", "build with life, stone or ember (like pressing M)"},
    };
    auto printOptions = [](const std::vector<OptionHelp>& list) {
        for (const OptionHelp& help : list) {
            std::cout << "  " << std::left << std::setw(18) << help.option << help.description
                      << "\n";
        }
    };

    std::cout << "Usage: gol3d [options]\n";
    printOptions(options);
    std::cout << "Scripted input (applied in order, for tests):\n"
              << "  --pos X,Y,Z  --look YAW,PITCH  --slot N (0 = empty hand)  --place  --break  "
                 "--resize W,H\n";
    printOptions(scriptOptions);
    std::cout << "Environment: GOL3D_VALIDATION=1 enables Vulkan validation; GOL3D_GPU=TEXT picks "
                 "a GPU by name.\n";
}

} // namespace gol3d

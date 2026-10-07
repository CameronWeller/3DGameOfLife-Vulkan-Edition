// Checks for src/game/Settings.h (options.txt) and src/game/CommandLine.h.

#include <filesystem>
#include <fstream>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

#include "TestSupport.h"
#include "game/CommandLine.h"
#include "game/Settings.h"
#include "life/CellTypes.h"

using namespace gol3d;
using testing::expect;

namespace {

// A path in a folder of its own under the system temp folder, unique to this
// run, so saveSettings has to create the folder.
std::filesystem::path tempFile(const std::string& name) {
    static const std::string RUN_ID = std::to_string(std::random_device{}());
    return std::filesystem::temp_directory_path() / ("gol3d-test-" + RUN_ID) / name;
}

void checkSettingsFile() {
    const std::filesystem::path path = tempFile("options.txt");
    Settings defaults = loadSettings(path);
    expect(defaults.fov == Settings{}.fov && defaults.renderDistance == Settings{}.renderDistance,
           "a missing file gives the defaults");

    Settings changed;
    changed.fov = 90.0f;
    changed.sensitivity = 150;
    changed.invertY = true;
    changed.renderDistance = 512;
    changed.guiScale = 2;
    changed.checkUpdates = false;
    changed.smoothLighting = false;
    changed.animate = false;
    changed.simBudget = 12;
    saveSettings(changed, path); // also creates the folder
    Settings loaded = loadSettings(path);
    expect(loaded.fov == 90.0f && loaded.sensitivity == 150 && loaded.invertY &&
               loaded.renderDistance == 512 && loaded.guiScale == 2 && !loaded.checkUpdates &&
               !loaded.smoothLighting && !loaded.animate && loaded.simBudget == 12,
           "every setting survives a save and load");

    // A damaged file: too large, not a number, not a setting, unknown, too small.
    {
        std::ofstream out(path);
        out << "fov:500\nsensitivity:banana\nno colon here\nfutureSetting:1\nsimBudget:1\n";
    }
    loaded = loadSettings(path);
    expect(loaded.fov == Settings::MAX_FOV, "out-of-range values are clamped");
    expect(loaded.sensitivity == Settings{}.sensitivity, "malformed values keep the default");
    expect(loaded.simBudget == Settings::MIN_SIM_BUDGET, "low values are clamped too");
    std::filesystem::remove_all(path.parent_path());
}

// Parses the arguments as if typed after "gol3d".
ParsedCommandLine parse(std::vector<const char*> args) {
    args.insert(args.begin(), "gol3d");
    return parseCommandLine(static_cast<int>(args.size()), args.data());
}

bool throws(std::vector<const char*> args) {
    try {
        parse(std::move(args));
    } catch (const std::exception&) {
        return true;
    }
    return false;
}

void checkCommandLine() {
    Options options =
        parse({"--rule", "2", "--seed", "42", "--empty", "--fly", "--speed", "64"}).options;
    expect(options.rule == 1 && options.seed == 42 && options.empty && options.fly,
           "basic options (--rule counts from 1, Options::rule from 0)");
    expect(options.speedExponent == 6, "--speed 64 is 2^6 generations per second");
    expect(parse({"--chunks", "10"}).options.chunkLimit == 64,
           "--chunks is clamped to at least 64");
    expect(parse({"--screenshot", "x.png"}).options.exitAfterFrames == 3,
           "a screenshot run ends after 3 frames");
    expect(parse({"--view", "5000"}).options.renderDistance == 1024, "--view is clamped");

    Options scripted = parse({"--pos", "1,2,3", "--look", "90,-10", "--slot", "5", "--material",
                              "stone", "--place"})
                           .options;
    expect(scripted.script.size() == 5, "scripted actions are kept in order");
    if (scripted.script.size() == 5) {
        expect(scripted.script[0].kind == ScriptAction::Position &&
                   scripted.script[0].vector == glm::vec3(1, 2, 3),
               "--pos");
        expect(scripted.script[1].vector.x == 90.0f && scripted.script[1].vector.y == -10.0f,
               "--look");
        expect(scripted.script[3].kind == ScriptAction::Material &&
                   scripted.script[3].number == static_cast<int>(CellKind::Stone),
               "--material by name");
        expect(scripted.script[4].kind == ScriptAction::Place, "--place");
    }

    expect(parse({"--help"}).action == ParsedCommandLine::Action::PrintHelp, "--help");
    expect(parse({"--version", "--bogus"}).action == ParsedCommandLine::Action::PrintVersion,
           "parsing stops at --version");
    expect(throws({"--rule", "99"}), "an out-of-range rule is an error");
    expect(throws({"--speed", "0"}), "a zero speed is an error");
    expect(throws({"--pos", "1,2"}), "a short vector is an error");
    expect(throws({"--material", "gold"}), "an unknown material is an error");
    expect(throws({"--seed"}), "a missing value is an error");
    expect(throws({"--bogus"}), "an unknown option is an error");
}

} // namespace

int main() {
    checkSettingsFile();
    checkCommandLine();
    return testing::finish("settings and command line");
}

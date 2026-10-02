// 3D Game of Life - playable Vulkan prototype with a Minecraft-style world.
//
// The world is unbounded and stored as 32^3-cell chunks, one bit per cell, that
// exist only where life is (or could be within a few generations).
// shaders/life3d_step.comp advances every active chunk a generation with
// bit-sliced arithmetic; after the last generation of a batch,
// shaders/life3d_build.comp lists the visible blocks for an indirect instanced
// draw and reports per-chunk population plus which neighbor chunks life can
// reach, and the CPU allocates and frees chunks to follow the pattern. A
// governor sizes each frame's simulation work to the time it measures, so huge
// worlds slow the tick rate instead of the frame rate. Controls follow
// Minecraft's defaults; see printControls().
//
// Build with GLM_FORCE_DEPTH_ZERO_TO_ONE defined for the whole target (a
// precompiled header may include GLM before this file).

#include <algorithm>
#include <array>
#include <bit>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <memory>
#include <optional>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#if defined(_WIN32)
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#elif defined(__APPLE__)
#include <dlfcn.h>
#include <mach-o/dyld.h>
#include <unistd.h>
#else
#include <unistd.h>
#endif

#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

static_assert(GLM_CONFIG_CLIP_CONTROL & GLM_CLIP_CONTROL_ZO_BIT,
              "Define GLM_FORCE_DEPTH_ZERO_TO_ONE for this target: Vulkan depth is 0..1");

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_vulkan.h"

#include "CellTypes.h"
#include "ChunkMap.h"
#include "GpuContext.h"
#include "Life3DRules.h"
#include "Life3DPatterns.h"
#include "MenuFont.h"
#include "Updater.h"
#include "tutorial/Tutorial.h"

using namespace VulkanHIP;

namespace {

// World storage; must match shaders/life3d_storage.glsl.
constexpr int CHUNK = 32;                  // cells along a chunk edge
constexpr int CHUNK_SHIFT = 5;
constexpr uint32_t CHUNK_ROWS = 1024;      // one 32-bit row of cells per (y, z)
constexpr uint32_t BLOCK_ROWS = 2048;      // a block-pool slot: "blocked" rows, then "emits" rows
constexpr uint32_t NO_CHUNK = 0xFFFFFFFFu;
constexpr uint32_t MAX_CHUNK_SLOTS = 1u << 17; // the instance format has 17 bits for the slot
constexpr uint32_t DEFAULT_CHUNK_LIMIT = 32768; // 1.07 billion cells, 256 MB of cell storage
constexpr uint32_t INITIAL_CHUNKS = 512;   // the pool doubles as the world grows
constexpr uint32_t INITIAL_BLOCK_SLOTS = 64;
// Generations per GPU submission. life3d_build.comp flags neighbor chunks for
// live cells within this many cells of a face, and life spreads at most one
// cell per generation, so that many generations can run before the CPU must
// add chunks.
constexpr uint32_t MAX_BATCH = 8;
constexpr uint32_t MAX_DISPATCH = 65535;   // workgroups per dispatch dimension the spec guarantees
constexpr uint32_t MAX_INSTANCES = 1u << 22; // drawn blocks (8 bytes each); more are simulated but not drawn
constexpr uint32_t BLOCK_INDICES = 18;     // three camera-facing quads per block, 12 vertices (life3d_blocks.vert)
constexpr uint32_t MAX_BOXES = 4096;       // outline boxes per frame
constexpr uint64_t GPU_TIMEOUT_NS = 4'000'000'000; // treat a longer wait as a lost device

glm::ivec3 chunkOf(const glm::ivec3& cell) { return cell >> CHUNK_SHIFT; } // floor division by 32
// Row of a cell within its chunk, and its bit in that row.
uint32_t rowOf(const glm::ivec3& cell) {
    glm::ivec3 l = cell & (CHUNK - 1);
    return static_cast<uint32_t>(l.z * CHUNK + l.y);
}
uint32_t bitOf(const glm::ivec3& cell) { return 1u << (cell.x & (CHUNK - 1)); }
glm::ivec3 neighborOffset(int k) { return glm::ivec3(k % 3 - 1, (k / 3) % 3 - 1, k / 9 - 1); }

// Scripted input for end-to-end checks, applied in order before the first frame.
struct ScriptAction {
    enum Kind { Position, Look, Place, Break, Slot, Resize, Push, Rotate, Tilt, Material } kind;
    glm::vec3 value{0.0f};
};

struct Options {
    size_t rule = 0; // Life 5766, the closest 3D analog of Conway's Life
    uint32_t seed = std::random_device{}();
    uint32_t chunkLimit = DEFAULT_CHUNK_LIMIT;
    bool empty = false;
    bool run = false;
    bool chunkBorders = false;
    bool debugOverlay = false;
    bool hideHud = false;
    int renderDistance = 0; // --view: render distance for this run (0 = the setting)
    uint32_t warmupSteps = 0;
    std::string screenshotPath;
    uint32_t exitAfterFrames = 0;
    bool verify = false;
    uint64_t benchGenerations = 0; // --bench: time this many generations at full speed, then exit
    int speedExponent = 0;         // simulation speed 2^N generations per second
    std::string loadPath;
    std::string savePath;
    bool fly = false;
    std::string menu; // open a menu at start (for screenshots): pause, settings, inventory, newworld, tutorial[:N]
    std::string updateFeed; // test hook: releases JSON URL (file:// works) instead of GitHub
    std::vector<ScriptAction> script;
};

void printUsage() {
    std::cout << "Usage: gol3d [options]\n"
                 "  --rule N          starting rule 1-" << lifeRules().size() << " (default 1, Life 5766)\n"
                 "  --seed N          random seed for soups\n"
                 "  --chunks N        most chunks the world may use, 64-131072 (default 32768; 32^3 cells each)\n"
                 "  --empty           start with an empty world\n"
                 "  --run             start with the simulation running\n"
                 "  --steps N         advance N generations before the first frame\n"
                 "  --speed N         start at N generations per second (rounded to a power of two)\n"
                 "  --bench N         advance N generations as fast as possible, print the rate and exit\n"
                 "  --borders         show chunk borders (F3+G in game)\n"
                 "  --debug           show the debug overlay (F3 in game)\n"
                 "  --hide-hud        start with the HUD hidden (F1 in game)\n"
                 "  --view N          render distance in blocks for this run, 64-1024\n"
                 "  --screenshot PATH save a PNG of the last frame (implies --frames 3)\n"
                 "  --frames N        exit after N frames\n"
                 "  --verify          check the GPU against the CPU reference and exit\n"
                 "  --load PATH       open a saved world (Ctrl+S saves world.life3d in the user data folder)\n"
                 "  --fly             start flying instead of walking\n"
                 "  --menu NAME       open pause, settings, inventory, newworld or tutorial[:LESSON] at start\n"
                 "  --update-feed URL check this releases JSON for updates (testing)\n"
                 "  --version         print the version and exit\n"
                 "  --save PATH       save the world to PATH on exit\n"
                 "Scripted input (applied in order, for tests):\n"
                 "  --pos X,Y,Z  --look YAW,PITCH  --slot N (0 = empty hand)  --place  --break  --resize W,H\n"
                 "  --push DX,DY,DZ   move the player with collision\n"
                 "  --rotate N        rotate the stamp N quarter turns (like pressing E N times)\n"
                 "  --tilt N          tilt the stamp N quarter turns around x (like pressing C N times)\n"
                 "  --material NAME   build with life, stone or ember (like pressing M)\n"
                 "Environment: GOL3D_VALIDATION=1 enables Vulkan validation; GOL3D_GPU=TEXT picks a GPU by name.\n";
}

glm::vec3 parseVector(const std::string& text, int components) {
    glm::vec3 v(0.0f);
    std::stringstream stream(text);
    std::string part;
    int i = 0;
    while (std::getline(stream, part, ',') && i < 3) v[i++] = std::stof(part);
    if (i != components) throw std::runtime_error("Expected " + std::to_string(components) + " comma-separated numbers: " + text);
    return v;
}

Options parseOptions(int argc, char** argv) {
    Options options;
    auto value = [&](int& i) -> std::string {
        if (i + 1 >= argc) throw std::runtime_error(std::string("Missing value for ") + argv[i]);
        return argv[++i];
    };
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--rule") {
            size_t rule = std::stoul(value(i));
            if (rule < 1 || rule > lifeRules().size()) throw std::runtime_error("--rule is out of range");
            options.rule = rule - 1;
        }
        else if (arg == "--seed") options.seed = static_cast<uint32_t>(std::stoul(value(i)));
        else if (arg == "--chunks") options.chunkLimit = std::clamp<uint32_t>(std::stoul(value(i)), 64, MAX_CHUNK_SLOTS);
        else if (arg == "--empty") options.empty = true;
        else if (arg == "--run") options.run = true;
        else if (arg == "--borders") options.chunkBorders = true;
        else if (arg == "--debug") options.debugOverlay = true;
        else if (arg == "--hide-hud") options.hideHud = true;
        else if (arg == "--view") options.renderDistance = std::clamp(std::stoi(value(i)), 64, 1024);
        else if (arg == "--steps") options.warmupSteps = static_cast<uint32_t>(std::stoul(value(i)));
        else if (arg == "--screenshot") options.screenshotPath = value(i);
        else if (arg == "--frames") options.exitAfterFrames = static_cast<uint32_t>(std::stoul(value(i)));
        else if (arg == "--verify") options.verify = true;
        else if (arg == "--bench") options.benchGenerations = std::stoull(value(i));
        else if (arg == "--speed") {
            double speed = std::stod(value(i));
            if (speed <= 0.0) throw std::runtime_error("--speed must be positive");
            options.speedExponent = static_cast<int>(std::lround(std::log2(speed)));
        }
        else if (arg == "--load") options.loadPath = value(i);
        else if (arg == "--fly") options.fly = true;
        else if (arg == "--menu") options.menu = value(i);
        else if (arg == "--update-feed") options.updateFeed = value(i);
        else if (arg == "--version") { std::cout << "gol3d " << GOL3D_VERSION_STRING << std::endl; std::exit(0); }
        else if (arg == "--save") options.savePath = value(i);
        else if (arg == "--pos") options.script.push_back({ScriptAction::Position, parseVector(value(i), 3)});
        else if (arg == "--look") options.script.push_back({ScriptAction::Look, parseVector(value(i), 2)});
        else if (arg == "--slot") options.script.push_back({ScriptAction::Slot, glm::vec3(std::stof(value(i)))});
        else if (arg == "--place") options.script.push_back({ScriptAction::Place});
        else if (arg == "--break") options.script.push_back({ScriptAction::Break});
        else if (arg == "--resize") options.script.push_back({ScriptAction::Resize, parseVector(value(i), 2)});
        else if (arg == "--push") options.script.push_back({ScriptAction::Push, parseVector(value(i), 3)});
        else if (arg == "--rotate") options.script.push_back({ScriptAction::Rotate, glm::vec3(std::stof(value(i)))});
        else if (arg == "--tilt") options.script.push_back({ScriptAction::Tilt, glm::vec3(std::stof(value(i)))});
        else if (arg == "--material") {
            std::string name = value(i);
            int kind = -1;
            for (const CellType& type : cellTypes()) {
                std::string lower = type.name;
                for (char& ch : lower) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
                if (lower == name) kind = static_cast<int>(type.kind);
            }
            if (kind < 0) throw std::runtime_error("--material must be life, stone or ember");
            options.script.push_back({ScriptAction::Material, glm::vec3(static_cast<float>(kind))});
        }
        else if (arg == "--help" || arg == "-h") { printUsage(); std::exit(0); }
        else throw std::runtime_error("Unknown option: " + arg);
    }
    if (!options.screenshotPath.empty() && options.exitAfterFrames == 0) options.exitAfterFrames = 3;
    return options;
}

std::filesystem::path executableDirectory(const char* argv0) {
    std::error_code ec;
    std::filesystem::path exe;
#if defined(_WIN32)
    wchar_t buffer[MAX_PATH];
    DWORD length = GetModuleFileNameW(nullptr, buffer, MAX_PATH);
    if (length > 0 && length < MAX_PATH) exe = std::filesystem::path(std::wstring(buffer, length));
#elif defined(__APPLE__)
    uint32_t size = 0;
    _NSGetExecutablePath(nullptr, &size);
    std::string buffer(size, '\0');
    if (_NSGetExecutablePath(buffer.data(), &size) == 0) exe = std::filesystem::canonical(buffer.c_str(), ec);
#else
    exe = std::filesystem::read_symlink("/proc/self/exe", ec);
#endif
    if (exe.empty()) exe = std::filesystem::absolute(argv0, ec);
    return exe.parent_path();
}

// Compiled shaders sit next to the executable in a build or Windows install,
// in share/gol3d on Linux, and in Resources inside the macOS app bundle.
std::filesystem::path findShaderDirectory(const std::filesystem::path& exeDir) {
    std::error_code ec;
    for (const std::filesystem::path& dir : {exeDir / "shaders", exeDir / ".." / "share" / "gol3d" / "shaders",
                                             exeDir / ".." / "Resources" / "shaders"}) {
        if (std::filesystem::exists(dir / "life3d_step.comp.spv", ec)) return dir;
    }
    return "shaders";
}

// Vulkan is opened at runtime (volk), so one binary starts on any machine with a
// Vulkan driver. On macOS the app bundle carries MoltenVK.
void initVulkanLoader([[maybe_unused]] const std::filesystem::path& exeDir) {
    bool loaded = false;
#if defined(__APPLE__)
    for (const std::filesystem::path& candidate : {exeDir / ".." / "Frameworks" / "libMoltenVK.dylib", exeDir / "libMoltenVK.dylib"}) {
        if (void* library = dlopen(candidate.c_str(), RTLD_NOW | RTLD_LOCAL)) {
            auto getProcAddr = reinterpret_cast<PFN_vkGetInstanceProcAddr>(dlsym(library, "vkGetInstanceProcAddr"));
            if (getProcAddr) {
                volkInitializeCustom(getProcAddr);
                loaded = true;
                break;
            }
        }
    }
#endif
    if (!loaded && volkInitialize() != VK_SUCCESS) {
        throw std::runtime_error("Vulkan is not available. Install a GPU driver with Vulkan support "
                                 "(on Linux also the Vulkan loader, e.g. libvulkan1 or vulkan-icd-loader).");
    }
#if GLFW_VERSION_MAJOR > 3 || (GLFW_VERSION_MAJOR == 3 && GLFW_VERSION_MINOR >= 4)
    glfwInitVulkanLoader(vkGetInstanceProcAddr); // GLFW uses the same loader for surfaces
#endif
}

uint32_t crc32(const uint8_t* data, size_t size) {
    static const std::array<uint32_t, 256> table = [] {
        std::array<uint32_t, 256> t{};
        for (uint32_t n = 0; n < 256; ++n) {
            uint32_t c = n;
            for (int k = 0; k < 8; ++k) c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
            t[n] = c;
        }
        return t;
    }();
    uint32_t crc = 0xFFFFFFFFu;
    for (size_t i = 0; i < size; ++i) crc = table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
    return ~crc;
}

// Minimal RGB PNG writer using stored (uncompressed) deflate blocks.
bool writePng(const std::string& path, uint32_t width, uint32_t height, const std::vector<uint8_t>& rgb) {
    std::vector<uint8_t> raw;
    raw.reserve((width * 3 + 1) * height);
    for (uint32_t y = 0; y < height; ++y) {
        raw.push_back(0);
        raw.insert(raw.end(), rgb.begin() + y * width * 3, rgb.begin() + (y + 1) * width * 3);
    }
    std::vector<uint8_t> zlib = {0x78, 0x01};
    size_t pos = 0;
    do {
        size_t len = std::min<size_t>(65535, raw.size() - pos);
        zlib.push_back(pos + len == raw.size() ? 1 : 0);
        zlib.push_back(len & 0xFF);
        zlib.push_back((len >> 8) & 0xFF);
        zlib.push_back(~len & 0xFF);
        zlib.push_back((~len >> 8) & 0xFF);
        zlib.insert(zlib.end(), raw.begin() + pos, raw.begin() + pos + len);
        pos += len;
    } while (pos < raw.size());
    uint32_t a = 1, b = 0;
    for (uint8_t v : raw) { a = (a + v) % 65521; b = (b + a) % 65521; }
    for (int shift : {24, 16, 8, 0}) zlib.push_back(((b << 16 | a) >> shift) & 0xFF);

    std::ofstream out(path, std::ios::binary);
    if (!out) return false;
    auto be32 = [&](uint32_t v) {
        const char bytes[4] = {char(v >> 24), char(v >> 16), char(v >> 8), char(v)};
        out.write(bytes, 4);
    };
    auto chunk = [&](const char* type, const std::vector<uint8_t>& data) {
        std::vector<uint8_t> typed(type, type + 4);
        typed.insert(typed.end(), data.begin(), data.end());
        be32(static_cast<uint32_t>(data.size()));
        out.write(reinterpret_cast<const char*>(typed.data()), typed.size());
        be32(crc32(typed.data(), typed.size()));
    };
    out.write("\x89PNG\r\n\x1a\n", 8);
    std::vector<uint8_t> header;
    for (uint32_t v : {width, height}) for (int shift : {24, 16, 8, 0}) header.push_back((v >> shift) & 0xFF);
    header.insert(header.end(), {8, 2, 0, 0, 0}); // 8-bit RGB
    chunk("IHDR", header);
    chunk("IDAT", zlib);
    chunk("IEND", {});
    return static_cast<bool>(out);
}

// Per-user folder for saves and screenshots; installed apps may start in a
// read-only working directory.
std::filesystem::path userDataDirectory() {
    const char* home = std::getenv("HOME");
    std::filesystem::path base;
#if defined(_WIN32)
    const char* appData = std::getenv("APPDATA");
    base = appData && *appData ? std::filesystem::path(appData) : std::filesystem::path(".");
#elif defined(__APPLE__)
    base = std::filesystem::path(home ? home : ".") / "Library" / "Application Support";
#else
    const char* xdg = std::getenv("XDG_DATA_HOME");
    base = xdg && *xdg ? std::filesystem::path(xdg) : std::filesystem::path(home ? home : ".") / ".local" / "share";
#endif
    std::filesystem::path dir = base / "gol3d";
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    return dir;
}

std::string timestampedScreenshotName() {
    std::time_t now = std::time(nullptr);
    std::ostringstream name;
    name << "life3d-" << std::put_time(std::localtime(&now), "%Y%m%d-%H%M%S") << ".png";
    std::filesystem::path dir = userDataDirectory() / "screenshots";
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    return (dir / name.str()).string();
}

// Hotbar stamps; their order matches the icons in shaders/life3d_screen.frag.
enum class Stamp { Cell, Block, Plus, SmallSoup, BigSoup, Wall, Pillar, RuleSeed, Glider };
constexpr std::array<const char*, 9> STAMP_NAMES = {
    "Cell", "Block 2x2x2", "Plus", "Soup 8^3", "Soup 16^3", "Wall 5x5", "Pillar 8", "Rule seed", "Glider"};
constexpr std::array<const char*, 9> STAMP_DESCRIPTIONS = {
    "One live cell.",
    "A solid 2x2x2 cube.",
    "A 3D cross of seven cells.",
    "An 8x8x8 random soup at the rule's density.",
    "A 16x16x16 random soup at the rule's density.",
    "A solid 5x5 wall. On the ground it stands upright.",
    "A line of 8 cells growing away from the surface.",
    "The current rule's own starting soup.",
    "Bays' glider (Life 4555's under that rule, Life 5766's otherwise). It lies flat on the ground and stands "
    "up on a wall; Q/E pick its heading, Z/C tilt it to climb or dive."};
// Same 5x5 bitmaps as ICONS in shaders/life3d_screen.frag (bit 24 = top-left).
constexpr std::array<uint32_t, 9> STAMP_ICONS = {
    0x0001000u, 0x00739C0u, 0x0023880u, 0x0051120u, 0x165E9B6u, 0x1FFFFFFu, 0x0421084u, 0x1555555u, 0x00209C0u};

// Player-adjustable options, stored as "key:value" lines like Minecraft's options.txt.
struct Settings {
    float fov = 70.0f;       // degrees, vertical
    int sensitivity = 100;   // percent of 0.15 degrees per pixel
    bool invertY = false;
    int renderDistance = 256; // blocks; fog ends here
    int guiScale = 0;        // 0 = automatic
    bool checkUpdates = true; // ask GitHub for a newer release at startup
    bool smoothLighting = true; // ambient occlusion at block corners
    bool animate = true;        // births grow in and deaths shrink away at slow speeds
    int simBudget = 8;          // most milliseconds of simulation per frame (the governor's budget)
};

std::filesystem::path settingsPath() {
#if defined(_WIN32)
    const char* appData = std::getenv("APPDATA");
    std::filesystem::path base = appData && *appData ? std::filesystem::path(appData) : std::filesystem::path(".");
#else
    const char* xdg = std::getenv("XDG_CONFIG_HOME");
    const char* home = std::getenv("HOME");
    std::filesystem::path base = xdg && *xdg ? std::filesystem::path(xdg)
                                             : std::filesystem::path(home ? home : ".") / ".config";
#endif
    return base / "gol3d" / "options.txt";
}

Settings loadSettings() {
    Settings settings;
    std::ifstream in(settingsPath());
    std::string line;
    while (std::getline(in, line)) {
        size_t colon = line.find(':');
        if (colon == std::string::npos) continue;
        std::string key = line.substr(0, colon), value = line.substr(colon + 1);
        try {
            if (key == "fov") settings.fov = std::clamp(std::stof(value), 30.0f, 110.0f);
            else if (key == "sensitivity") settings.sensitivity = std::clamp(std::stoi(value), 10, 300);
            else if (key == "invertY") settings.invertY = value == "true";
            else if (key == "renderDistance") settings.renderDistance = std::clamp(std::stoi(value), 64, 1024);
            else if (key == "guiScale") settings.guiScale = std::clamp(std::stoi(value), 0, 4);
            else if (key == "checkUpdates") settings.checkUpdates = value != "false";
            else if (key == "smoothLighting") settings.smoothLighting = value != "false";
            else if (key == "animate") settings.animate = value != "false";
            else if (key == "simBudget") settings.simBudget = std::clamp(std::stoi(value), 2, 40);
        } catch (const std::exception&) {
            // Ignore malformed lines and keep the default.
        }
    }
    return settings;
}

void saveSettings(const Settings& settings) {
    std::error_code ec;
    std::filesystem::create_directories(settingsPath().parent_path(), ec);
    std::ofstream out(settingsPath());
    out << "fov:" << settings.fov << "\nsensitivity:" << settings.sensitivity
        << "\ninvertY:" << (settings.invertY ? "true" : "false") << "\nrenderDistance:" << settings.renderDistance
        << "\nguiScale:" << settings.guiScale
        << "\ncheckUpdates:" << (settings.checkUpdates ? "true" : "false")
        << "\nsmoothLighting:" << (settings.smoothLighting ? "true" : "false")
        << "\nanimate:" << (settings.animate ? "true" : "false") << "\nsimBudget:" << settings.simBudget << "\n";
}

} // namespace

class LifePrototypeApp {
public:
    LifePrototypeApp(Options options, std::filesystem::path shaderDir, std::filesystem::path exeDir)
        : options(std::move(options)), shaderDir(std::move(shaderDir)), exeDir(std::move(exeDir)), rng(this->options.seed) {
        ruleIndex = this->options.rule;
        simulationRunning = this->options.run;
        showChunkBorders = this->options.chunkBorders;
        showDebug = this->options.debugOverlay;
        hudVisible = !this->options.hideHud;
        chunkLimit = this->options.chunkLimit;
        speedExponent = std::clamp(this->options.speedExponent, MIN_SPEED_EXPONENT, UNLIMITED_SPEED_EXPONENT);
        // Scripted and test runs use defaults so results do not depend on the player's options.
        persistSettings = this->options.script.empty() && this->options.screenshotPath.empty() &&
                          !this->options.verify && this->options.menu.empty() && !this->options.benchGenerations;
        if (persistSettings) settings = loadSettings();
        if (this->options.renderDistance) settings.renderDistance = this->options.renderDistance;
        // Screenshots of scripted runs show finished states, not cells mid-animation.
        capturing = this->options.exitAfterFrames != 0;
    }

    ~LifePrototypeApp() {
        cleanup();
    }

    // Set when an AppImage was updated in place and the player chose to restart.
    std::filesystem::path restartPath;

    int run() {
        initWindow();
        initVulkan();
        initRendering();
        if (options.verify) return verifyAgainstReference() ? 0 : 1;
        initImGui();
        newWorld(!options.empty);
        if (!options.loadPath.empty() && !loadWorld(options.loadPath)) return 1;
        flying = options.fly;
        advanceGenerations(options.warmupSteps);
        if (options.benchGenerations) return runBenchmark(options.benchGenerations);
        for (const ScriptAction& action : options.script) applyScriptAction(action);
        printControls();
        setCursorCaptured(options.script.empty() && options.screenshotPath.empty() && options.menu.empty());
        if (options.menu == "pause") openPauseMenu();
        else if (options.menu == "settings") { openPauseMenu(); screen = Screen::Settings; }
        else if (options.menu == "inventory") openInventory();
        else if (options.menu == "newworld") { openPauseMenu(); openNewWorldScreen(); }
        else if (options.menu.rfind("tutorial", 0) == 0) {
            // --menu tutorial:N opens lesson N; --steps then advances the lesson's scene.
            openTutorial(options.menu.size() > 9 ? std::stoul(options.menu.substr(9)) - 1 : 0);
            advanceGenerations(options.warmupSteps);
        }
        else if (!options.menu.empty()) throw std::runtime_error("Unknown --menu " + options.menu);
        if (!options.updateFeed.empty() || (persistSettings && settings.checkUpdates)) {
            updater = std::make_unique<gol3d::Updater>(GOL3D_VERSION_STRING, exeDir,
                                                       options.updateFeed.empty() ? gol3d::RELEASES_API_LATEST : options.updateFeed);
            updater->checkAsync();
        }
        mainLoop();
        if (persistSettings) saveSettings(settings);
        if (!options.savePath.empty() && !saveWorld(options.savePath)) return 1;
        return 0;
    }

private:
    struct Vertex {
        glm::vec3 pos;
        glm::vec3 normal;
    };

    // Matches the std140 Frame block in shaders/life3d_frame.glsl.
    struct FrameUniforms {
        glm::mat4 viewProj;
        glm::mat4 invViewProj;
        glm::vec4 camera;
        glm::vec4 viewport;
        glm::ivec4 hotbar;
        glm::vec4 fog;
        glm::vec4 anim;
        glm::vec4 sun;
    };

    // Push constants of shaders/life3d_step.comp and shaders/life3d_build.comp.
    struct StepConstants {
        uint32_t firstIndex;
        uint32_t count;
        uint32_t surviveMask;
        uint32_t birthMask;
    };
    struct BuildConstants {
        glm::mat4 cullViewProj;
        uint32_t firstIndex;
        uint32_t count;
        uint32_t margin;
        uint32_t flags; // bit 0 animate, bit 1 write instances
        uint32_t maxInstances;
        float cullDistance;
        float cameraX, cameraY, cameraZ;
    };

    struct Box {
        glm::vec4 min; // w = edge thickness
        glm::vec4 max; // w = color id (see shaders/life3d_boxes.vert)
    };

    // What the crosshair points at within reach.
    struct Target {
        bool hit = false;       // a block is targeted
        glm::ivec3 block{0};    // the targeted block
        bool canPlace = false;
        glm::ivec3 place{0};    // where a stamp's anchor goes
        glm::ivec3 normal{0, 1, 0}; // direction stamps grow into
    };

    // A buffer with its memory and, when host-visible, its mapping.
    struct GpuBuffer {
        VkBuffer buffer = VK_NULL_HANDLE;
        VkDeviceMemory memory = VK_NULL_HANDLE;
        void* mapped = nullptr;
        VkDeviceSize size = 0;
        template <typename T> T* as() const { return static_cast<T*>(mapped); }
    };

    // Everything sized by the chunk pool, regrown together when the world outgrows it.
    struct ChunkPool {
        uint32_t capacity = 0;
        std::array<GpuBuffer, 2> cells; // ping-pong generations
        GpuBuffer neighbors;            // 27 slots per chunk
        GpuBuffer active;               // slots of the active chunks
        GpuBuffer origins;              // world cell of each chunk's local (0, 0, 0)
        GpuBuffer blockSlots;           // block-pool slot per chunk, or NO_CHUNK
        GpuBuffer stats;                // uvec2 per chunk: population, reach mask (GPU atomics)
        GpuBuffer statsReadback;        // the same, copied back for the CPU after each build
    };

    static constexpr int MAX_FRAMES_IN_FLIGHT = 2;
    // Simulation speed is 2^exponent generations per second; the top step is "as
    // fast as the GPU goes".
    static constexpr int MIN_SPEED_EXPONENT = -5;  // one generation every 32 s
    static constexpr int MAX_SPEED_EXPONENT = 13;  // 8192 generations per second
    static constexpr int UNLIMITED_SPEED_EXPONENT = MAX_SPEED_EXPONENT + 1;
    // Minecraft scale: 1 cell = 1 block, creative flight at 10.92 blocks/s, sprint doubles it.
    static constexpr float FLY_SPEED = 10.92f;
    static constexpr float REACH = 6.0f;
    static constexpr float AIR_PLACE_DISTANCE = 4.0f;
    // Walking (Minecraft values in blocks and seconds).
    static constexpr float WALK_SPEED = 4.317f;
    static constexpr float SPRINT_SPEED = 5.612f;
    static constexpr float GRAVITY = 32.0f;
    static constexpr float JUMP_SPEED = 8.9f; // clears a 1.25-block jump
    static constexpr float TERMINAL_SPEED = 78.4f;
    static constexpr float DOUBLE_TAP_SECONDS = 0.3f;
    static std::string saveFilePath() { return (userDataDirectory() / "world.life3d").string(); }
    static constexpr float MOUSE_DEGREES_PER_PIXEL = 0.15f; // at 100% sensitivity
    static constexpr float BREAK_REPEAT = 0.25f;
    static constexpr float PLACE_REPEAT = 0.20f;
    static constexpr float EDIT_ANIMATION_SECONDS = 0.16f;
    static constexpr float REBUILD_DISTANCE = 16.0f; // camera travel that refreshes the distance-culled block list
    static inline const glm::vec3 SUN_DIRECTION = glm::normalize(glm::vec3(0.45f, 0.80f, 0.30f));

    Options options;
    std::filesystem::path shaderDir;
    std::filesystem::path exeDir;
    std::mt19937 rng;

    GLFWwindow* window = nullptr;
    gol3d::GpuContext gpu;
    VkDevice device = VK_NULL_HANDLE;
    VkDeviceSize gpuBytes = 0; // memory allocated through createBuffer

    // Swapchain and targets (dynamic rendering: no render pass or framebuffers)
    VkSwapchainKHR swapchain = VK_NULL_HANDLE;
    std::vector<VkImage> swapchainImages;
    std::vector<VkImageView> swapchainImageViews;
    VkFormat swapchainImageFormat = VK_FORMAT_UNDEFINED;
    VkExtent2D swapchainExtent{};
    bool captureSupported = false;
    VkImage depthImage = VK_NULL_HANDLE;
    VkDeviceMemory depthImageMemory = VK_NULL_HANDLE;
    VkImageView depthImageView = VK_NULL_HANDLE;
    VkFormat depthFormat = VK_FORMAT_UNDEFINED;
    bool framebufferResized = false;

    // Rendering: all pipelines share one layout and one descriptor set per frame.
    VkDescriptorSetLayout frameSetLayout = VK_NULL_HANDLE;
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    VkPipeline blockPipeline = VK_NULL_HANDLE;
    VkPipeline boxPipeline = VK_NULL_HANDLE;
    VkPipeline skyPipeline = VK_NULL_HANDLE;
    VkPipeline gridPipeline = VK_NULL_HANDLE;
    VkPipeline hudPipeline = VK_NULL_HANDLE;
    VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
    std::array<VkDescriptorSet, MAX_FRAMES_IN_FLIGHT> frameSets{};
    std::vector<Vertex> cubeVertices;
    GpuBuffer vertexBuffer, indexBuffer;
    std::array<GpuBuffer, MAX_FRAMES_IN_FLIGHT> uniformBuffers;
    std::array<GpuBuffer, MAX_FRAMES_IN_FLIGHT> boxBuffers;

    // Frame synchronization. Render-finished semaphores are per swapchain image
    // because presentation may still hold them after the frame fence signals.
    std::array<VkCommandBuffer, MAX_FRAMES_IN_FLIGHT> commandBuffers{};
    std::array<VkSemaphore, MAX_FRAMES_IN_FLIGHT> imageAvailableSemaphores{};
    std::array<VkFence, MAX_FRAMES_IN_FLIGHT> inFlightFences{};
    std::vector<VkSemaphore> renderFinishedSemaphores;
    size_t currentFrame = 0;

    // Chunk pool. pool.cells ping-pong between generations; the tables are
    // indexed by slot and kept current by the CPU between passes.
    ChunkPool pool;
    uint32_t chunkLimit = DEFAULT_CHUNK_LIMIT;
    uint32_t currentCells = 0;
    GpuBuffer blockPool;        // BLOCK_ROWS words per block slot
    uint32_t blockCapacity = 0;
    uint32_t maxBlockSlots = 0; // largest block pool a storage buffer can hold
    std::vector<uint32_t> freeBlockSlots;
    GpuBuffer instanceBuffer, indirectBuffer, readbackBuffer;

    ChunkMap chunkMap;
    std::vector<glm::ivec3> slotChunk;   // chunk coordinate of each slot
    std::vector<uint32_t> neighborSlots; // CPU copy of pool.neighbors (reading GPU memory back is slow)
    std::vector<uint32_t> activeIndex;   // position of each slot in activeSlots
    std::vector<uint32_t> blockSlotOf;   // block-pool slot of each chunk slot
    std::vector<uint32_t> blockCount;    // static blocks in each chunk
    std::vector<uint32_t> wantedStamp;   // last maintenance that saw life able to reach the chunk
    uint32_t maintenanceStamp = 0;
    std::vector<uint32_t> freeSlots;
    std::vector<uint32_t> quarantine;    // freed slots that the last block list may still draw
    std::vector<uint32_t> activeSlots;
    bool chunkLimitHit = false;
    bool pausedAtLimit = false; // pause once per world when the chunk limit is reached
    bool refreshPending = false; // edits or camera moves need a new block list
    bool editOpen = false;       // the previous-generation buffer holds the world before this frame's edits
    bool blockListStale = false; // the last batch skipped the block list
    bool capturing = false;      // a scripted run that exits after a few frames: no animations
    double benchGpuMs = 0.0, benchCpuMs = 0.0; // time in GPU passes and in chunk bookkeeping (--bench)

    VkDescriptorSetLayout computeSetLayout = VK_NULL_HANDLE;
    VkPipelineLayout computePipelineLayout = VK_NULL_HANDLE;
    VkPipeline stepPipeline = VK_NULL_HANDLE;
    VkPipeline buildPipeline = VK_NULL_HANDLE;
    VkDescriptorPool computeDescriptorPool = VK_NULL_HANDLE;
    std::array<VkDescriptorSet, 2> computeSets{}; // [which cell buffer is current]
    VkCommandBuffer computeCommandBuffer = VK_NULL_HANDLE;
    VkFence computeFence = VK_NULL_HANDLE;

    // Simulation
    size_t ruleIndex = 0;
    bool simulationRunning = false;
    int speedExponent = 0;      // 2^speedExponent generations per second
    double stepDebt = 0.0;      // generations owed to the target speed
    uint64_t fastForward = 0;   // generations queued by J / Shift+J
    uint64_t fastForwardTotal = 0;
    uint64_t generation = 0;
    uint64_t population = 0;
    uint64_t drawnBlocks = 0;
    uint64_t visibleBlocks = 0; // before the draw cap
    // Governor: what a generation costs (ms, smoothed) and what rate it achieves.
    // GPU milliseconds (smoothed) per generation step, per block-list build and
    // per stats-only build, measured with timestamps; plus the CPU side of a
    // submission (chunk bookkeeping and waiting), from the wall clock.
    double stepCostMs = 0.0, drawCostMs = 0.0, statsCostMs = 0.0, overheadMs = 0.0;
    VkQueryPool timestamps = VK_NULL_HANDLE;
    double timestampMs = 0.0; // milliseconds per timestamp tick; 0 = no timestamps on this queue
    uint64_t timestampMask = ~0ull;
    double simCooldownUntil = 0.0;
    bool tickLimited = false;   // the target speed is more than the GPU can keep up with
    double limitedSince = -1.0;
    std::vector<std::pair<double, uint64_t>> rateSamples; // (time, generation)
    double measuredRate = 0.0;
    float lastSimMs = 0.0f;
    float worstFrameMs = 0.0f;
    // Birth/death animation of the last change
    double lastChangeTime = -100.0;
    float changeAnimationSeconds = 0.0f; // 0 = the last change is not animated
    bool lastBuildAnimated = false;
    glm::vec3 lastBuildEye{0.0f};
    glm::vec3 lastBuildForward{1.0f, 0.0f, 0.0f};
    glm::vec3 lastSortEye{1e9f};
    std::vector<float> populationHistory; // one entry per generation step shown in the HUD graph

    // Player (creative flight)
    glm::vec3 eye{0.0f};
    float yaw = 0.0f;   // degrees, 0 looks along +x
    float pitch = 0.0f; // degrees, positive looks up
    Target target;

    // Input
    bool cursorCaptured = false;
    bool haveCursorPosition = false;
    double lastCursorX = 0.0, lastCursorY = 0.0;
    int hotbarSlot = 0; // -1 = empty hand: nothing to place, no placement outline
    CellKind material = CellKind::Life; // what stamps are made of (M)
    int brushRotation = 0; // quarter turns around the placement surface (Q/E)
    int brushTilt = 0;     // quarter turns around the world x axis (Z/C), applied after brushRotation
    bool breakHeld = false, placeHeld = false;
    float breakTimer = 0.0f, placeTimer = 0.0f;
    bool hudVisible = true;
    bool showChunkBorders = false;
    bool fullscreen = false;

    // Menus (Dear ImGui). Pause, Settings and New World freeze the world like
    // Minecraft's single-player pause; the inventory does not.
    enum class Screen { Playing, Paused, Settings, Inventory, NewWorld };
    Screen screen = Screen::Playing;
    Settings settings;
    bool persistSettings = false;
    bool runningBeforePause = false;
    bool imguiReady = false;
    float uiScale = 0.0f;
    ImFont* titleFont = nullptr; // larger Karla for card titles
    ImGuiStyle baseStyle;
    double slotNameUntil = 0.0;
    std::string toast;
    double toastUntil = 0.0;
    double menuOpenedAt = 0.0; // menus fade in
    Screen shownScreen = Screen::Playing;
    bool showDebug = false;
    bool f3UsedInCombo = false;
    std::unique_ptr<gol3d::Updater> updater;
    bool updateAnnounced = false;
    int newWorldRule = 0;
    int newWorldSeed = 0;
    bool newWorldEmpty = false;
    bool flying = false;          // double-tap Space toggles, as in creative mode
    bool onGround = false;
    float verticalSpeed = 0.0f;
    double lastSpacePress = -1.0;
    int windowedX = 100, windowedY = 100, windowedWidth = 1280, windowedHeight = 800;
    tutorial::Tutorial tutorialPanel; // lesson panel over the running world (src/tutorial)

    // Screenshots
    std::string pendingScreenshot;
    GpuBuffer captureBuffer;

    // HUD / timing
    std::chrono::steady_clock::time_point startTime = std::chrono::steady_clock::now();
    float hudTimer = 0.0f;
    uint32_t hudFrames = 0;
    float fps = 0.0f;
    uint64_t framesRendered = 0;

    const LifeRule& rule() const { return lifeRules()[ruleIndex]; }

    void waitFence(VkFence fence, const char* what) {
        VkResult result = vkWaitForFences(device, 1, &fence, VK_TRUE, GPU_TIMEOUT_NS);
        if (result == VK_TIMEOUT) throw std::runtime_error(std::string("GPU did not finish ") + what + " within 4 s");
        if (result != VK_SUCCESS) throw std::runtime_error(std::string("Lost the GPU while waiting for ") + what);
    }

    // ------------------------------------------------------------------ setup

    void initWindow() {
        if (!glfwInit()) throw std::runtime_error("Could not open a window (GLFW failed to start).");
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        window = glfwCreateWindow(windowedWidth, windowedHeight, "3D Game of Life", nullptr, nullptr);
        if (!window) throw std::runtime_error("Could not open a window.");
        glfwSetWindowUserPointer(window, this);
        glfwSetFramebufferSizeCallback(window, [](GLFWwindow* w, int, int) {
            static_cast<LifePrototypeApp*>(glfwGetWindowUserPointer(w))->framebufferResized = true;
        });
        glfwSetKeyCallback(window, [](GLFWwindow* w, int key, int, int action, int mods) {
            static_cast<LifePrototypeApp*>(glfwGetWindowUserPointer(w))->onKey(key, action, mods);
        });
        glfwSetMouseButtonCallback(window, [](GLFWwindow* w, int button, int action, int mods) {
            static_cast<LifePrototypeApp*>(glfwGetWindowUserPointer(w))->onMouseButton(button, action, mods);
        });
        glfwSetCursorPosCallback(window, [](GLFWwindow* w, double x, double y) {
            static_cast<LifePrototypeApp*>(glfwGetWindowUserPointer(w))->onCursorMove(x, y);
        });
        glfwSetScrollCallback(window, [](GLFWwindow* w, double, double yoffset) {
            static_cast<LifePrototypeApp*>(glfwGetWindowUserPointer(w))->onScroll(yoffset);
        });
        glfwSetWindowFocusCallback(window, [](GLFWwindow* w, int focused) {
            auto* app = static_cast<LifePrototypeApp*>(glfwGetWindowUserPointer(w));
            // Like Minecraft, losing focus mid-game opens the pause menu.
            if (!focused && app->screen == Screen::Playing && app->cursorCaptured) app->openPauseMenu();
        });
        if (glfwRawMouseMotionSupported()) glfwSetInputMode(window, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
    }

    void initVulkan() {
        gpu.init(window);
        device = gpu.device;
        std::cout << "GPU: " << gpu.properties.deviceName << " (Vulkan " << VK_API_VERSION_MAJOR(gpu.properties.apiVersion)
                  << "." << VK_API_VERSION_MINOR(gpu.properties.apiVersion) << ")" << std::endl;
    }

    void initRendering() {
        createSwapchain();
        createImageViews();
        depthFormat = findDepthFormat();
        createDepthResources();
        createPerImageSemaphores();

        createWorldBuffers();
        createVertexBuffer();
        createFrameBuffers();
        createFrameSetLayout();
        createGraphicsPipelines();
        createFrameSets();
        createComputePipelines();
        createComputeSets();
        createCommandBuffers();
        createSyncObjects();
        resetChunks();
    }

    std::optional<uint32_t> findMemoryType(uint32_t typeBits, VkMemoryPropertyFlags wanted, uint32_t skip = 0) {
        const VkPhysicalDeviceMemoryProperties& properties = gpu.memoryProperties;
        for (uint32_t i = 0; i < properties.memoryTypeCount; ++i) {
            if ((skip & (1u << i)) == 0 && (typeBits & (1u << i)) && (properties.memoryTypes[i].propertyFlags & wanted) == wanted) return i;
        }
        return std::nullopt;
    }

    // Allocates with the first memory property set in `preferences` that exists
    // and has room (a full resizable-BAR heap falls back to the next choice).
    // Returns false when no choice has room.
    bool tryCreateBuffer(GpuBuffer& out, VkDeviceSize size, VkBufferUsageFlags usage,
                         std::initializer_list<VkMemoryPropertyFlags> preferences) {
        VkBufferCreateInfo bufferInfo{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
        bufferInfo.size = size;
        bufferInfo.usage = usage;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        if (vkCreateBuffer(device, &bufferInfo, nullptr, &out.buffer) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create buffer!");
        }
        VkMemoryRequirements requirements;
        vkGetBufferMemoryRequirements(device, out.buffer, &requirements);
        for (VkMemoryPropertyFlags wanted : preferences) {
            uint32_t tried = 0;
            while (std::optional<uint32_t> type = findMemoryType(requirements.memoryTypeBits, wanted, tried)) {
                tried |= 1u << *type;
                VkMemoryAllocateInfo allocInfo{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
                allocInfo.allocationSize = requirements.size;
                allocInfo.memoryTypeIndex = *type;
                if (vkAllocateMemory(device, &allocInfo, nullptr, &out.memory) != VK_SUCCESS) continue;
                vkBindBufferMemory(device, out.buffer, out.memory, 0);
                out.mapped = nullptr;
                if ((wanted & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) &&
                    vkMapMemory(device, out.memory, 0, VK_WHOLE_SIZE, 0, &out.mapped) != VK_SUCCESS) {
                    throw std::runtime_error("Failed to map buffer memory!");
                }
                out.size = size;
                gpuBytes += requirements.size;
                return true;
            }
        }
        vkDestroyBuffer(device, out.buffer, nullptr);
        out = GpuBuffer{};
        return false;
    }

    void createBuffer(GpuBuffer& out, VkDeviceSize size, VkBufferUsageFlags usage,
                      std::initializer_list<VkMemoryPropertyFlags> preferences) {
        if (!tryCreateBuffer(out, size, usage, preferences)) {
            throw std::runtime_error("Out of GPU memory (" + std::to_string(size >> 20) + " MB buffer)");
        }
    }

    void destroyBuffer(GpuBuffer& b) {
        if (b.buffer == VK_NULL_HANDLE) return;
        VkMemoryRequirements requirements;
        vkGetBufferMemoryRequirements(device, b.buffer, &requirements);
        gpuBytes -= std::min<VkDeviceSize>(gpuBytes, requirements.size);
        vkDestroyBuffer(device, b.buffer, nullptr);
        vkFreeMemory(device, b.memory, nullptr);
        b = GpuBuffer{};
    }

    static constexpr VkMemoryPropertyFlags HOST = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    static constexpr VkMemoryPropertyFlags HOST_CACHED = HOST | VK_MEMORY_PROPERTY_HOST_CACHED_BIT;
    static constexpr VkMemoryPropertyFlags DEVICE_HOST = HOST | VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    static constexpr VkMemoryPropertyFlags DEVICE = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    static constexpr VkBufferUsageFlags STORAGE = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    static constexpr VkBufferUsageFlags STORAGE_COPY =
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;

    void createSwapchain() {
        gol3d::SurfaceSupport support = gpu.querySurface();
        VkSurfaceFormatKHR surfaceFormat = support.formats[0];
        for (const auto& format : support.formats) {
            if (format.format == VK_FORMAT_B8G8R8A8_SRGB && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
                surfaceFormat = format;
                break;
            }
        }

        VkExtent2D extent = support.capabilities.currentExtent;
        if (extent.width == UINT32_MAX) {
            int width, height;
            glfwGetFramebufferSize(window, &width, &height);
            extent.width = std::clamp(static_cast<uint32_t>(width), support.capabilities.minImageExtent.width,
                                      support.capabilities.maxImageExtent.width);
            extent.height = std::clamp(static_cast<uint32_t>(height), support.capabilities.minImageExtent.height,
                                       support.capabilities.maxImageExtent.height);
        }

        uint32_t imageCount = support.capabilities.minImageCount + 1;
        if (support.capabilities.maxImageCount > 0) imageCount = std::min(imageCount, support.capabilities.maxImageCount);

        captureSupported = support.capabilities.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

        VkSwapchainCreateInfoKHR createInfo{VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
        createInfo.surface = gpu.surface;
        createInfo.minImageCount = imageCount;
        createInfo.imageFormat = surfaceFormat.format;
        createInfo.imageColorSpace = surfaceFormat.colorSpace;
        createInfo.imageExtent = extent;
        createInfo.imageArrayLayers = 1;
        createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | (captureSupported ? VK_IMAGE_USAGE_TRANSFER_SRC_BIT : 0);
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE; // one queue draws and presents
        createInfo.preTransform = support.capabilities.currentTransform;
        createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        createInfo.presentMode = VK_PRESENT_MODE_FIFO_KHR; // vsync caps the frame rate; always supported
        createInfo.clipped = VK_TRUE;

        if (vkCreateSwapchainKHR(device, &createInfo, nullptr, &swapchain) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create swap chain!");
        }
        uint32_t actualCount = 0;
        vkGetSwapchainImagesKHR(device, swapchain, &actualCount, nullptr);
        swapchainImages.resize(actualCount);
        vkGetSwapchainImagesKHR(device, swapchain, &actualCount, swapchainImages.data());
        swapchainImageFormat = surfaceFormat.format;
        swapchainExtent = extent;
    }

    void createImageViews() {
        swapchainImageViews.resize(swapchainImages.size());
        for (size_t i = 0; i < swapchainImages.size(); i++) {
            VkImageViewCreateInfo createInfo{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
            createInfo.image = swapchainImages[i];
            createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
            createInfo.format = swapchainImageFormat;
            createInfo.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
            if (vkCreateImageView(device, &createInfo, nullptr, &swapchainImageViews[i]) != VK_SUCCESS) {
                throw std::runtime_error("Failed to create image views!");
            }
        }
    }

    VkFormat findDepthFormat() {
        for (VkFormat format : {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D24_UNORM_S8_UINT, VK_FORMAT_D32_SFLOAT_S8_UINT}) {
            VkFormatProperties props;
            vkGetPhysicalDeviceFormatProperties(gpu.physicalDevice, format, &props);
            if (props.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) return format;
        }
        throw std::runtime_error("Failed to find supported depth format!");
    }

    void createDepthResources() {
        VkImageCreateInfo imageInfo{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.extent = {swapchainExtent.width, swapchainExtent.height, 1};
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 1;
        imageInfo.format = depthFormat;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        if (vkCreateImage(device, &imageInfo, nullptr, &depthImage) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create depth image!");
        }
        VkMemoryRequirements requirements;
        vkGetImageMemoryRequirements(device, depthImage, &requirements);
        std::optional<uint32_t> type = findMemoryType(requirements.memoryTypeBits, DEVICE);
        if (!type) throw std::runtime_error("No device-local memory for the depth buffer!");
        VkMemoryAllocateInfo allocInfo{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
        allocInfo.allocationSize = requirements.size;
        allocInfo.memoryTypeIndex = *type;
        if (vkAllocateMemory(device, &allocInfo, nullptr, &depthImageMemory) != VK_SUCCESS) {
            throw std::runtime_error("Failed to allocate depth image memory!");
        }
        vkBindImageMemory(device, depthImage, depthImageMemory, 0);

        VkImageViewCreateInfo viewInfo{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        viewInfo.image = depthImage;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = depthFormat;
        viewInfo.subresourceRange = {VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1};
        if (vkCreateImageView(device, &viewInfo, nullptr, &depthImageView) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create depth image view!");
        }
    }

    void createPerImageSemaphores() {
        VkSemaphoreCreateInfo semaphoreInfo{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
        renderFinishedSemaphores.resize(swapchainImages.size());
        for (auto& semaphore : renderFinishedSemaphores) {
            if (vkCreateSemaphore(device, &semaphoreInfo, nullptr, &semaphore) != VK_SUCCESS) {
                throw std::runtime_error("Failed to create semaphore!");
            }
        }
    }

    void destroySwapchainResources() {
        for (auto semaphore : renderFinishedSemaphores) vkDestroySemaphore(device, semaphore, nullptr);
        renderFinishedSemaphores.clear();
        vkDestroyImageView(device, depthImageView, nullptr);
        vkDestroyImage(device, depthImage, nullptr);
        vkFreeMemory(device, depthImageMemory, nullptr);
        depthImageView = VK_NULL_HANDLE;
        depthImage = VK_NULL_HANDLE;
        depthImageMemory = VK_NULL_HANDLE;
        for (auto view : swapchainImageViews) vkDestroyImageView(device, view, nullptr);
        swapchainImageViews.clear();
        vkDestroySwapchainKHR(device, swapchain, nullptr);
        swapchain = VK_NULL_HANDLE;
    }

    void recreateSwapchain() {
        int width = 0, height = 0;
        glfwGetFramebufferSize(window, &width, &height);
        while ((width == 0 || height == 0) && !glfwWindowShouldClose(window)) {
            glfwWaitEvents(); // minimized
            glfwGetFramebufferSize(window, &width, &height);
        }
        vkDeviceWaitIdle(device);
        destroySwapchainResources();
        createSwapchain();
        createImageViews();
        createDepthResources();
        createPerImageSemaphores();
        framebufferResized = false;
    }

    // Buffers whose size does not depend on the world, plus the first chunk and
    // block pools (both grow on demand).
    void createWorldBuffers() {
        // A storage buffer may not be bigger than the GPU allows (128 MB on some).
        const uint64_t range = gpu.properties.limits.maxStorageBufferRange;
        uint32_t fits = static_cast<uint32_t>(std::min<uint64_t>(range / (CHUNK_ROWS * sizeof(uint32_t)), MAX_CHUNK_SLOTS));
        if (chunkLimit > fits) {
            std::cout << "This GPU's buffers hold at most " << fits << " chunks; using that as the chunk limit." << std::endl;
            chunkLimit = fits;
        }
        maxBlockSlots = static_cast<uint32_t>(std::min<uint64_t>(range / (BLOCK_ROWS * sizeof(uint32_t)), MAX_CHUNK_SLOTS));
        createBuffer(instanceBuffer, VkDeviceSize(MAX_INSTANCES) * sizeof(glm::uvec2), STORAGE, {DEVICE, HOST});
        createBuffer(indirectBuffer, sizeof(VkDrawIndexedIndirectCommand),
                     STORAGE_COPY | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT, {DEVICE, HOST});
        createBuffer(readbackBuffer, sizeof(VkDrawIndexedIndirectCommand), VK_BUFFER_USAGE_TRANSFER_DST_BIT, {HOST_CACHED, HOST});
        if (!allocateChunkPool(pool, std::min(INITIAL_CHUNKS, chunkLimit))) {
            throw std::runtime_error("Out of GPU memory for the world");
        }
        resizeChunkTables(pool.capacity);
        createBuffer(blockPool, VkDeviceSize(INITIAL_BLOCK_SLOTS) * BLOCK_ROWS * sizeof(uint32_t), STORAGE_COPY, {DEVICE_HOST, HOST});
        blockCapacity = INITIAL_BLOCK_SLOTS;
    }

    // Cells, tables and stats for `capacity` chunks. Cell storage is device-local
    // and CPU-visible (resizable BAR) when available: the GPU does the heavy
    // reads, the CPU only touches a few rows to edit and to collide with blocks.
    bool allocateChunkPool(ChunkPool& p, uint32_t capacity) {
        p.capacity = capacity;
        bool ok = true;
        for (GpuBuffer& cells : p.cells) {
            ok = ok && tryCreateBuffer(cells, VkDeviceSize(capacity) * CHUNK_ROWS * sizeof(uint32_t), STORAGE_COPY, {DEVICE_HOST, HOST});
        }
        ok = ok && tryCreateBuffer(p.neighbors, VkDeviceSize(capacity) * 27 * sizeof(uint32_t), STORAGE_COPY, {DEVICE_HOST, HOST});
        ok = ok && tryCreateBuffer(p.active, VkDeviceSize(capacity) * sizeof(uint32_t), STORAGE_COPY, {DEVICE_HOST, HOST});
        ok = ok && tryCreateBuffer(p.origins, VkDeviceSize(capacity) * sizeof(glm::ivec4), STORAGE_COPY, {DEVICE_HOST, HOST});
        ok = ok && tryCreateBuffer(p.blockSlots, VkDeviceSize(capacity) * sizeof(uint32_t), STORAGE_COPY, {DEVICE_HOST, HOST});
        ok = ok && tryCreateBuffer(p.stats, VkDeviceSize(capacity) * sizeof(glm::uvec2), STORAGE_COPY, {DEVICE, HOST});
        ok = ok && tryCreateBuffer(p.statsReadback, VkDeviceSize(capacity) * sizeof(glm::uvec2),
                                   VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT, {HOST_CACHED, HOST});
        if (!ok) releaseChunkPool(p);
        return ok;
    }

    void releaseChunkPool(ChunkPool& p) {
        for (GpuBuffer& cells : p.cells) destroyBuffer(cells);
        for (GpuBuffer* b : {&p.neighbors, &p.active, &p.origins, &p.blockSlots, &p.stats, &p.statsReadback}) destroyBuffer(*b);
        p.capacity = 0;
    }

    void resizeChunkTables(uint32_t capacity) {
        slotChunk.resize(capacity, glm::ivec3(0));
        neighborSlots.resize(size_t(capacity) * 27, NO_CHUNK);
        activeIndex.resize(capacity, 0);
        blockSlotOf.resize(capacity, NO_CHUNK);
        blockCount.resize(capacity, 0);
        wantedStamp.resize(capacity, 0);
    }

    // Doubles the chunk pool (up to the limit), keeping every chunk in its slot.
    // Rare and brief: it waits for the GPU and copies the old pool.
    bool growChunkPool() {
        if (pool.capacity >= chunkLimit) return false;
        const uint32_t oldCapacity = pool.capacity;
        uint32_t capacity = std::min(oldCapacity * 2, chunkLimit);
        vkDeviceWaitIdle(device);
        ChunkPool bigger;
        if (!allocateChunkPool(bigger, capacity)) {
            notify("Out of GPU memory: the world can't grow past " + std::to_string(pool.capacity) + " chunks");
            chunkLimit = pool.capacity;
            return false;
        }
        // Copied on the GPU: reading device memory back through the CPU mapping is slow.
        VkCommandBuffer cmd = beginCompute();
        auto copy = [&](const GpuBuffer& from, GpuBuffer& to) {
            VkBufferCopy region{0, 0, from.size};
            vkCmdCopyBuffer(cmd, from.buffer, to.buffer, 1, &region);
        };
        for (int i = 0; i < 2; ++i) copy(pool.cells[i], bigger.cells[i]);
        copy(pool.neighbors, bigger.neighbors);
        copy(pool.active, bigger.active);
        copy(pool.origins, bigger.origins);
        copy(pool.blockSlots, bigger.blockSlots);
        copy(pool.statsReadback, bigger.statsReadback); // chunk maintenance may grow the pool while reading them
        memoryBarrier(cmd, VK_PIPELINE_STAGE_2_COPY_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_2_HOST_BIT,
                      VK_ACCESS_2_HOST_READ_BIT | VK_ACCESS_2_HOST_WRITE_BIT);
        submitCompute(cmd, "growing the world");
        releaseChunkPool(pool);
        pool = bigger;
        resizeChunkTables(capacity);
        for (uint32_t slot = capacity; slot-- > oldCapacity;) freeSlots.push_back(slot);
        writeComputeSets();
        writeFrameSets();
        return true;
    }

    bool growBlockPool() {
        uint32_t capacity = std::min(blockCapacity * 2, maxBlockSlots);
        if (capacity <= blockCapacity) return false;
        vkDeviceWaitIdle(device);
        GpuBuffer bigger;
        if (!tryCreateBuffer(bigger, VkDeviceSize(capacity) * BLOCK_ROWS * sizeof(uint32_t), STORAGE_COPY, {DEVICE_HOST, HOST})) {
            return false;
        }
        VkCommandBuffer cmd = beginCompute();
        VkBufferCopy region{0, 0, blockPool.size};
        vkCmdCopyBuffer(cmd, blockPool.buffer, bigger.buffer, 1, &region);
        memoryBarrier(cmd, VK_PIPELINE_STAGE_2_COPY_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_2_HOST_BIT,
                      VK_ACCESS_2_HOST_READ_BIT | VK_ACCESS_2_HOST_WRITE_BIT);
        submitCompute(cmd, "growing the block pool");
        destroyBuffer(blockPool);
        blockPool = bigger;
        for (uint32_t b = capacity; b-- > blockCapacity;) freeBlockSlots.push_back(b);
        blockCapacity = capacity;
        writeComputeSets();
        return true;
    }

    uint32_t* cellRows(uint32_t which) const { return pool.cells[which].as<uint32_t>(); }
    // Neighbor table entries are written to the CPU copy and through to the GPU.
    void setNeighbor(uint32_t slot, int k, uint32_t value) {
        size_t i = size_t(slot) * 27 + k;
        neighborSlots[i] = value;
        pool.neighbors.as<uint32_t>()[i] = value;
    }
    uint32_t* blockRows() const { return blockPool.as<uint32_t>(); }

    void createVertexBuffer() {
        // One unit cube as 12 triangles with per-face normals, for box outlines.
        const std::array<glm::vec3, 6> normals = {
            glm::vec3(1, 0, 0), glm::vec3(-1, 0, 0), glm::vec3(0, 1, 0),
            glm::vec3(0, -1, 0), glm::vec3(0, 0, 1), glm::vec3(0, 0, -1)};
        for (const glm::vec3& n : normals) {
            glm::vec3 u = std::abs(n.y) > 0.5f ? glm::vec3(1, 0, 0) : glm::vec3(0, 1, 0);
            glm::vec3 w = glm::cross(n, u);
            glm::vec3 c = n * 0.5f;
            std::array<glm::vec3, 4> corners = {c - 0.5f * u - 0.5f * w, c + 0.5f * u - 0.5f * w,
                                                c + 0.5f * u + 0.5f * w, c - 0.5f * u + 0.5f * w};
            for (int i : {0, 1, 2, 0, 2, 3}) cubeVertices.push_back({corners[i], n});
        }
        VkDeviceSize size = sizeof(Vertex) * cubeVertices.size();
        createBuffer(vertexBuffer, size, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, {DEVICE_HOST, HOST});
        std::memcpy(vertexBuffer.mapped, cubeVertices.data(), size);
        // Blocks: two triangles per camera-facing quad, corners 0-1-2 and 0-2-3.
        std::array<uint16_t, BLOCK_INDICES> indices{};
        for (uint16_t face = 0; face < 3; ++face)
            for (int i = 0; i < 6; ++i) indices[face * 6 + i] = static_cast<uint16_t>(face * 4 + std::array{0, 1, 2, 0, 2, 3}[i]);
        createBuffer(indexBuffer, sizeof(indices), VK_BUFFER_USAGE_INDEX_BUFFER_BIT, {DEVICE_HOST, HOST});
        std::memcpy(indexBuffer.mapped, indices.data(), sizeof(indices));
    }

    void createFrameBuffers() {
        for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            createBuffer(uniformBuffers[i], sizeof(FrameUniforms), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, {DEVICE_HOST, HOST});
            createBuffer(boxBuffers[i], VkDeviceSize(MAX_BOXES) * sizeof(Box), STORAGE, {DEVICE_HOST, HOST});
        }
    }

    void createFrameSetLayout() {
        std::array<VkDescriptorSetLayoutBinding, 4> bindings{};
        bindings[0] = {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, nullptr};
        bindings[1] = {1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, nullptr};
        bindings[2] = {2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, nullptr};
        bindings[3] = {3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, nullptr};
        VkDescriptorSetLayoutCreateInfo layoutInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
        layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
        layoutInfo.pBindings = bindings.data();
        if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &frameSetLayout) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create descriptor set layout!");
        }

        VkPushConstantRange pushRange{VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(uint32_t)};
        VkPipelineLayoutCreateInfo pipelineLayoutInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
        pipelineLayoutInfo.setLayoutCount = 1;
        pipelineLayoutInfo.pSetLayouts = &frameSetLayout;
        pipelineLayoutInfo.pushConstantRangeCount = 1;
        pipelineLayoutInfo.pPushConstantRanges = &pushRange;
        if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create pipeline layout!");
        }
    }

    VkShaderModule loadShader(const char* name) {
        std::filesystem::path path = shaderDir / name;
        std::ifstream file(path, std::ios::binary | std::ios::ate);
        if (!file) throw std::runtime_error("Missing shader " + path.string());
        std::vector<char> code(static_cast<size_t>(file.tellg()));
        file.seekg(0);
        file.read(code.data(), static_cast<std::streamsize>(code.size()));
        VkShaderModuleCreateInfo createInfo{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
        createInfo.codeSize = code.size();
        createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());
        VkShaderModule module = VK_NULL_HANDLE;
        if (vkCreateShaderModule(device, &createInfo, nullptr, &module) != VK_SUCCESS) {
            throw std::runtime_error("Failed to load shader " + path.string());
        }
        return module;
    }

    struct PipelineSpec {
        const char* vertexShader;
        const char* fragmentShader;
        bool cubeVertices;
        bool depthTest;
        bool depthWrite;
        bool alphaBlend;
    };

    VkPipeline createGraphicsPipeline(const PipelineSpec& spec) {
        VkShaderModule vertexModule = loadShader(spec.vertexShader);
        VkShaderModule fragmentModule = loadShader(spec.fragmentShader);
        std::array<VkPipelineShaderStageCreateInfo, 2> stages{};
        stages[0] = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_VERTEX_BIT, vertexModule, "main", nullptr};
        stages[1] = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_FRAGMENT_BIT, fragmentModule, "main", nullptr};

        VkVertexInputBindingDescription binding{0, sizeof(Vertex), VK_VERTEX_INPUT_RATE_VERTEX};
        std::array<VkVertexInputAttributeDescription, 2> attributes{{
            {0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, pos)},
            {1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, normal)},
        }};
        VkPipelineVertexInputStateCreateInfo vertexInput{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
        if (spec.cubeVertices) {
            vertexInput.vertexBindingDescriptionCount = 1;
            vertexInput.pVertexBindingDescriptions = &binding;
            vertexInput.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributes.size());
            vertexInput.pVertexAttributeDescriptions = attributes.data();
        }

        VkPipelineInputAssemblyStateCreateInfo inputAssembly{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
        inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

        VkPipelineViewportStateCreateInfo viewportState{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
        viewportState.viewportCount = 1;
        viewportState.scissorCount = 1;

        VkPipelineRasterizationStateCreateInfo rasterizer{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
        rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
        rasterizer.lineWidth = 1.0f;
        rasterizer.cullMode = VK_CULL_MODE_NONE; // blocks only emit camera-facing faces
        rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;

        VkPipelineMultisampleStateCreateInfo multisampling{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
        multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

        VkPipelineDepthStencilStateCreateInfo depthStencil{VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
        depthStencil.depthTestEnable = spec.depthTest;
        depthStencil.depthWriteEnable = spec.depthWrite;
        depthStencil.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;

        VkPipelineColorBlendAttachmentState blendAttachment{};
        blendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                         VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        if (spec.alphaBlend) {
            blendAttachment.blendEnable = VK_TRUE;
            blendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
            blendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
            blendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
            blendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
            blendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
            blendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
        }
        VkPipelineColorBlendStateCreateInfo colorBlending{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
        colorBlending.attachmentCount = 1;
        colorBlending.pAttachments = &blendAttachment;

        std::array<VkDynamicState, 2> dynamicStates = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
        VkPipelineDynamicStateCreateInfo dynamicState{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
        dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
        dynamicState.pDynamicStates = dynamicStates.data();

        VkPipelineRenderingCreateInfo rendering = renderingInfo();
        VkGraphicsPipelineCreateInfo pipelineInfo{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO, &rendering};
        pipelineInfo.stageCount = static_cast<uint32_t>(stages.size());
        pipelineInfo.pStages = stages.data();
        pipelineInfo.pVertexInputState = &vertexInput;
        pipelineInfo.pInputAssemblyState = &inputAssembly;
        pipelineInfo.pViewportState = &viewportState;
        pipelineInfo.pRasterizationState = &rasterizer;
        pipelineInfo.pMultisampleState = &multisampling;
        pipelineInfo.pDepthStencilState = &depthStencil;
        pipelineInfo.pColorBlendState = &colorBlending;
        pipelineInfo.pDynamicState = &dynamicState;
        pipelineInfo.layout = pipelineLayout;
        VkPipeline pipeline = VK_NULL_HANDLE;
        VkResult result = vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline);
        vkDestroyShaderModule(device, vertexModule, nullptr);
        vkDestroyShaderModule(device, fragmentModule, nullptr);
        if (result != VK_SUCCESS) throw std::runtime_error("Failed to create graphics pipeline!");
        return pipeline;
    }

    // Attachment formats for dynamic rendering (pipelines and ImGui).
    VkPipelineRenderingCreateInfo renderingInfo() const {
        VkPipelineRenderingCreateInfo info{VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO};
        info.colorAttachmentCount = 1;
        info.pColorAttachmentFormats = &swapchainImageFormat;
        info.depthAttachmentFormat = depthFormat;
        return info;
    }

    void createGraphicsPipelines() {
        blockPipeline = createGraphicsPipeline({"life3d_blocks.vert.spv", "life3d_world.frag.spv", false, true, true, false});
        boxPipeline = createGraphicsPipeline({"life3d_boxes.vert.spv", "life3d_world.frag.spv", true, true, true, false});
        skyPipeline = createGraphicsPipeline({"life3d_screen.vert.spv", "life3d_screen.frag.spv", false, false, false, false});
        gridPipeline = createGraphicsPipeline({"life3d_screen.vert.spv", "life3d_screen.frag.spv", false, true, false, true});
        hudPipeline = createGraphicsPipeline({"life3d_screen.vert.spv", "life3d_screen.frag.spv", false, false, false, true});
    }

    void writeBufferDescriptor(VkDescriptorSet set, uint32_t binding, VkDescriptorType type, VkBuffer buffer) {
        VkDescriptorBufferInfo bufferInfo{buffer, 0, VK_WHOLE_SIZE};
        VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        write.dstSet = set;
        write.dstBinding = binding;
        write.descriptorType = type;
        write.descriptorCount = 1;
        write.pBufferInfo = &bufferInfo;
        vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);
    }

    void createFrameSets() {
        std::array<VkDescriptorPoolSize, 2> poolSizes{{
            {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, MAX_FRAMES_IN_FLIGHT},
            {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, MAX_FRAMES_IN_FLIGHT * 3},
        }};
        VkDescriptorPoolCreateInfo poolInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
        poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
        poolInfo.pPoolSizes = poolSizes.data();
        poolInfo.maxSets = MAX_FRAMES_IN_FLIGHT;
        if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create descriptor pool!");
        }
        for (int frame = 0; frame < MAX_FRAMES_IN_FLIGHT; ++frame) {
            VkDescriptorSetAllocateInfo allocInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
            allocInfo.descriptorPool = descriptorPool;
            allocInfo.descriptorSetCount = 1;
            allocInfo.pSetLayouts = &frameSetLayout;
            if (vkAllocateDescriptorSets(device, &allocInfo, &frameSets[frame]) != VK_SUCCESS) {
                throw std::runtime_error("Failed to allocate descriptor set!");
            }
        }
        writeFrameSets();
    }

    void writeFrameSets() {
        for (int frame = 0; frame < MAX_FRAMES_IN_FLIGHT; ++frame) {
            writeBufferDescriptor(frameSets[frame], 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, uniformBuffers[frame].buffer);
            writeBufferDescriptor(frameSets[frame], 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, instanceBuffer.buffer);
            writeBufferDescriptor(frameSets[frame], 2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, boxBuffers[frame].buffer);
            writeBufferDescriptor(frameSets[frame], 3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, pool.origins.buffer);
        }
    }

    // The step and build passes share one set layout (life3d_storage.glsl and the
    // bindings in each shader) and one push-constant range.
    void createComputePipelines() {
        std::array<VkDescriptorSetLayoutBinding, 10> bindings{};
        for (uint32_t i = 0; i < bindings.size(); ++i) {
            bindings[i] = {i, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr};
        }
        VkDescriptorSetLayoutCreateInfo layoutInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
        layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
        layoutInfo.pBindings = bindings.data();
        if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &computeSetLayout) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create compute descriptor set layout!");
        }

        VkPushConstantRange pushRange{VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(BuildConstants)};
        VkPipelineLayoutCreateInfo pipelineLayoutInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
        pipelineLayoutInfo.setLayoutCount = 1;
        pipelineLayoutInfo.pSetLayouts = &computeSetLayout;
        pipelineLayoutInfo.pushConstantRangeCount = 1;
        pipelineLayoutInfo.pPushConstantRanges = &pushRange;
        if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &computePipelineLayout) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create compute pipeline layout!");
        }
        stepPipeline = createComputePipeline("life3d_step.comp.spv");
        buildPipeline = createComputePipeline("life3d_build.comp.spv");
    }

    VkPipeline createComputePipeline(const char* shader) {
        VkShaderModule module = loadShader(shader);
        VkComputePipelineCreateInfo pipelineInfo{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
        pipelineInfo.layout = computePipelineLayout;
        pipelineInfo.stage = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_COMPUTE_BIT, module, "main", nullptr};
        VkPipeline pipeline = VK_NULL_HANDLE;
        VkResult result = vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline);
        vkDestroyShaderModule(device, module, nullptr);
        if (result != VK_SUCCESS) throw std::runtime_error(std::string("Failed to create compute pipeline ") + shader);
        return pipeline;
    }

    // computeSets[p] reads pool.cells[p] as the current generation; binding 1 is
    // the other buffer (the next generation for a step, the previous one for a build).
    void createComputeSets() {
        VkDescriptorPoolSize poolSize{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 20};
        VkDescriptorPoolCreateInfo poolInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
        poolInfo.poolSizeCount = 1;
        poolInfo.pPoolSizes = &poolSize;
        poolInfo.maxSets = 2;
        if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &computeDescriptorPool) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create compute descriptor pool!");
        }
        for (int parity = 0; parity < 2; ++parity) {
            VkDescriptorSetAllocateInfo allocInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
            allocInfo.descriptorPool = computeDescriptorPool;
            allocInfo.descriptorSetCount = 1;
            allocInfo.pSetLayouts = &computeSetLayout;
            if (vkAllocateDescriptorSets(device, &allocInfo, &computeSets[parity]) != VK_SUCCESS) {
                throw std::runtime_error("Failed to allocate compute descriptor set!");
            }
        }
        writeComputeSets();
    }

    void writeComputeSets() {
        for (int parity = 0; parity < 2; ++parity) {
            const std::array<VkBuffer, 10> buffers = {
                pool.cells[parity].buffer, pool.cells[1 - parity].buffer, pool.neighbors.buffer, pool.active.buffer,
                blockPool.buffer, pool.blockSlots.buffer, pool.stats.buffer, instanceBuffer.buffer,
                indirectBuffer.buffer, pool.origins.buffer};
            for (uint32_t binding = 0; binding < buffers.size(); ++binding) {
                writeBufferDescriptor(computeSets[parity], binding, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, buffers[binding]);
            }
        }
    }

    void createCommandBuffers() {
        VkCommandBufferAllocateInfo allocInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
        allocInfo.commandPool = gpu.commandPool;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = MAX_FRAMES_IN_FLIGHT;
        if (vkAllocateCommandBuffers(device, &allocInfo, commandBuffers.data()) != VK_SUCCESS) {
            throw std::runtime_error("Failed to allocate command buffers!");
        }
        allocInfo.commandBufferCount = 1;
        if (vkAllocateCommandBuffers(device, &allocInfo, &computeCommandBuffer) != VK_SUCCESS) {
            throw std::runtime_error("Failed to allocate compute command buffer!");
        }
    }

    void createSyncObjects() {
        VkSemaphoreCreateInfo semaphoreInfo{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
        VkFenceCreateInfo fenceInfo{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
        for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            if (vkCreateSemaphore(device, &semaphoreInfo, nullptr, &imageAvailableSemaphores[i]) != VK_SUCCESS ||
                vkCreateFence(device, &fenceInfo, nullptr, &inFlightFences[i]) != VK_SUCCESS) {
                throw std::runtime_error("Failed to create synchronization objects!");
            }
        }
        fenceInfo.flags = 0;
        if (vkCreateFence(device, &fenceInfo, nullptr, &computeFence) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create compute fence!");
        }
        // Timestamps around the step and build passes, for the governor.
        uint32_t familyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(gpu.physicalDevice, &familyCount, nullptr);
        std::vector<VkQueueFamilyProperties> families(familyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(gpu.physicalDevice, &familyCount, families.data());
        if (families[gpu.queueFamily].timestampValidBits > 0 && gpu.properties.limits.timestampPeriod > 0.0f) {
            VkQueryPoolCreateInfo queryInfo{VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO};
            queryInfo.queryType = VK_QUERY_TYPE_TIMESTAMP;
            queryInfo.queryCount = 3;
            if (vkCreateQueryPool(device, &queryInfo, nullptr, &timestamps) == VK_SUCCESS) {
                timestampMs = gpu.properties.limits.timestampPeriod / 1e6;
                uint32_t bits = families[gpu.queueFamily].timestampValidBits;
                timestampMask = bits >= 64 ? ~0ull : (1ull << bits) - 1;
            }
        }
    }

    // ----------------------------------------------------------------- chunks

    void resetChunks() {
        // Frames still drawing the old world read chunk origins this reset rewrites.
        vkDeviceWaitIdle(device);
        chunkMap.clear();
        freeSlots.clear();
        quarantine.clear();
        for (uint32_t slot = pool.capacity; slot-- > 0;) freeSlots.push_back(slot); // hand out low slots first
        activeSlots.clear();
        freeBlockSlots.clear();
        for (uint32_t b = blockCapacity; b-- > 0;) freeBlockSlots.push_back(b);
        std::fill(blockSlotOf.begin(), blockSlotOf.end(), NO_CHUNK);
        std::fill(blockCount.begin(), blockCount.end(), 0u);
        chunkLimitHit = false;
        pausedAtLimit = false;
        editOpen = false;
        populationHistory.clear();
    }

    uint32_t findChunk(const glm::ivec3& key) const { return chunkMap.find(key.x, key.y, key.z); }

    // The chunk's slot, allocating it (and linking it to its neighbors) if needed.
    // NO_CHUNK when the world has reached its chunk limit.
    uint32_t ensureChunk(const glm::ivec3& key) {
        uint32_t slot = findChunk(key);
        if (slot != NO_CHUNK) return slot;
        if (!ChunkMap::inRange(key.x, key.y, key.z) || (freeSlots.empty() && !growChunkPool())) {
            chunkLimitHit = true;
            return NO_CHUNK;
        }
        slot = freeSlots.back();
        freeSlots.pop_back();
        for (uint32_t which = 0; which < 2; ++which) {
            std::memset(cellRows(which) + size_t(slot) * CHUNK_ROWS, 0, CHUNK_ROWS * sizeof(uint32_t));
        }
        pool.origins.as<glm::ivec4>()[slot] = glm::ivec4(key * CHUNK, 0);
        pool.blockSlots.as<uint32_t>()[slot] = NO_CHUNK;
        slotChunk[slot] = key;
        blockSlotOf[slot] = NO_CHUNK;
        blockCount[slot] = 0;
        chunkMap.insert(key.x, key.y, key.z, slot);
        for (int k = 0; k < 27; ++k) {
            if (k == 13) {
                setNeighbor(slot, 13, slot);
                continue;
            }
            uint32_t neighbor = findChunk(key + neighborOffset(k));
            setNeighbor(slot, k, neighbor);
            if (neighbor != NO_CHUNK) setNeighbor(neighbor, 26 - k, slot); // the opposite offset
        }
        activeIndex[slot] = static_cast<uint32_t>(activeSlots.size());
        pool.active.as<uint32_t>()[activeSlots.size()] = slot;
        activeSlots.push_back(slot);
        return slot;
    }

    void freeChunk(uint32_t slot) {
        for (int k = 0; k < 27; ++k) {
            uint32_t neighbor = neighborSlots[size_t(slot) * 27 + k];
            if (k != 13 && neighbor != NO_CHUNK) setNeighbor(neighbor, 26 - k, NO_CHUNK);
        }
        const glm::ivec3& key = slotChunk[slot];
        chunkMap.erase(key.x, key.y, key.z);
        if (blockSlotOf[slot] != NO_CHUNK) releaseBlockSlot(slot);
        uint32_t index = activeIndex[slot];
        uint32_t last = activeSlots.back();
        activeSlots[index] = last;
        pool.active.as<uint32_t>()[index] = last;
        activeIndex[last] = index;
        activeSlots.pop_back();
        quarantine.push_back(slot);
    }

    uint32_t ensureBlockSlot(uint32_t slot) {
        if (blockSlotOf[slot] != NO_CHUNK) return blockSlotOf[slot];
        if (freeBlockSlots.empty() && !growBlockPool()) return NO_CHUNK;
        uint32_t b = freeBlockSlots.back();
        freeBlockSlots.pop_back();
        std::memset(blockRows() + size_t(b) * BLOCK_ROWS, 0, BLOCK_ROWS * sizeof(uint32_t));
        blockSlotOf[slot] = b;
        pool.blockSlots.as<uint32_t>()[slot] = b;
        return b;
    }

    void releaseBlockSlot(uint32_t slot) {
        freeBlockSlots.push_back(blockSlotOf[slot]);
        blockSlotOf[slot] = NO_CHUNK;
        pool.blockSlots.as<uint32_t>()[slot] = NO_CHUNK;
        blockCount[slot] = 0;
    }

    // After a build: keep chunks that hold life or blocks, add chunks that life
    // can reach within MAX_BATCH generations, and free the rest. Slots freed
    // here are reused only after the next build, because the block list drawn
    // until then may still show cells of the freed chunk shrinking away.
    void maintainChunks() {
        freeSlots.insert(freeSlots.end(), quarantine.begin(), quarantine.end());
        quarantine.clear();
        ++maintenanceStamp;
        population = 0;
        const std::vector<uint32_t> processed = activeSlots;
        for (uint32_t slot : processed) {
            glm::uvec2 stats = pool.statsReadback.as<glm::uvec2>()[slot];
            population += stats.x;
            for (uint32_t reach = stats.y; reach != 0; reach &= reach - 1) {
                int k = std::countr_zero(reach);
                uint32_t neighbor = neighborSlots[size_t(slot) * 27 + k];
                if (neighbor == NO_CHUNK) neighbor = ensureChunk(slotChunk[slot] + neighborOffset(k));
                if (neighbor != NO_CHUNK) wantedStamp[neighbor] = maintenanceStamp;
            }
        }
        for (uint32_t slot : processed) {
            if (pool.statsReadback.as<glm::uvec2>()[slot].x == 0 && blockCount[slot] == 0 && wantedStamp[slot] != maintenanceStamp) {
                freeChunk(slot);
            }
        }
    }

    // What occupies a cell: -1 when empty, else its CellKind.
    int cellKind(const glm::ivec3& cell) const {
        uint32_t slot = findChunk(chunkOf(cell));
        if (slot == NO_CHUNK) return -1;
        uint32_t row = rowOf(cell), bit = bitOf(cell);
        if (cellRows(currentCells)[size_t(slot) * CHUNK_ROWS + row] & bit) return static_cast<int>(CellKind::Life);
        uint32_t b = blockSlotOf[slot];
        if (b == NO_CHUNK || !(blockRows()[size_t(b) * BLOCK_ROWS + row] & bit)) return -1;
        bool emits = blockRows()[size_t(b) * BLOCK_ROWS + CHUNK_ROWS + row] & bit;
        return static_cast<int>(emits ? CellKind::Ember : CellKind::Stone);
    }

    bool occupied(const glm::ivec3& cell) const { return cellKind(cell) >= 0; }
    bool cellAlive(const glm::ivec3& cell) const { return cellKind(cell) == static_cast<int>(CellKind::Life); }

    // Before the first edit of a frame, saves the world into the previous-generation
    // buffer so the next block list shows placed cells growing in and removed
    // ones shrinking away.
    void beginEdit() {
        refreshPending = true;
        if (editOpen || !animations()) return;
        editOpen = true;
        if (activeSlots.empty()) return; // new chunks start empty in both buffers
        std::vector<VkBufferCopy> regions;
        regions.reserve(activeSlots.size());
        for (uint32_t slot : activeSlots) {
            VkDeviceSize offset = VkDeviceSize(slot) * CHUNK_ROWS * sizeof(uint32_t);
            regions.push_back({offset, offset, CHUNK_ROWS * sizeof(uint32_t)});
        }
        VkCommandBuffer cmd = beginCompute();
        vkCmdCopyBuffer(cmd, pool.cells[currentCells].buffer, pool.cells[1 - currentCells].buffer,
                        static_cast<uint32_t>(regions.size()), regions.data());
        memoryBarrier(cmd, VK_PIPELINE_STAGE_2_COPY_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_2_HOST_BIT,
                      VK_ACCESS_2_HOST_READ_BIT | VK_ACCESS_2_HOST_WRITE_BIT);
        submitCompute(cmd, "saving the world before an edit");
    }

    // CPU edits land in the current generation; the next build picks them up.
    void setLife(const glm::ivec3& cell, bool alive) {
        uint32_t slot = alive ? ensureChunk(chunkOf(cell)) : findChunk(chunkOf(cell));
        if (slot == NO_CHUNK) return;
        uint32_t row = rowOf(cell), bit = bitOf(cell);
        if (alive && blockSlotOf[slot] != NO_CHUNK && (blockRows()[size_t(blockSlotOf[slot]) * BLOCK_ROWS + row] & bit)) return;
        uint32_t& word = cellRows(currentCells)[size_t(slot) * CHUNK_ROWS + row];
        word = alive ? word | bit : word & ~bit;
    }

    // Puts a block of `kind` into an empty cell. Placing never replaces what is there.
    bool placeCell(const glm::ivec3& cell, CellKind kind) {
        if (occupied(cell)) return false;
        beginEdit();
        if (kind == CellKind::Life) {
            setLife(cell, true);
            return true;
        }
        uint32_t slot = ensureChunk(chunkOf(cell));
        if (slot == NO_CHUNK) return false;
        uint32_t b = ensureBlockSlot(slot);
        if (b == NO_CHUNK) return false;
        uint32_t row = rowOf(cell), bit = bitOf(cell);
        blockRows()[size_t(b) * BLOCK_ROWS + row] |= bit;
        if (cellType(kind).countsAsNeighbor) blockRows()[size_t(b) * BLOCK_ROWS + CHUNK_ROWS + row] |= bit;
        ++blockCount[slot];
        return true;
    }

    // Empties a cell, whatever is in it.
    void clearCell(const glm::ivec3& cell) {
        int kind = cellKind(cell);
        if (kind < 0) return;
        beginEdit();
        if (kind == static_cast<int>(CellKind::Life)) {
            setLife(cell, false);
            return;
        }
        uint32_t slot = findChunk(chunkOf(cell));
        uint32_t b = blockSlotOf[slot];
        uint32_t row = rowOf(cell), bit = bitOf(cell);
        blockRows()[size_t(b) * BLOCK_ROWS + row] &= ~bit;
        blockRows()[size_t(b) * BLOCK_ROWS + CHUNK_ROWS + row] &= ~bit;
        if (--blockCount[slot] == 0) releaseBlockSlot(slot);
    }

    void seedSoup(const glm::ivec3& minCorner, const glm::ivec3& size, float density) {
        std::uniform_real_distribution<float> chance(0.0f, 1.0f);
        for (int z = 0; z < size.z; ++z)
            for (int y = 0; y < size.y; ++y)
                for (int x = 0; x < size.x; ++x)
                    if (chance(rng) < density) setLife(minCorner + glm::ivec3(x, y, z), true);
        refreshPending = true;
    }

    void newWorld(bool seed) {
        tutorialPanel.close(); // its lesson no longer matches the world
        resetChunks();
        generation = 0;
        population = 0;
        stepDebt = 0.0;
        fastForward = fastForwardTotal = 0;
        int size = rule().seedSize;
        // The seed soup rests on the ground (y = 0), like a structure in a superflat world.
        if (seed) seedSoup(glm::ivec3(-size / 2, 0, -size / 2), glm::ivec3(size), rule().seedDensity);
        rebuild(Rebuild::Reset);
        // Spawn on the ground at a distance, facing the seed.
        float distance = std::max(20.0f, 1.6f * static_cast<float>(size));
        eye = glm::vec3(0.6f * distance, EYE_HEIGHT, 0.8f * distance);
        glm::vec3 dir = glm::normalize(glm::vec3(0.0f, 0.4f * static_cast<float>(size), 0.0f) - eye);
        yaw = glm::degrees(std::atan2(dir.z, dir.x));
        pitch = glm::degrees(std::asin(dir.y));
        flying = false;
        verticalSpeed = 0.0f;
    }

    // Visits every occupied cell: fn(cell, kind).
    template <typename Fn> void forEachCell(Fn&& fn) const {
        const uint32_t* current = cellRows(currentCells);
        for (uint32_t slot : activeSlots) {
            glm::ivec3 origin = slotChunk[slot] * CHUNK;
            uint32_t b = blockSlotOf[slot];
            for (uint32_t row = 0; row < CHUNK_ROWS; ++row) {
                glm::ivec3 base = origin + glm::ivec3(0, row % CHUNK, row / CHUNK);
                for (uint32_t bits = current[size_t(slot) * CHUNK_ROWS + row]; bits; bits &= bits - 1) {
                    fn(base + glm::ivec3(std::countr_zero(bits), 0, 0), CellKind::Life);
                }
                if (b == NO_CHUNK) continue;
                const uint32_t emits = blockRows()[size_t(b) * BLOCK_ROWS + CHUNK_ROWS + row];
                for (uint32_t bits = blockRows()[size_t(b) * BLOCK_ROWS + row]; bits; bits &= bits - 1) {
                    int x = std::countr_zero(bits);
                    fn(base + glm::ivec3(x, 0, 0), (emits >> x & 1u) ? CellKind::Ember : CellKind::Stone);
                }
            }
        }
    }

    // Save format "L3D2": rule index, generation, player pose, live cells as
    // int32 x,y,z triples, then blocks as x,y,z,kind (kind is a CellKind value).
    // Little-endian as written by this machine. "L3D1" saves (no blocks) still load.
    bool saveWorld(const std::string& path) {
        std::vector<glm::ivec3> cells;
        std::vector<glm::ivec4> blocks;
        forEachCell([&](const glm::ivec3& cell, CellKind kind) {
            if (kind == CellKind::Life) cells.push_back(cell);
            else blocks.emplace_back(cell, static_cast<int>(kind));
        });
        std::ofstream out(path, std::ios::binary);
        auto put = [&](const auto& value) { out.write(reinterpret_cast<const char*>(&value), sizeof(value)); };
        out.write("L3D2", 4);
        put(static_cast<uint32_t>(ruleIndex));
        put(static_cast<uint64_t>(generation));
        put(eye);
        put(yaw);
        put(pitch);
        put(static_cast<uint64_t>(cells.size()));
        out.write(reinterpret_cast<const char*>(cells.data()), std::streamsize(cells.size() * sizeof(glm::ivec3)));
        put(static_cast<uint64_t>(blocks.size()));
        out.write(reinterpret_cast<const char*>(blocks.data()), std::streamsize(blocks.size() * sizeof(glm::ivec4)));
        if (!out) {
            std::cerr << "Failed to save " << path << std::endl;
            return false;
        }
        std::cout << "Saved " << cells.size() << " cells and " << blocks.size() << " blocks to "
                  << std::filesystem::absolute(path).string() << std::endl;
        return true;
    }

    bool loadWorld(const std::string& path) {
        std::ifstream in(path, std::ios::binary);
        char magic[4] = {};
        uint32_t savedRule = 0;
        uint64_t savedGeneration = 0, count = 0, blockTotal = 0;
        glm::vec3 savedEye(0.0f);
        float savedYaw = 0.0f, savedPitch = 0.0f;
        auto get = [&](auto& value) { in.read(reinterpret_cast<char*>(&value), sizeof(value)); };
        in.read(magic, 4);
        std::string format(magic, 4);
        get(savedRule);
        get(savedGeneration);
        get(savedEye);
        get(savedYaw);
        get(savedPitch);
        get(count);
        // Never trust a count further than the file reaches.
        std::streamoff here = in.tellg();
        in.seekg(0, std::ios::end);
        const uint64_t remaining = in ? static_cast<uint64_t>(in.tellg() - here) : 0;
        in.seekg(here);
        if (!in || (format != "L3D1" && format != "L3D2") || savedRule >= lifeRules().size() ||
            count > remaining / sizeof(glm::ivec3)) {
            std::cerr << "Not a 3D Life save: " << path << std::endl;
            return false;
        }
        std::vector<glm::ivec3> cells(count);
        in.read(reinterpret_cast<char*>(cells.data()), std::streamsize(count * sizeof(glm::ivec3)));
        std::vector<glm::ivec4> blocks;
        if (format == "L3D2") {
            get(blockTotal);
            if (blockTotal > remaining / sizeof(glm::ivec4)) in.setstate(std::ios::failbit);
            else {
                blocks.resize(blockTotal);
                in.read(reinterpret_cast<char*>(blocks.data()), std::streamsize(blockTotal * sizeof(glm::ivec4)));
            }
        }
        if (!in) {
            std::cerr << "Truncated save: " << path << std::endl;
            return false;
        }
        tutorialPanel.close();
        resetChunks();
        for (const glm::ivec4& block : blocks) {
            if (block.w > 0 && block.w < static_cast<int>(cellTypes().size())) placeCell(glm::ivec3(block), static_cast<CellKind>(block.w));
        }
        for (const glm::ivec3& cell : cells) setLife(cell, true);
        ruleIndex = savedRule;
        generation = savedGeneration;
        eye = savedEye;
        yaw = savedYaw;
        pitch = savedPitch;
        verticalSpeed = 0.0f;
        stepDebt = 0.0;
        fastForward = fastForwardTotal = 0;
        rebuild(Rebuild::Reset);
        std::cout << "Loaded " << population << " cells and " << blocks.size() << " blocks from " << path
                  << (chunkLimitHit ? " (chunk limit reached; some cells were dropped)" : "") << std::endl;
        return true;
    }

    // ------------------------------------------------------------ GPU passes

    VkCommandBuffer beginCompute() {
        vkResetCommandBuffer(computeCommandBuffer, 0);
        VkCommandBufferBeginInfo beginInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkBeginCommandBuffer(computeCommandBuffer, &beginInfo);
        return computeCommandBuffer;
    }

    void submitCompute(VkCommandBuffer cmd, const char* what) {
        vkEndCommandBuffer(cmd);
        VkCommandBufferSubmitInfo commandInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO};
        commandInfo.commandBuffer = cmd;
        VkSubmitInfo2 submitInfo{VK_STRUCTURE_TYPE_SUBMIT_INFO_2};
        submitInfo.commandBufferInfoCount = 1;
        submitInfo.pCommandBufferInfos = &commandInfo;
        vkResetFences(device, 1, &computeFence);
        if (vkQueueSubmit2(gpu.queue, 1, &submitInfo, computeFence) != VK_SUCCESS) {
            throw std::runtime_error("Failed to submit compute work!");
        }
        waitFence(computeFence, what);
    }

    static void memoryBarrier(VkCommandBuffer cmd, VkPipelineStageFlags2 srcStage, VkAccessFlags2 srcAccess,
                              VkPipelineStageFlags2 dstStage, VkAccessFlags2 dstAccess) {
        VkMemoryBarrier2 barrier{VK_STRUCTURE_TYPE_MEMORY_BARRIER_2};
        barrier.srcStageMask = srcStage;
        barrier.srcAccessMask = srcAccess;
        barrier.dstStageMask = dstStage;
        barrier.dstAccessMask = dstAccess;
        VkDependencyInfo dependency{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
        dependency.memoryBarrierCount = 1;
        dependency.pMemoryBarriers = &barrier;
        vkCmdPipelineBarrier2(cmd, &dependency);
    }

    // Dispatches a compute pass over the active list in ranges of at most
    // MAX_DISPATCH chunks; each chunk is four 32 x 8 slabs.
    template <typename Constants> void dispatchChunks(VkCommandBuffer cmd, Constants constants) {
        const uint32_t total = static_cast<uint32_t>(activeSlots.size());
        for (uint32_t first = 0; first < total; first += MAX_DISPATCH) {
            constants.firstIndex = first;
            constants.count = std::min(MAX_DISPATCH, total - first);
            vkCmdPushConstants(cmd, computePipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(constants), &constants);
            vkCmdDispatch(cmd, CHUNK / 8, constants.count, 1);
        }
    }

    // Advances `steps` generations (at most MAX_BATCH) in one GPU submission,
    // then refreshes the chunk stats, updates the chunk set and, with `draw`,
    // rebuilds the block list. With steps = 0 it only rebuilds (after edits or
    // camera moves). `animate` flags births and draws deaths of the last change
    // so they can be animated. Batches that are not the last of a frame skip the
    // block list; nobody would see it. `stats` = false skips the per-chunk stats
    // and chunk bookkeeping, for draws of a world that has not changed since the
    // last pass with stats (camera moves, the draw after a frame's steps).
    void runBatch(uint32_t steps, bool animate, bool draw = true, bool stats = true) {
        stats = stats || steps > 0;
        if (draw && glm::distance(eye, lastSortEye) > REBUILD_DISTANCE) sortChunksByDistance();
        const auto wallStart = std::chrono::steady_clock::now();
        VkCommandBuffer cmd = beginCompute();
        if (timestamps) {
            vkCmdResetQueryPool(cmd, timestamps, 0, 3);
            vkCmdWriteTimestamp2(cmd, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, timestamps, 0);
        }
        // Earlier frames may still be drawing the block list this pass rewrites.
        memoryBarrier(cmd, VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT | VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                      VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT | VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT,
                      VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_2_CLEAR_BIT | VK_PIPELINE_STAGE_2_COPY_BIT,
                      VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT | VK_ACCESS_2_TRANSFER_WRITE_BIT);
        uint32_t parity = currentCells;
        if (steps > 0 && !activeSlots.empty()) {
            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, stepPipeline);
            for (uint32_t s = 0; s < steps; ++s) {
                vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, computePipelineLayout, 0, 1, &computeSets[parity], 0, nullptr);
                dispatchChunks(cmd, StepConstants{0, 0, rule().surviveMask, rule().birthMask});
                memoryBarrier(cmd, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_WRITE_BIT,
                              VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT);
                parity = 1 - parity;
            }
        } else if (steps > 0) {
            parity = (parity + steps) & 1u; // an empty world stays empty
        }
        if (timestamps) vkCmdWriteTimestamp2(cmd, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, timestamps, 1);

        if (stats) vkCmdFillBuffer(cmd, pool.stats.buffer, 0, VK_WHOLE_SIZE, 0);
        const VkDrawIndexedIndirectCommand emptyDraw{BLOCK_INDICES, 0, 0, 0, 0};
        if (draw) vkCmdUpdateBuffer(cmd, indirectBuffer.buffer, 0, sizeof(emptyDraw), &emptyDraw);
        memoryBarrier(cmd, VK_PIPELINE_STAGE_2_CLEAR_BIT | VK_PIPELINE_STAGE_2_COPY_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT,
                      VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT);
        if (!activeSlots.empty()) {
            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, buildPipeline);
            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, computePipelineLayout, 0, 1, &computeSets[parity], 0, nullptr);
            float cull = static_cast<float>(settings.renderDistance) + 2.0f * REBUILD_DISTANCE;
            uint32_t flags = (animate ? 1u : 0u) | (draw ? 2u : 0u) | (stats ? 4u : 0u);
            dispatchChunks(cmd, BuildConstants{cullingViewProjection(), 0, 0, MAX_BATCH, flags, MAX_INSTANCES, cull,
                                               eye.x, eye.y, eye.z});
        }
        if (timestamps) vkCmdWriteTimestamp2(cmd, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, timestamps, 2);
        // Results go to the indirect draw, the vertex shader and the CPU.
        memoryBarrier(cmd, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_WRITE_BIT,
                      VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT | VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_2_COPY_BIT |
                          VK_PIPELINE_STAGE_2_HOST_BIT,
                      VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT | VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_TRANSFER_READ_BIT |
                          VK_ACCESS_2_HOST_READ_BIT);
        VkBufferCopy copy{0, 0, sizeof(VkDrawIndexedIndirectCommand)};
        vkCmdCopyBuffer(cmd, indirectBuffer.buffer, readbackBuffer.buffer, 1, &copy);
        if (stats) {
            VkBufferCopy statsCopy{0, 0, pool.stats.size};
            vkCmdCopyBuffer(cmd, pool.stats.buffer, pool.statsReadback.buffer, 1, &statsCopy);
        }
        memoryBarrier(cmd, VK_PIPELINE_STAGE_2_COPY_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_2_HOST_BIT,
                      VK_ACCESS_2_HOST_READ_BIT);
        auto t0 = std::chrono::steady_clock::now();
        submitCompute(cmd, steps ? "the simulation pass" : "the block list");
        auto t1 = std::chrono::steady_clock::now();
        benchGpuMs += std::chrono::duration<double, std::milli>(t1 - t0).count();

        currentCells = parity;
        generation += steps;
        editOpen = false;
        measureBatch(steps, draw && !stats, wallStart);
        blockListStale = !draw;
        if (draw) {
            visibleBlocks = readbackBuffer.as<VkDrawIndexedIndirectCommand>()->instanceCount;
            drawnBlocks = std::min<uint64_t>(visibleBlocks, MAX_INSTANCES);
            refreshPending = false;
            lastBuildEye = eye;
            lastBuildForward = forward();
            lastBuildAnimated = animate;
        }
        if (!stats) return;
        auto t2 = std::chrono::steady_clock::now();
        maintainChunks();
        benchCpuMs += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t2).count();
        if (steps > 0) recordPopulation();
    }

    // Updates the governor's cost estimates from the batch that just finished.
    void measureBatch(uint32_t steps, bool draw, std::chrono::steady_clock::time_point wallStart) {
        auto smooth = [](double& estimate, double sample) { estimate = estimate > 0.0 ? 0.8 * estimate + 0.2 * sample : sample; };
        double wall = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - wallStart).count();
        uint64_t ticks[3] = {};
        if (timestamps && vkGetQueryPoolResults(device, timestamps, 0, 3, sizeof(ticks), ticks, sizeof(uint64_t),
                                                VK_QUERY_RESULT_64_BIT) == VK_SUCCESS) {
            // Counters may wrap past their valid bits; a pass never reads as free.
            double stepMs = std::max(1e-4, static_cast<double>((ticks[1] - ticks[0]) & timestampMask) * timestampMs);
            double buildMs = std::max(1e-4, static_cast<double>((ticks[2] - ticks[1]) & timestampMask) * timestampMs);
            if (steps > 0) smooth(stepCostMs, stepMs / steps);
            smooth(draw ? drawCostMs : statsCostMs, buildMs);
            smooth(overheadMs, std::max(0.0, wall - stepMs - buildMs));
        } else { // no timestamps: charge everything to the steps
            if (steps > 0) smooth(stepCostMs, std::max(1e-4, wall / steps));
            else smooth(draw ? drawCostMs : statsCostMs, wall);
        }
    }

    // Orders the active list nearest first. The build pass appends blocks roughly
    // in that order, so the depth test rejects most hidden fragments early and,
    // past the draw cap, the blocks left out are the far ones.
    void sortChunksByDistance() {
        lastSortEye = eye;
        glm::vec3 center = eye - float(CHUNK) * 0.5f;
        auto distance2 = [&](uint32_t slot) {
            glm::vec3 d = glm::vec3(slotChunk[slot] * CHUNK) - center;
            return glm::dot(d, d);
        };
        std::vector<std::pair<float, uint32_t>> order;
        order.reserve(activeSlots.size());
        for (uint32_t slot : activeSlots) order.emplace_back(distance2(slot), slot);
        std::sort(order.begin(), order.end());
        uint32_t* mapped = pool.active.as<uint32_t>();
        for (size_t i = 0; i < order.size(); ++i) {
            activeSlots[i] = order[i].second;
            activeIndex[order[i].second] = static_cast<uint32_t>(i);
            mapped[i] = order[i].second;
        }
    }

    // Why the block list is rebuilt without a generation step.
    enum class Rebuild {
        Edit,   // the player changed blocks: animate them if animations are on
        Camera, // the camera moved: keep any animation in progress
        Reset,  // a new world, a load or a tutorial scene: nothing animates
    };

    void rebuild(Rebuild reason) {
        const double now = glfwGetTime();
        bool animate = false;
        if (reason == Rebuild::Edit && editOpen) {
            lastChangeTime = now;
            changeAnimationSeconds = EDIT_ANIMATION_SECONDS;
            animate = true;
        } else if (reason == Rebuild::Camera) {
            animate = lastBuildAnimated && now < lastChangeTime + changeAnimationSeconds;
        } else {
            changeAnimationSeconds = 0.0f;
        }
        runBatch(0, animate, true, reason != Rebuild::Camera);
    }

    void recordPopulation() {
        constexpr size_t HISTORY = 240;
        populationHistory.push_back(static_cast<float>(population));
        if (populationHistory.size() > HISTORY) populationHistory.erase(populationHistory.begin());
    }

    // Runs n generations as fast as possible, in full batches (scripts, --steps).
    void advanceGenerations(uint64_t n) {
        while (n > 0) {
            uint32_t batch = static_cast<uint32_t>(std::min<uint64_t>(n, MAX_BATCH));
            n -= batch;
            runBatch(batch, false, n == 0);
        }
    }

    // ------------------------------------------------------------ simulation speed

    bool animations() const { return settings.animate && !capturing; }

    double targetRate() const { return std::ldexp(1.0, speedExponent); }
    bool unlimitedSpeed() const { return speedExponent >= UNLIMITED_SPEED_EXPONENT; }

    static std::string formatRate(double rate) {
        std::ostringstream out;
        if (rate >= 1.0 || rate <= 0.0) out << std::lround(rate);
        else out << "1/" << std::lround(1.0 / rate);
        return out.str();
    }

    std::string speedLabel() const {
        return unlimitedSpeed() ? std::string("max") : formatRate(targetRate()) + " gen/s";
    }

    void changeSpeed(int steps) {
        speedExponent = std::clamp(speedExponent + steps, MIN_SPEED_EXPONENT, UNLIMITED_SPEED_EXPONENT);
        stepDebt = std::min(stepDebt, 1.0);
        notify("Speed: " + (unlimitedSpeed() ? std::string("as fast as possible") : speedLabel()));
    }

    void queueFastForward(uint64_t generations) {
        if (fastForward > 0) {
            fastForward = fastForwardTotal = 0;
            notify("Fast-forward cancelled");
            return;
        }
        fastForward = fastForwardTotal = generations;
        notify("Fast-forward " + std::to_string(generations) + " generations (J to cancel)");
    }

    // The governor. Each frame it owes the simulation `rate * dt` generations (or
    // a fast-forward's), and spends at most the frame budget paying them, using
    // the measured cost of a generation to size GPU batches. When the world is too
    // big to keep up, the tick rate drops to what fits instead of the frame rate:
    // the backlog is dropped and the HUD shows the speed actually reached. A
    // single generation that blows the budget is followed by enough idle frames
    // to keep the game at least half responsive.
    void updateSimulation(float deltaTime) {
        const double now = glfwGetTime();
        // Edits since the last pass must reach chunk bookkeeping before any step,
        // or births next to new cells could fall into chunks that don't exist yet.
        if (refreshPending) rebuild(Rebuild::Edit);
        uint64_t due = fastForward;
        if (simulationRunning) {
            stepDebt = unlimitedSpeed() ? 1e12 : stepDebt + deltaTime * targetRate();
            due += static_cast<uint64_t>(std::min(stepDebt, 1e12));
        }
        sampleRate(now);
        if (due == 0) return;
        if (now < simCooldownUntil) {
            noteLimited(now, true);
            return;
        }
        const bool flatOut = unlimitedSpeed() || fastForward > 0;
        const double budgetMs = flatOut ? std::max(settings.simBudget, 25) : settings.simBudget;
        // The GPU runs frames and the simulation in order; let the last frame
        // finish first so its drawing is not counted as simulation time.
        waitFence(inFlightFences[(currentFrame + MAX_FRAMES_IN_FLIGHT - 1) % MAX_FRAMES_IN_FLIGHT], "a frame");
        const auto start = std::chrono::steady_clock::now();
        auto elapsedMs = [&] { return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count(); };
        // Steps run in batches without a block list; one build at the end draws
        // the result, so its cost is set aside first.
        const double reserve = drawCostMs + overheadMs;
        // At slow speeds a single generation's births and deaths are animated over
        // most of the tick. That generation draws in its own batch, before chunk
        // bookkeeping frees chunks whose last cells are still shrinking away.
        const bool animate = animations() && !flatOut && due == 1 && targetRate() <= 16.0;
        uint64_t done = 0;
        if (animate) {
            runBatch(1, true, true);
            done = 1;
        }
        while (done < due) {
            uint32_t batch = static_cast<uint32_t>(std::min<uint64_t>(due - done, MAX_BATCH));
            const double elapsed = elapsedMs();
            if (done > 0 && elapsed >= budgetMs) break;
            if (stepCostMs > 0.0) {
                double room = budgetMs - elapsed - reserve - statsCostMs - overheadMs;
                double affordable = room > 0.0 ? std::floor(room / stepCostMs) : 0.0;
                if (done > 0 && affordable < 1.0) break;
                batch = static_cast<uint32_t>(std::clamp(affordable, 1.0, static_cast<double>(batch)));
            }
            runBatch(batch, false, false);
            done += batch;
            if (chunkLimitHit && !pausedAtLimit) {
                // Past the limit, growth freezes at the edge; stop and say so instead.
                pausedAtLimit = true;
                simulationRunning = false;
                fastForward = 0;
                notify("Chunk limit of " + std::to_string(chunkLimit) + " reached at generation " + std::to_string(generation) +
                       ": paused. G continues (growth stops at the edge); --chunks N raises the limit.");
                break;
            }
        }
        // The block list for what the frame will show.
        if (!animate) runBatch(0, false, true, false);
        if (animate) {
            lastChangeTime = glfwGetTime();
            changeAnimationSeconds = static_cast<float>(std::min(0.22, 0.75 / targetRate()));
        } else {
            changeAnimationSeconds = 0.0f;
        }
        const uint64_t fromJump = std::min(done, fastForward);
        fastForward -= fromJump;
        if (fastForward == 0) fastForwardTotal = 0;
        if (simulationRunning) stepDebt = std::max(0.0, stepDebt - static_cast<double>(done - fromJump));
        const bool behind = done < due && !flatOut;
        if (behind) stepDebt = std::min(stepDebt, 1.0); // drop the backlog: slow down instead of spiraling
        if (unlimitedSpeed()) stepDebt = 0.0;
        noteLimited(now, behind);
        lastSimMs = static_cast<float>(elapsedMs());
        if (lastSimMs > 1.5 * budgetMs) simCooldownUntil = now + lastSimMs / 1000.0; // keep at least half the time for frames
    }

    void noteLimited(double now, bool limited) {
        // Hysteresis so the HUD does not flicker between the two states.
        if (limited) {
            limitedSince = now;
            tickLimited = true;
        } else if (tickLimited && now - limitedSince > 1.0) {
            tickLimited = false;
        }
    }

    // Generations per second actually reached over the last second or so.
    void sampleRate(double now) {
        rateSamples.emplace_back(now, generation);
        while (rateSamples.size() > 2 && now - rateSamples.front().first > 1.5) rateSamples.erase(rateSamples.begin());
        double span = now - rateSamples.front().first;
        measuredRate = span > 0.25 ? static_cast<double>(generation - rateSamples.front().second) / span : measuredRate;
    }

    // ---------------------------------------------------------------- checks

    // Compares the chunked GPU world with the dense CPU reference, for every rule,
    // with Stone and Ember blocks mixed into the soup, one generation per batch
    // and full batches. The soup is centered on a chunk corner so neighbor
    // lookups, chunk growth and the reach margin are exercised.
    bool verifyAgainstReference() {
        constexpr int BOX = 96, HALF = BOX / 2, SOUP = 14, STEPS = 24;
        bool ok = true;
        std::vector<uint32_t> expected(size_t(BOX) * BOX * BOX), scratch;
        std::vector<uint8_t> blocks(expected.size());
        auto index = [&](int x, int y, int z) { return (size_t(z) * BOX + y) * BOX + x; };
        for (uint32_t batch : {1u, MAX_BATCH}) {
            for (size_t r = 0; r < lifeRules().size(); ++r) {
                ruleIndex = r;
                resetChunks();
                generation = 0;
                rng.seed(options.seed + static_cast<uint32_t>(r));
                std::uniform_real_distribution<float> chance(0.0f, 1.0f);
                float density = std::max(rule().seedDensity, 0.25f);
                for (int z = -SOUP / 2; z < SOUP / 2; ++z)
                    for (int y = -SOUP / 2; y < SOUP / 2; ++y)
                        for (int x = -SOUP / 2; x < SOUP / 2; ++x) {
                            float roll = chance(rng);
                            if (roll < 0.015f) placeCell({x, y, z}, CellKind::Stone);
                            else if (roll < 0.03f) placeCell({x, y, z}, CellKind::Ember);
                            else if (roll < 0.03f + density) setLife({x, y, z}, true);
                        }
                runBatch(0, false);
                for (int z = 0; z < BOX; ++z)
                    for (int y = 0; y < BOX; ++y)
                        for (int x = 0; x < BOX; ++x) {
                            int kind = cellKind(glm::ivec3(x, y, z) - HALF);
                            expected[index(x, y, z)] = kind == static_cast<int>(CellKind::Life) ? 1u : 0u;
                            blocks[index(x, y, z)] = kind > 0 ? static_cast<uint8_t>(kind) : 0;
                        }
                size_t mismatches = 0;
                for (int step = 0; step < STEPS; step += static_cast<int>(batch)) {
                    for (uint32_t i = 0; i < batch; ++i) {
                        stepLifeReference(expected, scratch, BOX, BOX, BOX, rule(), &blocks);
                        expected.swap(scratch);
                    }
                    runBatch(batch, false);
                    uint64_t expectedPopulation = 0;
                    for (int z = 0; z < BOX; ++z)
                        for (int y = 0; y < BOX; ++y)
                            for (int x = 0; x < BOX; ++x) {
                                uint32_t want = expected[index(x, y, z)];
                                expectedPopulation += want;
                                mismatches += (cellAlive(glm::ivec3(x, y, z) - HALF) ? 1u : 0u) != want;
                            }
                    mismatches += expectedPopulation != population; // catches stray cells outside the box
                }
                std::cout << "verify " << rule().name << " " << describeRule(rule()) << ", " << batch
                          << (batch == 1 ? " generation" : " generations") << " per batch: "
                          << (mismatches ? "FAIL" : "ok") << " (" << mismatches << " mismatches, population "
                          << population << ", " << activeSlots.size() << " chunks)" << std::endl;
                ok = ok && mismatches == 0;
            }
        }
        return ok;
    }

    // --bench N: the simulation alone, as fast as it goes, from the starting world.
    int runBenchmark(uint64_t generations) {
        uint64_t peakChunks = activeSlots.size(), startPopulation = population;
        auto start = std::chrono::steady_clock::now();
        uint64_t done = 0;
        while (done < generations && !(chunkLimitHit && pausedAtLimit)) {
            uint32_t batch = static_cast<uint32_t>(std::min<uint64_t>(generations - done, MAX_BATCH));
            done += batch;
            runBatch(batch, false, done >= generations);
            peakChunks = std::max<uint64_t>(peakChunks, activeSlots.size());
            if (chunkLimitHit) pausedAtLimit = true;
        }
        if (blockListStale) runBatch(0, false);
        double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
        std::cout << std::fixed << std::setprecision(1) << "bench: " << rule().name << ", " << done << " generations in "
                  << seconds << " s = " << static_cast<double>(done) / seconds << " gen/s; population " << startPopulation
                  << " -> " << population << ", peak " << peakChunks << " chunks (" << pool.capacity
                  << " allocated), " << drawnBlocks << " blocks drawn, GPU memory " << (gpuBytes >> 20) << " MB"
                  << (chunkLimitHit ? " (chunk limit reached)" : "") << std::endl;
        std::cout << "bench: " << benchGpuMs << " ms in GPU passes, " << benchCpuMs << " ms in chunk bookkeeping" << std::endl;
        return 0;
    }

    // ----------------------------------------------------------------- player

    glm::vec3 forward() const {
        float y = glm::radians(yaw), p = glm::radians(pitch);
        return glm::vec3(std::cos(p) * std::cos(y), std::sin(p), std::cos(p) * std::sin(y));
    }

    glm::mat4 projection() const {
        float aspect = swapchainExtent.height ? float(swapchainExtent.width) / float(swapchainExtent.height) : 1.0f;
        glm::mat4 proj = glm::perspective(glm::radians(settings.fov), aspect, 0.05f, 1000.0f);
        proj[1][1] *= -1; // Vulkan clip space has Y pointing down
        return proj;
    }

    glm::mat4 viewProjection() const {
        return projection() * glm::lookAt(eye, eye + forward(), glm::vec3(0, 1, 0));
    }

    // The view the block list is culled to: CULL_MARGIN_DEGREES wider than the
    // camera's on every side and starting a little behind it, so turning a
    // little or stepping back does not uncover missing blocks before the next
    // rebuild (see mainLoop).
    static constexpr float CULL_MARGIN_DEGREES = 20.0f;
    glm::mat4 cullingViewProjection() const {
        float aspect = swapchainExtent.height ? float(swapchainExtent.width) / float(swapchainExtent.height) : 1.0f;
        float halfY = glm::radians(settings.fov * 0.5f);
        float halfX = std::atan(std::tan(halfY) * aspect);
        float wideY = std::min(halfY + glm::radians(CULL_MARGIN_DEGREES), glm::radians(85.0f));
        float wideX = std::min(halfX + glm::radians(CULL_MARGIN_DEGREES), glm::radians(85.0f));
        glm::mat4 proj = glm::perspective(2.0f * wideY, std::tan(wideX) / std::tan(wideY), 0.05f, 4096.0f);
        glm::vec3 back = eye - forward() * (2.0f * REBUILD_DISTANCE);
        return proj * glm::lookAt(back, back + forward(), glm::vec3(0, 1, 0));
    }

    // Keyboard input only counts while the mouse is captured; gravity always applies.
    void updateMovement(float deltaTime) {
        auto down = [&](int key) { return cursorCaptured && glfwGetKey(window, key) == GLFW_PRESS; };
        float y = glm::radians(yaw);
        glm::vec3 flatForward(std::cos(y), 0.0f, std::sin(y));
        glm::vec3 right(-std::sin(y), 0.0f, std::cos(y));
        glm::vec3 move(0.0f);
        if (down(GLFW_KEY_W)) move += flatForward;
        if (down(GLFW_KEY_S)) move -= flatForward;
        if (down(GLFW_KEY_D)) move += right;
        if (down(GLFW_KEY_A)) move -= right;
        if (glm::dot(move, move) > 0.0f) move = glm::normalize(move);
        const bool sprint = down(GLFW_KEY_LEFT_CONTROL);

        glm::vec3 delta;
        if (flying) {
            if (down(GLFW_KEY_SPACE)) move.y += 1.0f;
            if (down(GLFW_KEY_LEFT_SHIFT)) move.y -= 1.0f;
            delta = move * FLY_SPEED * (sprint ? 2.0f : 1.0f) * deltaTime;
        } else {
            onGround = standingOnSomething();
            if (down(GLFW_KEY_SPACE) && onGround) verticalSpeed = JUMP_SPEED;
            verticalSpeed = std::max(verticalSpeed - GRAVITY * deltaTime, -TERMINAL_SPEED);
            delta = move * (sprint ? SPRINT_SPEED : WALK_SPEED) * deltaTime;
            delta.y = verticalSpeed * deltaTime;
        }
        moveWithCollision(delta);
    }

    // Minecraft's player box: 0.6 wide, 1.8 tall, eyes 1.62 above the feet.
    static constexpr float EYE_HEIGHT = 1.62f;
    static glm::vec3 playerMin(const glm::vec3& at) { return at - glm::vec3(0.3f, EYE_HEIGHT, 0.3f); }
    static glm::vec3 playerMax(const glm::vec3& at) { return at + glm::vec3(0.3f, 1.8f - EYE_HEIGHT, 0.3f); }

    // True when a block or the ground is directly under the player's feet.
    bool standingOnSomething() const {
        constexpr float PROBE = 0.01f;
        glm::vec3 lo = playerMin(eye), hi = playerMax(eye);
        if (lo.y >= 0.0f && lo.y < PROBE) return true;
        int below = static_cast<int>(std::floor(lo.y - PROBE));
        if (below >= static_cast<int>(std::floor(lo.y))) return false; // feet are not near a block top
        for (int z = static_cast<int>(std::floor(lo.z)); z < static_cast<int>(std::ceil(hi.z)); ++z)
            for (int x = static_cast<int>(std::floor(lo.x)); x < static_cast<int>(std::ceil(hi.x)); ++x)
                if (occupied(glm::ivec3(x, below, z))) return true;
        return false;
    }

    // Moves one axis at a time (y first, like Minecraft) and stops at blocks the
    // player was not already inside. Blocks born inside the player never trap it.
    // While walking, the y = 0 ground is solid.
    void moveWithCollision(const glm::vec3& delta) {
        constexpr float GAP = 1e-3f;
        for (int axis : {1, 0, 2}) {
            if (delta[axis] == 0.0f) continue;
            glm::vec3 next = eye;
            next[axis] += delta[axis];
            glm::vec3 oldLo = playerMin(eye), oldHi = playerMax(eye);
            glm::vec3 lo = playerMin(next), hi = playerMax(next);
            bool blocked = false;
            float stop = delta[axis] > 0 ? std::numeric_limits<float>::max() : std::numeric_limits<float>::lowest();
            // Sweep the whole path so large steps (lag frames, sprint flying) cannot tunnel.
            glm::ivec3 first = glm::ivec3(glm::floor(glm::min(lo, oldLo)));
            glm::ivec3 last = glm::ivec3(glm::ceil(glm::max(hi, oldHi))) - 1;
            for (int z = first.z; z <= last.z; ++z)
                for (int yy = first.y; yy <= last.y; ++yy)
                    for (int x = first.x; x <= last.x; ++x) {
                        glm::vec3 c(x, yy, z);
                        bool wasInside = c.x < oldHi.x && c.x + 1 > oldLo.x && c.y < oldHi.y && c.y + 1 > oldLo.y &&
                                         c.z < oldHi.z && c.z + 1 > oldLo.z;
                        if (wasInside || !occupied(glm::ivec3(x, yy, z))) continue;
                        blocked = true;
                        stop = delta[axis] > 0 ? std::min(stop, c[axis]) : std::max(stop, c[axis] + 1.0f);
                    }
            if (!flying && axis == 1 && lo.y < 0.0f && oldLo.y >= 0.0f) {
                blocked = true;
                stop = std::max(stop, 0.0f);
            }
            if (blocked) {
                // Put the box against the blocking face.
                next[axis] = delta[axis] > 0 ? stop - (playerMax(eye)[axis] - eye[axis]) - GAP
                                             : stop + (eye[axis] - playerMin(eye)[axis]) + GAP;
                if (axis == 1) verticalSpeed = 0.0f;
            }
            eye = next;
        }
        onGround = !flying && standingOnSomething();
    }

    // Walks the crosshair ray through blocks (Amanatides-Woo) up to REACH.
    // Stamps are placed against the face that was hit, else on the y = 0 ground
    // within reach, else into empty air a few blocks ahead.
    void updateTarget() {
        target = Target{};
        glm::vec3 dir = forward();
        glm::ivec3 cell = glm::ivec3(glm::floor(eye));
        glm::ivec3 step(dir.x > 0 ? 1 : -1, dir.y > 0 ? 1 : -1, dir.z > 0 ? 1 : -1);
        glm::vec3 tMax, tDelta;
        for (int axis = 0; axis < 3; ++axis) {
            if (std::abs(dir[axis]) < 1e-8f) {
                tMax[axis] = tDelta[axis] = std::numeric_limits<float>::max();
                continue;
            }
            float boundary = static_cast<float>(cell[axis] + (step[axis] > 0 ? 1 : 0));
            tMax[axis] = (boundary - eye[axis]) / dir[axis];
            tDelta[axis] = std::abs(1.0f / dir[axis]);
        }
        int lastAxis = -1;
        float t = 0.0f;
        while (t <= REACH) {
            if (occupied(cell)) {
                target.hit = true;
                target.block = cell;
                if (lastAxis >= 0) {
                    target.normal = glm::ivec3(0);
                    target.normal[lastAxis] = -step[lastAxis];
                    target.place = cell + target.normal;
                    target.canPlace = true;
                }
                return;
            }
            lastAxis = tMax.x < tMax.y ? (tMax.x < tMax.z ? 0 : 2) : (tMax.y < tMax.z ? 1 : 2);
            t = tMax[lastAxis];
            cell[lastAxis] += step[lastAxis];
            tMax[lastAxis] += tDelta[lastAxis];
        }
        // The y = 0 ground: build on top of it like Minecraft's surface.
        if (eye.y > 0.0f && dir.y < 0.0f && -eye.y / dir.y <= REACH) {
            glm::vec3 p = eye + dir * (-eye.y / dir.y);
            target.place = glm::ivec3(static_cast<int>(std::floor(p.x)), 0, static_cast<int>(std::floor(p.z)));
            target.normal = glm::ivec3(0, 1, 0);
            target.canPlace = true;
            return;
        }
        glm::vec3 absDir = glm::abs(dir);
        int major = absDir.x > absDir.y ? (absDir.x > absDir.z ? 0 : 2) : (absDir.y > absDir.z ? 1 : 2);
        target.normal = glm::ivec3(0);
        target.normal[major] = step[major];
        target.place = glm::ivec3(glm::floor(eye + dir * AIR_PLACE_DISTANCE));
        target.canPlace = true;
    }

    // Cells of the selected stamp. Stamps are centered across the target face and
    // extend away from it along `normal`, so they never overlap the targeted block.
    // Q/E rotate them in quarter turns around `normal`, then Z/C tilt them in
    // quarter turns around the world x axis. `solid` fills soups completely (for
    // the placement outline).
    std::vector<glm::ivec3> stampCells(Stamp stamp, const glm::ivec3& anchor, const glm::ivec3& normal, bool solid = false) {
        int axis = normal.x != 0 ? 0 : normal.y != 0 ? 1 : 2;
        glm::ivec3 u(0), v(0);
        u[(axis + 1) % 3] = 1;
        v[(axis + 2) % 3] = 1;
        std::vector<glm::ivec3> cells;
        auto box = [&](int across, int along, float density) {
            std::uniform_real_distribution<float> chance(0.0f, 1.0f);
            for (int c = 0; c < along; ++c)
                for (int b = 0; b < across; ++b)
                    for (int a = 0; a < across; ++a)
                        if (solid || density >= 1.0f || chance(rng) < density)
                            cells.push_back(anchor + u * (a - across / 2) + v * (b - across / 2) + normal * c);
        };
        switch (stamp) {
            case Stamp::Cell: cells.push_back(anchor); break;
            case Stamp::Block: box(2, 2, 1.0f); break;
            case Stamp::Plus:
                for (const glm::ivec3& d : {glm::ivec3(0), u, -u, v, -v, normal, -normal}) cells.push_back(anchor + normal + d);
                break;
            case Stamp::SmallSoup: box(8, 8, std::max(rule().seedDensity, 0.25f)); break;
            case Stamp::BigSoup: box(16, 16, std::max(rule().seedDensity, 0.25f)); break;
            case Stamp::Wall:
                if (axis == 1) {
                    // On a floor or ceiling, stand the wall up across the view direction.
                    glm::vec3 view = forward();
                    glm::ivec3 across = std::abs(view.x) > std::abs(view.z) ? glm::ivec3(0, 0, 1) : glm::ivec3(1, 0, 0);
                    for (int h = 0; h < 5; ++h)
                        for (int a = 0; a < 5; ++a) cells.push_back(anchor + across * (a - 2) + normal * h);
                } else {
                    box(5, 1, 1.0f);
                }
                break;
            case Stamp::Pillar: box(1, 8, 1.0f); break;
            case Stamp::RuleSeed: box(rule().seedSize, rule().seedSize, rule().seedDensity); break;
            case Stamp::Glider: {
                // Pattern x and z lie across the surface, pattern y grows away from it,
                // so the glider slides along the surface it is placed on.
                const Pattern glider = std::string(rule().name) == "Life 4555" ? life4555Glider() : life5766Glider();
                for (const PatternCell& c : glider) cells.push_back(anchor + u * (c.x - 1) + v * (c.z - 1) + normal * c.y);
                break;
            }
        }
        for (glm::ivec3& cell : cells) {
            glm::ivec3 d = cell - anchor;
            for (int turn = 0; turn < brushRotation; ++turn) {
                int a = d[(axis + 1) % 3], b = d[(axis + 2) % 3];
                d[(axis + 1) % 3] = -b;
                d[(axis + 2) % 3] = a;
            }
            for (int turn = 0; turn < brushTilt; ++turn) d = glm::ivec3(d.x, -d.z, d.y);
            cell = anchor + d;
        }
        return cells;
    }

    bool overlapsPlayer(const glm::ivec3& cell) const {
        glm::vec3 lo = playerMin(eye), hi = playerMax(eye);
        glm::vec3 c(cell);
        return c.x < hi.x && c.x + 1.0f > lo.x && c.y < hi.y && c.y + 1.0f > lo.y && c.z < hi.z && c.z + 1.0f > lo.z;
    }

    // Stamps are made of the selected material (M) and only fill empty cells.
    void placeStamp() {
        if (!target.canPlace || hotbarSlot < 0) return;
        for (const glm::ivec3& cell : stampCells(static_cast<Stamp>(hotbarSlot), target.place, target.normal)) {
            if (!overlapsPlayer(cell)) placeCell(cell, material);
        }
    }

    void breakBlock() {
        if (target.hit) clearCell(target.block);
    }

    void applyScriptAction(const ScriptAction& action) {
        switch (action.kind) {
            case ScriptAction::Position: eye = action.value; break;
            case ScriptAction::Look:
                yaw = action.value.x;
                pitch = std::clamp(action.value.y, -89.9f, 89.9f);
                break;
            case ScriptAction::Slot:
                hotbarSlot = std::clamp(static_cast<int>(action.value.x) - 1, -1, static_cast<int>(STAMP_NAMES.size()) - 1);
                break;
            case ScriptAction::Rotate:
                rotateBrush(static_cast<int>(action.value.x));
                break;
            case ScriptAction::Tilt:
                tiltBrush(static_cast<int>(action.value.x));
                break;
            case ScriptAction::Material:
                material = static_cast<CellKind>(static_cast<int>(action.value.x));
                break;
            case ScriptAction::Push:
                moveWithCollision(action.value);
                std::cout << "push: feet at " << eye.x << " " << eye.y - EYE_HEIGHT << " " << eye.z << std::endl;
                break;
            case ScriptAction::Resize:
                glfwSetWindowSize(window, static_cast<int>(action.value.x), static_cast<int>(action.value.y));
                break;
            case ScriptAction::Place:
            case ScriptAction::Break: {
                updateTarget();
                uint64_t before = population;
                if (action.kind == ScriptAction::Place && hotbarSlot >= 0 && target.canPlace) {
                    glm::ivec3 lo(std::numeric_limits<int>::max()), hi(std::numeric_limits<int>::lowest());
                    for (const glm::ivec3& c : stampCells(static_cast<Stamp>(hotbarSlot), target.place, target.normal, true)) {
                        lo = glm::min(lo, c);
                        hi = glm::max(hi, c);
                    }
                    std::cout << "stamp bounds (" << lo.x << "," << lo.y << "," << lo.z << ")-(" << hi.x << "," << hi.y
                              << "," << hi.z << ") rotation " << brushRotation * 90 << " tilt " << brushTilt * 90
                              << std::endl;
                }
                if (action.kind == ScriptAction::Place) placeStamp(); else breakBlock();
                if (refreshPending) rebuild(Rebuild::Edit);
                std::cout << (action.kind == ScriptAction::Place ? "place " : "break ") << handName()
                          << (target.hit ? " at block face" : " in air") << ": population " << before << " -> "
                          << population << std::endl;
                break;
            }
        }
    }

    // ------------------------------------------------------------------ input

    void setCursorCaptured(bool captured) {
        cursorCaptured = captured;
        haveCursorPosition = false;
        glfwSetInputMode(window, GLFW_CURSOR, captured ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
        if (!captured) breakHeld = placeHeld = false;
    }

    void toggleFullscreen() {
        fullscreen = !fullscreen;
        if (fullscreen) {
            glfwGetWindowPos(window, &windowedX, &windowedY);
            glfwGetWindowSize(window, &windowedWidth, &windowedHeight);
            GLFWmonitor* monitor = glfwGetPrimaryMonitor();
            const GLFWvidmode* mode = glfwGetVideoMode(monitor);
            glfwSetWindowMonitor(window, monitor, 0, 0, mode->width, mode->height, mode->refreshRate);
        } else {
            glfwSetWindowMonitor(window, nullptr, windowedX, windowedY, windowedWidth, windowedHeight, 0);
        }
        framebufferResized = true;
    }

    const char* handName() const { return hotbarSlot < 0 ? "Empty hand" : STAMP_NAMES[hotbarSlot]; }

    std::string handLabel() const {
        std::string label = handName();
        if (hotbarSlot >= 0 && material != CellKind::Life) label += std::string(" of ") + cellType(material).name;
        if (hotbarSlot >= 0 && brushRotation != 0) label += " (rotated " + std::to_string(brushRotation * 90) + " deg)";
        if (hotbarSlot >= 0 && brushTilt != 0) label += " (tilted " + std::to_string(brushTilt * 90) + " deg)";
        return label;
    }

    void rotateBrush(int quarterTurns) {
        brushRotation = ((brushRotation + quarterTurns) % 4 + 4) % 4;
        slotNameUntil = glfwGetTime() + 2.0;
    }

    void tiltBrush(int quarterTurns) {
        brushTilt = ((brushTilt + quarterTurns) % 4 + 4) % 4;
        slotNameUntil = glfwGetTime() + 2.0;
    }

    void notify(const std::string& message) {
        std::cout << message << std::endl;
        toast = message;
        toastUntil = glfwGetTime() + 4.0;
    }

    void selectSlot(int slot) {
        hotbarSlot = slot;
        slotNameUntil = glfwGetTime() + 2.0;
    }

    void openPauseMenu() {
        if (screen != Screen::Playing) return;
        runningBeforePause = simulationRunning;
        simulationRunning = false;
        screen = Screen::Paused;
        setCursorCaptured(false);
    }

    void resumeGame() {
        screen = Screen::Playing;
        simulationRunning = runningBeforePause;
        stepDebt = 0.0;
        setCursorCaptured(true);
    }

    void openInventory() {
        screen = Screen::Inventory;
        setCursorCaptured(false);
    }

    void openNewWorldScreen() {
        newWorldRule = static_cast<int>(ruleIndex);
        newWorldSeed = static_cast<int>(rng() & 0x7FFFFFFF);
        newWorldEmpty = false;
        screen = Screen::NewWorld;
    }

    bool worldFrozen() const {
        return screen == Screen::Paused || screen == Screen::Settings || screen == Screen::NewWorld;
    }

    void onKey(int key, int action, int mods) {
        const bool ctrl = mods & GLFW_MOD_CONTROL, shift = mods & GLFW_MOD_SHIFT;
        if (action == GLFW_PRESS && ctrl && key == GLFW_KEY_Q) {
            glfwSetWindowShouldClose(window, GLFW_TRUE);
            return;
        }
        if (screen != Screen::Playing) {
            if (action != GLFW_PRESS) return;
            if (key == GLFW_KEY_ESCAPE || (key == GLFW_KEY_TAB && screen == Screen::Inventory)) {
                switch (screen) {
                    case Screen::Paused: resumeGame(); break;
                    case Screen::Inventory: screen = Screen::Playing; setCursorCaptured(true); break;
                    case Screen::Settings:
                        if (persistSettings) saveSettings(settings);
                        screen = Screen::Paused;
                        break;
                    case Screen::NewWorld: screen = Screen::Paused; break;
                    case Screen::Playing: break;
                }
            }
            return;
        }
        // F3 alone toggles the debug overlay on release; F3+G shows chunk borders.
        if (key == GLFW_KEY_F3) {
            if (action == GLFW_PRESS) f3UsedInCombo = false;
            if (action == GLFW_RELEASE && !f3UsedInCombo) showDebug = !showDebug;
            return;
        }
        if (action == GLFW_REPEAT && key == GLFW_KEY_N && !ctrl) {
            stepOnce();
            return;
        }
        if (action != GLFW_PRESS) return;
        if (tutorial::Tutorial::Request request = tutorialPanel.handleKey(key); request != tutorial::Tutorial::Request::None) {
            handleTutorialRequest(request);
            return;
        }
        if (key >= GLFW_KEY_1 && key < GLFW_KEY_1 + static_cast<int>(STAMP_NAMES.size())) {
            int slot = key - GLFW_KEY_1;
            selectSlot(slot == hotbarSlot ? -1 : slot); // pressing the selected number again empties the hand
            return;
        }
        switch (key) {
            case GLFW_KEY_ESCAPE: openPauseMenu(); break;
            case GLFW_KEY_TAB: openInventory(); break;
            case GLFW_KEY_Q: rotateBrush(-1); break; // Ctrl+Q (quit) is handled above
            case GLFW_KEY_E: rotateBrush(1); break;
            case GLFW_KEY_Z: tiltBrush(-1); break;
            case GLFW_KEY_C: tiltBrush(1); break;
            case GLFW_KEY_SPACE: {
                double now = glfwGetTime();
                if (now - lastSpacePress < DOUBLE_TAP_SECONDS) {
                    flying = !flying;
                    verticalSpeed = 0.0f;
                    lastSpacePress = -1.0;
                } else {
                    lastSpacePress = now;
                }
                break;
            }
            case GLFW_KEY_S:
                if (ctrl) saveWorldWithMessage();
                break;
            case GLFW_KEY_O:
                if (ctrl) loadWorldWithMessage();
                break;
            case GLFW_KEY_G:
                if (glfwGetKey(window, GLFW_KEY_F3) == GLFW_PRESS) {
                    showChunkBorders = !showChunkBorders; // Minecraft's F3+G
                    f3UsedInCombo = true;
                } else {
                    simulationRunning = !simulationRunning;
                    stepDebt = 0.0;
                }
                break;
            case GLFW_KEY_N:
                if (ctrl) {
                    newWorld(!shift);
                    notify(shift ? "New empty world" : std::string("New world: ") + rule().name);
                } else {
                    stepOnce();
                }
                break;
            case GLFW_KEY_J: queueFastForward(shift ? 1000 : 100); break;
            case GLFW_KEY_M: cycleMaterial(shift ? -1 : 1); break;
            case GLFW_KEY_R: {
                size_t count = lifeRules().size();
                ruleIndex = shift ? (ruleIndex + count - 1) % count : (ruleIndex + 1) % count;
                notify(std::string("Rule: ") + rule().name + " " + describeRule(rule()));
                break;
            }
            // Speed doubles or halves per press; Shift makes it 8x.
            case GLFW_KEY_EQUAL:
            case GLFW_KEY_KP_ADD:
            case GLFW_KEY_RIGHT_BRACKET:
                changeSpeed(shift ? 3 : 1);
                break;
            case GLFW_KEY_MINUS:
            case GLFW_KEY_KP_SUBTRACT:
            case GLFW_KEY_LEFT_BRACKET:
                changeSpeed(shift ? -3 : -1);
                break;
            case GLFW_KEY_F1: hudVisible = !hudVisible; break;
            case GLFW_KEY_F2: requestScreenshot(timestampedScreenshotName()); break;
            case GLFW_KEY_F11: toggleFullscreen(); break;
            case GLFW_KEY_H: printControls(); break;
            default: break;
        }
    }

    // One generation (N), animated at the speed of a slow tick.
    void stepOnce() {
        if (refreshPending) rebuild(Rebuild::Edit); // see updateSimulation
        bool animate = animations();
        runBatch(1, animate);
        lastChangeTime = glfwGetTime();
        changeAnimationSeconds = animate ? 0.22f : 0.0f;
    }

    void cycleMaterial(int direction) {
        int count = static_cast<int>(cellTypes().size());
        material = static_cast<CellKind>(((static_cast<int>(material) + direction) % count + count) % count);
        notify(std::string("Building with ") + cellType(material).name);
        slotNameUntil = glfwGetTime() + 2.0;
    }

    void saveWorldWithMessage() {
        if (saveWorld(saveFilePath())) notify("Saved world to " + saveFilePath());
        else notify("Could not save the world");
    }

    void loadWorldWithMessage() {
        if (loadWorld(saveFilePath())) notify("Loaded " + saveFilePath());
        else notify("Could not load " + saveFilePath());
    }

    // Left click places the selected stamp, right click removes the targeted block.
    // Both repeat while held. (The reverse of Minecraft, by request.)
    void onMouseButton(int button, int action, int) {
        if (screen != Screen::Playing) return; // menus handle their own clicks
        if (!cursorCaptured) {
            // The first click only grabs the mouse, unless it lands on the tutorial panel.
            if (action == GLFW_PRESS && !(imguiReady && ImGui::GetIO().WantCaptureMouse)) setCursorCaptured(true);
            return;
        }
        if (button == GLFW_MOUSE_BUTTON_LEFT) {
            placeHeld = action == GLFW_PRESS;
            placeTimer = 0.0f;
            if (placeHeld) placeStamp();
        } else if (button == GLFW_MOUSE_BUTTON_RIGHT) {
            breakHeld = action == GLFW_PRESS;
            breakTimer = 0.0f;
            if (breakHeld) breakBlock();
        }
    }

    void onCursorMove(double x, double y) {
        if (screen == Screen::Playing && cursorCaptured && haveCursorPosition) {
            float degrees = MOUSE_DEGREES_PER_PIXEL * static_cast<float>(settings.sensitivity) / 100.0f;
            float dy = static_cast<float>(y - lastCursorY) * degrees * (settings.invertY ? -1.0f : 1.0f);
            yaw += static_cast<float>(x - lastCursorX) * degrees;
            pitch = std::clamp(pitch - dy, -89.9f, 89.9f);
        }
        lastCursorX = x;
        lastCursorY = y;
        haveCursorPosition = true;
    }

    void onScroll(double yoffset) {
        if (screen != Screen::Playing || yoffset == 0.0) return;
        // Like Minecraft: scrolling down selects the next slot. From an empty hand,
        // scrolling picks the first or last slot.
        int count = static_cast<int>(STAMP_NAMES.size());
        int next = hotbarSlot < 0 ? (yoffset < 0 ? 0 : count - 1) : hotbarSlot + (yoffset < 0 ? 1 : -1);
        selectSlot((next % count + count) % count);
    }

    void updateHeldButtons(float deltaTime) {
        if (breakHeld && (breakTimer += deltaTime) >= BREAK_REPEAT) {
            breakTimer = 0.0f;
            breakBlock();
        }
        if (placeHeld && (placeTimer += deltaTime) >= PLACE_REPEAT) {
            placeTimer = 0.0f;
            placeStamp();
        }
    }

    void printControls() const {
        std::cout << "\n3D Game of Life - Minecraft-style controls\n"
                     "  Mouse  look (click the window to grab the mouse)   Esc  pause menu and settings\n"
                     "  W A S D  move   Space  jump (fly up)   Left Shift  fly down   Left Ctrl  sprint\n"
                     "  Double-tap Space  toggle flying\n"
                     "  Left click  place the selected stamp   Right click  remove the outlined block\n"
                     "  Tab  stamps and rules   Q / E  rotate the stamp   Z / C  tilt it around x   1-" << STAMP_NAMES.size()
                  << " / scroll  hotbar (press the selected number again for an empty hand):\n ";
        for (size_t i = 0; i < STAMP_NAMES.size(); ++i) std::cout << "  " << (i + 1) << " " << STAMP_NAMES[i];
        std::cout << "\n"
                     "  M / Shift+M  building material: Life, Stone (inert wall), Ember (permanent live neighbor)\n"
                     "  G  run/pause generations   N  single generation\n"
                     "  [ ]  half / double speed (1/32 to 8192 gen/s, then max; Shift: 8x)\n"
                     "  J / Shift+J  fast-forward 100 / 1000 generations (J again cancels)\n"
                     "  R / Shift+R  next/previous rule   Ctrl+N  new world   Ctrl+Shift+N  empty world\n"
                     "  Ctrl+S  save the world   Ctrl+O  load it (user data folder)\n"
                     "  F1  hide HUD   F2  screenshot   F3  debug info   F3+G  chunk borders   F11  fullscreen\n"
                     "  H  help   Ctrl+Q  quit\n"
                     "Rules:\n";
        for (size_t i = 0; i < lifeRules().size(); ++i) {
            std::cout << "  " << lifeRules()[i].name << "  " << describeRule(lifeRules()[i]) << "\n";
        }
        std::cout << std::endl;
    }

    // ------------------------------------------------------------------- menus

    void initImGui() {
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.IniFilename = nullptr;
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        applyMenuStyle();
        baseStyle = ImGui::GetStyle();
        // Installed after the game's GLFW callbacks, which ImGui then chains to.
        ImGui_ImplGlfw_InitForVulkan(window, true);

        VkInstance instance = gpu.instance;
        ImGui_ImplVulkan_LoadFunctions(VK_API_VERSION_1_3, [](const char* name, void* user) {
            return vkGetInstanceProcAddr(*static_cast<VkInstance*>(user), name);
        }, &instance);
        ImGui_ImplVulkan_InitInfo info{};
        info.ApiVersion = VK_API_VERSION_1_3;
        info.Instance = gpu.instance;
        info.PhysicalDevice = gpu.physicalDevice;
        info.Device = device;
        info.QueueFamily = gpu.queueFamily;
        info.Queue = gpu.queue;
        info.DescriptorPoolSize = IMGUI_IMPL_VULKAN_MINIMUM_SAMPLED_IMAGE_POOL_SIZE + IMGUI_IMPL_VULKAN_MINIMUM_SAMPLER_POOL_SIZE + 4;
        info.MinImageCount = 2;
        info.ImageCount = std::max<uint32_t>(2, static_cast<uint32_t>(swapchainImages.size()));
        info.UseDynamicRendering = true;
        info.PipelineInfoMain.PipelineRenderingCreateInfo = renderingInfo();
        if (!ImGui_ImplVulkan_Init(&info)) throw std::runtime_error("Failed to initialize the menu renderer!");
        imguiReady = true;
    }

    // The swapchain is sRGB, so the GPU applies gamma on write; menu colors are
    // authored in sRGB and converted to linear here.
    static float toLinear(float c) { return std::pow(c, 2.2f); }
    static ImU32 color(int r, int g, int b, int a = 255) {
        auto channel = [](int v) { return static_cast<int>(std::lround(255.0f * toLinear(static_cast<float>(v) / 255.0f))); };
        return IM_COL32(channel(r), channel(g), channel(b), a);
    }
    static ImU32 accentColor(int a = 255) { return color(94, 230, 168, a); }

    // 3D Life's own menu look: dark translucent cards with rounded corners, a mint
    // accent (the color of live cells in the docs), and the Karla font.
    static void applyMenuStyle() {
        ImGuiStyle& style = ImGui::GetStyle();
        ImGui::StyleColorsDark(&style);
        style.WindowRounding = 12.0f;
        style.ChildRounding = style.PopupRounding = 8.0f;
        style.FrameRounding = style.GrabRounding = style.ScrollbarRounding = style.TabRounding = 6.0f;
        style.WindowBorderSize = style.ChildBorderSize = style.PopupBorderSize = 1.0f;
        style.FrameBorderSize = 0.0f;
        style.WindowPadding = ImVec2(18, 16);
        style.FramePadding = ImVec2(10, 5);
        style.ItemSpacing = ImVec2(8, 6);
        style.ItemInnerSpacing = ImVec2(6, 4);
        style.GrabMinSize = 10.0f;
        style.ScrollbarSize = 10.0f;
        auto rgb = [](int r, int g, int b, float a = 1.0f) { return ImVec4(r / 255.0f, g / 255.0f, b / 255.0f, a); };
        const ImVec4 accent = rgb(94, 230, 168);
        ImVec4* c = style.Colors;
        c[ImGuiCol_Text] = rgb(232, 237, 245);
        c[ImGuiCol_TextDisabled] = rgb(138, 149, 170);
        c[ImGuiCol_WindowBg] = rgb(14, 18, 28, 0.94f);
        c[ImGuiCol_ChildBg] = rgb(255, 255, 255, 0.03f);
        c[ImGuiCol_PopupBg] = rgb(20, 26, 40, 0.98f);
        c[ImGuiCol_Border] = rgb(120, 140, 180, 0.22f);
        c[ImGuiCol_BorderShadow] = rgb(0, 0, 0, 0.0f);
        c[ImGuiCol_Button] = rgb(36, 44, 62);
        c[ImGuiCol_ButtonHovered] = rgb(50, 62, 88);
        c[ImGuiCol_ButtonActive] = rgb(62, 78, 110);
        c[ImGuiCol_FrameBg] = rgb(40, 49, 70);
        c[ImGuiCol_FrameBgHovered] = rgb(52, 63, 90);
        c[ImGuiCol_FrameBgActive] = rgb(62, 76, 108);
        c[ImGuiCol_SliderGrab] = accent;
        c[ImGuiCol_SliderGrabActive] = rgb(150, 245, 200);
        c[ImGuiCol_CheckMark] = accent;
        c[ImGuiCol_Header] = rgb(94, 230, 168, 0.18f);
        c[ImGuiCol_HeaderHovered] = rgb(94, 230, 168, 0.28f);
        c[ImGuiCol_HeaderActive] = rgb(94, 230, 168, 0.40f);
        c[ImGuiCol_Separator] = rgb(120, 140, 180, 0.22f);
        c[ImGuiCol_ScrollbarBg] = rgb(0, 0, 0, 0.0f);
        c[ImGuiCol_ScrollbarGrab] = rgb(60, 72, 100);
        c[ImGuiCol_ScrollbarGrabHovered] = rgb(76, 90, 124);
        c[ImGuiCol_ScrollbarGrabActive] = rgb(90, 106, 144);
        c[ImGuiCol_TextSelectedBg] = rgb(94, 230, 168, 0.35f);
        c[ImGuiCol_NavCursor] = accent;
        for (int i = 0; i < ImGuiCol_COUNT; ++i) {
            c[i] = ImVec4(toLinear(c[i].x), toLinear(c[i].y), toLinear(c[i].z), c[i].w);
        }
    }

    // Auto scales with the window height (1x at 720 lines), in quarter steps;
    // fonts are rasterized at the final size, so any scale stays sharp.
    float effectiveUiScale() const {
        if (settings.guiScale > 0) return static_cast<float>(settings.guiScale);
        float scale = std::round(4.0f * static_cast<float>(swapchainExtent.height) / 720.0f) / 4.0f;
        return std::clamp(scale, 1.0f, 4.0f);
    }

    // Applies the GUI scale. Dear ImGui bakes glyphs at whatever size is drawn,
    // so text stays crisp at every scale without rebuilding the font atlas.
    void updateUiScale() {
        float scale = effectiveUiScale();
        if (scale == uiScale) return;
        if (uiScale == 0.0f) {
            ImFontConfig font;
            font.FontDataOwnedByAtlas = false; // the TTF lives in the executable (MenuFont.h)
            void* ttf = const_cast<unsigned char*>(MENU_FONT_TTF);
            titleFont = ImGui::GetIO().Fonts->AddFontFromMemoryTTF(ttf, MENU_FONT_TTF_SIZE, BODY_FONT_SIZE, &font);
        }
        ImGuiStyle& style = ImGui::GetStyle();
        style = baseStyle;
        style.ScaleAllSizes(scale);
        style.FontSizeBase = BODY_FONT_SIZE;
        style.FontScaleMain = scale;
        uiScale = scale;
    }

    static constexpr float BODY_FONT_SIZE = 15.0f;
    static constexpr float TITLE_FONT_SIZE = 22.0f;

    float px(float value) const { return value * uiScale; }

    void buildUi() {
        if (!imguiReady) return;
        updateUiScale();
        ImGuiIO& io = ImGui::GetIO();
        if (screen == Screen::Playing && (cursorCaptured || !tutorialPanel.active())) io.ConfigFlags |= ImGuiConfigFlags_NoMouse;
        else io.ConfigFlags &= ~ImGuiConfigFlags_NoMouse;
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        if (screen != shownScreen) {
            shownScreen = screen;
            menuOpenedAt = glfwGetTime();
        }
        // Menus fade in over a short moment instead of popping up.
        float fade = capturing ? 1.0f : static_cast<float>(std::clamp((glfwGetTime() - menuOpenedAt) / 0.14, 0.0, 1.0));
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, screen == Screen::Playing ? 1.0f : fade * (2.0f - fade));
        switch (screen) {
            case Screen::Playing:
                drawHudOverlay();
                if (hudVisible && tutorialPanel.active()) handleTutorialRequest(tutorialPanel.draw(uiScale, tutorialStatus()));
                break;
            case Screen::Paused: drawPauseMenu(); break;
            case Screen::Settings: drawSettingsMenu(); break;
            case Screen::Inventory: drawInventory(); break;
            case Screen::NewWorld: drawNewWorldMenu(); break;
        }
        ImGui::PopStyleVar();
        if (updater && !updateAnnounced && updater->state() == gol3d::Updater::State::Available) {
            updateAnnounced = true;
            notify("Update available: 3D Life " + updater->release().version + ". Press Esc for details.");
        }
        drawToast();
        ImGui::Render();
    }

    // Text with a soft shadow so it reads over bright sky and blocks.
    void shadowText(ImDrawList* draw, ImVec2 pos, const std::string& text, ImU32 textColor = IM_COL32(255, 255, 255, 255)) {
        ImU32 shadow = color(0, 0, 0, ((textColor >> IM_COL32_A_SHIFT) & 0xFF) * 3 / 5);
        draw->AddText(ImVec2(pos.x + px(1), pos.y + px(1)), shadow, text.c_str());
        draw->AddText(pos, textColor, text.c_str());
    }

    static std::string withCommas(uint64_t value) {
        std::string digits = std::to_string(value), out;
        for (size_t i = 0; i < digits.size(); ++i) {
            if (i && (digits.size() - i) % 3 == 0) out += ',';
            out += digits[i];
        }
        return out;
    }

    static ImU32 materialColor(CellKind kind, int alpha = 255) {
        const uint8_t* rgb = cellType(kind).rgb;
        return color(rgb[0], rgb[1], rgb[2], alpha);
    }

    // Top-left status panel: run state and speed (with what the governor actually
    // reaches), population with its recent history, the building material, a
    // fast-forward's progress, and the F3 details.
    void drawHudOverlay() {
        if (!hudVisible) return;
        ImDrawList* draw = ImGui::GetForegroundDrawList();
        ImVec2 size = ImGui::GetIO().DisplaySize;
        const float line = ImGui::GetTextLineHeight();
        const ImU32 white = IM_COL32(255, 255, 255, 255), muted = color(170, 182, 200), amber = color(255, 196, 90);

        struct Row {
            std::string text;
            ImU32 color;
        };
        std::vector<Row> rows;
        std::string title = std::string(rule().name) + "   gen " + withCommas(generation);
        std::string speed;
        ImU32 speedColor = muted;
        if (fastForward > 0) {
            speed = "fast-forward  " + withCommas(fastForwardTotal - fastForward) + " / " + withCommas(fastForwardTotal);
            speedColor = accentColor();
        } else if (!simulationRunning) {
            speed = "paused  (" + speedLabel() + ", G to run)";
        } else if (unlimitedSpeed()) {
            speed = "max speed  " + formatRate(measuredRate) + " gen/s";
            speedColor = accentColor();
        } else if (tickLimited) {
            speed = speedLabel() + "  ->  " + formatRate(measuredRate) + " gen/s (slowed to keep up)";
            speedColor = amber;
        } else {
            speed = speedLabel();
            speedColor = accentColor();
        }
        rows.push_back({speed, speedColor});
        std::string material = std::string("building with ") + cellType(this->material).name;
        rows.push_back({withCommas(population) + " alive", white});
        if (showDebug) {
            std::ostringstream a, b, c, d, e, f;
            a << std::fixed << std::setprecision(3) << "XYZ: " << eye.x << " / " << eye.y - EYE_HEIGHT << " / " << eye.z;
            glm::ivec3 chunk = chunkOf(glm::ivec3(glm::floor(eye)));
            b << "Chunk: " << chunk.x << " " << chunk.y << " " << chunk.z << "  |  " << (flying ? "flying" : "walking")
              << (onGround ? ", on ground" : "");
            c << std::fixed << std::setprecision(1) << "Facing: yaw " << yaw << ", pitch " << pitch;
            d << withCommas(activeSlots.size()) << " chunks of " << withCommas(pool.capacity) << " (limit "
              << withCommas(chunkLimit) << ")" << (chunkLimitHit ? " LIMIT" : "") << "  |  GPU " << (gpuBytes >> 20) << " MB";
            e << withCommas(drawnBlocks) << " blocks drawn" << (visibleBlocks > drawnBlocks ? " (capped)" : "");
            f << std::fixed << std::setprecision(2) << "GPU " << stepCostMs << " ms/gen, " << drawCostMs
              << " ms/block list  |  sim " << std::setprecision(1) << lastSimMs << " ms/frame  |  " << std::lround(fps) << " fps";
            for (const std::string& text : {describeRule(rule()), a.str(), b.str(), c.str(), d.str(), e.str(), f.str()})
                rows.push_back({text, color(200, 210, 225)});
            if (target.hit) {
                int kind = cellKind(target.block);
                rows.push_back({"Targeted: " + std::string(kind >= 0 ? cellTypes()[kind].name : "?") + " at " +
                                    std::to_string(target.block.x) + " " + std::to_string(target.block.y) + " " +
                                    std::to_string(target.block.z),
                                color(200, 210, 225)});
            }
        }

        // Panel geometry: title, rows, material line, then the population graph.
        const float icon = line;
        float width = ImGui::CalcTextSize(title.c_str()).x + icon + px(6);
        for (const Row& row : rows) width = std::max(width, ImGui::CalcTextSize(row.text.c_str()).x);
        width = std::max(width, ImGui::CalcTextSize(material.c_str()).x + line);
        width = std::max(width, px(200));
        const float graphHeight = line * 1.6f;
        const bool graph = populationHistory.size() >= 2;
        ImVec2 min(px(8), px(8));
        float height = line * static_cast<float>(rows.size() + 2) + px(12) + (graph ? graphHeight + px(6) : 0.0f) +
                       (fastForward > 0 ? px(8) : 0.0f);
        ImVec2 max(min.x + width + px(22), min.y + height);
        draw->AddRectFilled(min, max, color(14, 18, 28, 180), px(8));
        draw->AddRect(min, max, color(120, 140, 180, 50), px(8), px(1));
        draw->AddRectFilled(min, ImVec2(min.x + px(3), max.y), speedColor, px(8), ImDrawFlags_RoundCornersLeft);

        // Run state icon (play triangle or pause bars) then the title.
        float x = min.x + px(12), y = min.y + px(6);
        if (simulationRunning || fastForward > 0) {
            draw->AddTriangleFilled(ImVec2(x, y + icon * 0.15f), ImVec2(x, y + icon * 0.85f), ImVec2(x + icon * 0.62f, y + icon * 0.5f), speedColor);
        } else {
            draw->AddRectFilled(ImVec2(x, y + icon * 0.18f), ImVec2(x + icon * 0.22f, y + icon * 0.82f), muted, px(1));
            draw->AddRectFilled(ImVec2(x + icon * 0.4f, y + icon * 0.18f), ImVec2(x + icon * 0.62f, y + icon * 0.82f), muted, px(1));
        }
        draw->AddText(ImVec2(x + icon * 0.62f + px(6), y), white, title.c_str());
        y += line;
        for (const Row& row : rows) {
            draw->AddText(ImVec2(x, y), row.color, row.text.c_str());
            y += line;
        }
        // Material swatch.
        float swatch = line * 0.62f;
        ImVec2 swatchMin(x, y + (line - swatch) * 0.5f);
        draw->AddRectFilled(swatchMin, ImVec2(swatchMin.x + swatch, swatchMin.y + swatch), materialColor(this->material), px(2));
        draw->AddText(ImVec2(x + swatch + px(6), y), muted, material.c_str());
        y += line;
        if (fastForward > 0 && fastForwardTotal > 0) {
            float done = static_cast<float>(fastForwardTotal - fastForward) / static_cast<float>(fastForwardTotal);
            ImVec2 barMin(x, y + px(2)), barMax(min.x + width + px(10), y + px(6));
            draw->AddRectFilled(barMin, barMax, color(40, 49, 70), px(2));
            draw->AddRectFilled(barMin, ImVec2(barMin.x + (barMax.x - barMin.x) * done, barMax.y), accentColor(), px(2));
            y += px(8);
        }
        if (graph) drawPopulationGraph(draw, ImVec2(x, y + px(4)), ImVec2(min.x + width + px(10), y + px(4) + graphHeight));

        // Selected stamp name above the hotbar, fading out (hotbar metrics match life3d_screen.frag).
        double remaining = slotNameUntil - glfwGetTime();
        if (remaining > 0.0 && hotbarSlot >= 0) {
            float hotbarScale = std::max(1.0f, std::floor(size.y / 540.0f));
            float slot = 40.0f * hotbarScale;
            std::string name = handLabel();
            ImVec2 textSize = ImGui::CalcTextSize(name.c_str());
            int alpha = static_cast<int>(255.0 * std::min(1.0, remaining / 0.5));
            shadowText(draw, ImVec2((size.x - textSize.x) * 0.5f, size.y - slot - 16.0f * hotbarScale - textSize.y),
                       name, IM_COL32(255, 255, 255, alpha));
        }
    }

    // Population over the last steps as a filled sparkline, scaled to its peak.
    void drawPopulationGraph(ImDrawList* draw, ImVec2 min, ImVec2 max) {
        float peak = 1.0f;
        for (float v : populationHistory) peak = std::max(peak, v);
        const size_t n = populationHistory.size();
        auto point = [&](size_t i) {
            float t = static_cast<float>(i) / static_cast<float>(n - 1);
            return ImVec2(min.x + t * (max.x - min.x), max.y - (max.y - min.y) * populationHistory[i] / peak);
        };
        draw->AddLine(ImVec2(min.x, max.y), max, color(120, 140, 180, 60), px(1));
        for (size_t i = 0; i + 1 < n; ++i) {
            ImVec2 a = point(i), b = point(i + 1);
            draw->AddQuadFilled(ImVec2(a.x, max.y), a, b, ImVec2(b.x, max.y), accentColor(46));
        }
        std::vector<ImVec2> points(n);
        for (size_t i = 0; i < n; ++i) points[i] = point(i);
        draw->AddPolyline(points.data(), static_cast<int>(n), accentColor(220), px(1.5f));
        std::string label = "peak " + withCommas(static_cast<uint64_t>(peak));
        ImVec2 textSize = ImGui::CalcTextSize(label.c_str());
        draw->AddText(ImGui::GetFont(), ImGui::GetFontSize() * 0.8f, ImVec2(max.x - textSize.x * 0.8f, min.y - px(2)),
                      color(170, 182, 200, 200), label.c_str());
    }

    // A rounded message pill near the top of the screen.
    void drawToast() {
        double remaining = toastUntil - glfwGetTime();
        if (remaining <= 0.0 || toast.empty()) return;
        ImDrawList* draw = ImGui::GetForegroundDrawList();
        ImVec2 size = ImGui::GetIO().DisplaySize;
        ImVec2 textSize = ImGui::CalcTextSize(toast.c_str());
        int alpha = static_cast<int>(255.0 * std::min(1.0, remaining / 0.5));
        ImVec2 pos((size.x - textSize.x) * 0.5f, size.y * 0.1f);
        ImVec2 min(pos.x - px(14), pos.y - px(6)), max(pos.x + textSize.x + px(14), pos.y + textSize.y + px(6));
        float radius = (max.y - min.y) * 0.5f;
        draw->AddRectFilled(min, max, color(14, 18, 28, alpha * 9 / 10), radius);
        draw->AddRect(min, max, accentColor(alpha * 2 / 3), radius, px(1));
        draw->AddText(pos, IM_COL32(255, 255, 255, alpha), toast.c_str());
    }

    // ------------------------------------------------------- menu building blocks

    // Dims the world, then opens a card of the given width centered on screen.
    // Always pair with ImGui::End().
    bool beginCard(const char* id, float width) {
        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->Pos);
        ImGui::SetNextWindowSize(viewport->Size);
        ImGui::SetNextWindowBgAlpha(0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::Begin((std::string(id) + "-backdrop").c_str(), nullptr,
                     ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
                         ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNav);
        // Dim the world, darker toward the top and bottom edges, so the card stands out.
        ImDrawList* draw = ImGui::GetWindowDrawList();
        ImVec2 lo = viewport->Pos, hi(viewport->Pos.x + viewport->Size.x, viewport->Pos.y + viewport->Size.y);
        float band = viewport->Size.y * 0.35f;
        ImU32 dim = color(8, 10, 18, 120), dark = color(8, 10, 18, 200);
        draw->AddRectFilled(lo, hi, dim);
        draw->AddRectFilledMultiColor(lo, ImVec2(hi.x, lo.y + band), dark, dark, color(8, 10, 18, 0), color(8, 10, 18, 0));
        draw->AddRectFilledMultiColor(ImVec2(lo.x, hi.y - band), hi, color(8, 10, 18, 0), color(8, 10, 18, 0), dark, dark);
        ImGui::End();
        ImGui::PopStyleVar(2);
        ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSizeConstraints(ImVec2(px(width), 0.0f), ImVec2(px(width), viewport->Size.y - px(24)));
        return ImGui::Begin(id, nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                                             ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize);
    }

    // The 3D Life mark (Conway's glider in the accent color), a title, and an
    // optional muted subtitle on the right.
    void cardHeader(const char* title, const char* subtitle = nullptr) {
        ImGui::PushFont(titleFont, TITLE_FONT_SIZE);
        float height = ImGui::GetFontSize();
        ImGui::PopFont();
        float cell = std::floor(height / 3.4f);
        ImVec2 at = ImGui::GetCursorScreenPos();
        float top = at.y + std::floor((height - 3.0f * cell) * 0.5f);
        static constexpr int GLIDER[5][2] = {{1, 0}, {2, 1}, {0, 2}, {1, 2}, {2, 2}};
        ImDrawList* draw = ImGui::GetWindowDrawList();
        for (const auto& [x, y] : GLIDER) {
            ImVec2 min(at.x + x * cell, top + y * cell);
            draw->AddRectFilled(ImVec2(min.x + px(1), min.y + px(1)), ImVec2(min.x + cell - px(1), min.y + cell - px(1)),
                                accentColor(), px(1.5f));
        }
        ImGui::Dummy(ImVec2(3.0f * cell + px(6), height));
        ImGui::SameLine();
        ImGui::PushFont(titleFont, TITLE_FONT_SIZE);
        ImGui::TextUnformatted(title);
        ImGui::PopFont();
        if (subtitle) {
            ImVec2 size = ImGui::CalcTextSize(subtitle);
            rightAlignNext(size.x);
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (height - size.y) * 0.5f);
            ImGui::TextDisabled("%s", subtitle);
        }
        ImGui::Dummy(ImVec2(0, px(2)));
        ImGui::Separator();
        ImGui::Dummy(ImVec2(0, px(2)));
    }

    // Places the next item on this line, flush with the right edge of the window.
    static void rightAlignNext(float width) {
        ImGui::SameLine();
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + std::max(0.0f, ImGui::GetContentRegionAvail().x - width));
    }

    // A small accent-colored heading inside a card.
    void sectionLabel(const char* text) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(accentColor()));
        ImGui::TextUnformatted(text);
        ImGui::PopStyleColor();
    }

    // Wrapped text in a given color (sRGB).
    void note(const std::string& text, ImU32 textColor) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(textColor));
        ImGui::PushTextWrapPos(0.0f);
        ImGui::TextUnformatted(text.c_str());
        ImGui::PopTextWrapPos();
        ImGui::PopStyleColor();
    }

    void mutedText(const std::string& text) { note(text, color(138, 149, 170)); }

    float buttonHeight() const { return ImGui::GetFrameHeight() + px(6); }

    void pushAccentButton() {
        ImGui::PushStyleColor(ImGuiCol_Button, ImGui::ColorConvertU32ToFloat4(accentColor()));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImGui::ColorConvertU32ToFloat4(color(130, 240, 192)));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImGui::ColorConvertU32ToFloat4(color(70, 200, 140)));
        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(color(10, 30, 22)));
    }

    enum class ButtonKind { Normal, Primary, Danger };

    // A full-width button with its label on the left and an optional key hint on
    // the right. Primary buttons are filled with the accent color.
    bool menuItem(const char* label, const char* hint = nullptr, ButtonKind kind = ButtonKind::Normal) {
        if (kind == ButtonKind::Primary) pushAccentButton();
        if (kind == ButtonKind::Danger) {
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImGui::ColorConvertU32ToFloat4(color(150, 56, 64)));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImGui::ColorConvertU32ToFloat4(color(180, 66, 74)));
        }
        ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(kind == ButtonKind::Primary ? 0.5f : 0.0f, 0.5f));
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(px(12), ImGui::GetStyle().FramePadding.y));
        ImVec2 at = ImGui::GetCursorScreenPos();
        float width = ImGui::GetContentRegionAvail().x;
        bool clicked = ImGui::Button(label, ImVec2(width, buttonHeight()));
        ImGui::PopStyleVar(2);
        if (kind == ButtonKind::Primary) ImGui::PopStyleColor(4);
        if (kind == ButtonKind::Danger) ImGui::PopStyleColor(2);
        if (hint) {
            ImVec2 size = ImGui::CalcTextSize(hint);
            ImU32 hintColor = kind == ButtonKind::Primary ? color(10, 30, 22, 170) : ImGui::GetColorU32(ImGuiCol_TextDisabled);
            ImGui::GetWindowDrawList()->AddText(ImVec2(at.x + width - size.x - px(12), at.y + (buttonHeight() - size.y) * 0.5f),
                                                hintColor, hint);
        }
        return clicked;
    }

    // Two buttons sharing a row; returns 1 or 2 for the one clicked.
    int buttonPair(const char* left, const char* right, ButtonKind rightKind = ButtonKind::Normal) {
        float half = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) * 0.5f;
        int clicked = 0;
        if (ImGui::Button(left, ImVec2(half, buttonHeight()))) clicked = 1;
        ImGui::SameLine();
        if (rightKind == ButtonKind::Primary) pushAccentButton();
        if (ImGui::Button(right, ImVec2(half, buttonHeight()))) clicked = 2;
        if (rightKind == ButtonKind::Primary) ImGui::PopStyleColor(4);
        return clicked;
    }

    // ------------------------------------------------------------------ screens

    // A row of small labeled values ("GENERATION 1,204") in rounded boxes.
    void statChips(std::initializer_list<std::pair<const char*, std::string>> chips) {
        ImDrawList* draw = ImGui::GetWindowDrawList();
        const float gap = px(6), count = static_cast<float>(chips.size());
        const float width = (ImGui::GetContentRegionAvail().x - gap * (count - 1.0f)) / count;
        const float label = ImGui::GetFontSize() * 0.72f, height = label + ImGui::GetFontSize() + px(14);
        ImVec2 at = ImGui::GetCursorScreenPos();
        for (const auto& [name, value] : chips) {
            draw->AddRectFilled(at, ImVec2(at.x + width, at.y + height), color(255, 255, 255, 10), px(6));
            draw->AddRect(at, ImVec2(at.x + width, at.y + height), color(120, 140, 180, 50), px(6), px(1));
            draw->AddText(ImGui::GetFont(), label, ImVec2(at.x + px(8), at.y + px(5)), color(138, 149, 170), name);
            draw->AddText(ImVec2(at.x + px(8), at.y + px(7) + label), IM_COL32(255, 255, 255, 255), value.c_str());
            at.x += width + gap;
        }
        ImGui::Dummy(ImVec2(0, height));
    }

    // Run/pause, slower/faster and fast-forward, for players who don't know the keys.
    void simulationControls(bool& running) {
        const float gap = ImGui::GetStyle().ItemSpacing.x;
        const float full = ImGui::GetContentRegionAvail().x, h = buttonHeight();
        const float small = h * 1.15f;
        if (running) pushAccentButton();
        bool toggle = ImGui::Button(running ? "Running" : "Paused", ImVec2(full * 0.34f, h));
        if (running) ImGui::PopStyleColor(4);
        if (toggle) {
            running = !running;
            stepDebt = 0.0;
        }
        hint("G in game");
        ImGui::SameLine();
        if (ImGui::Button("-##slower", ImVec2(small, h))) changeSpeed(-1);
        hint("Half as fast ([)");
        ImGui::SameLine();
        std::string label = unlimitedSpeed() ? std::string("max speed") : speedLabel();
        float labelWidth = full - full * 0.34f - 2.0f * small - 3.0f * gap;
        ImVec2 at = ImGui::GetCursorScreenPos();
        ImGui::Dummy(ImVec2(labelWidth, h));
        ImVec2 size = ImGui::CalcTextSize(label.c_str());
        ImGui::GetWindowDrawList()->AddText(ImVec2(at.x + (labelWidth - size.x) * 0.5f, at.y + (h - size.y) * 0.5f),
                                            IM_COL32(255, 255, 255, 255), label.c_str());
        ImGui::SameLine();
        if (ImGui::Button("+##faster", ImVec2(small, h))) changeSpeed(1);
        hint("Twice as fast (])");
        const float half = (full - gap) * 0.5f;
        bool forwarding = fastForward > 0;
        if (ImGui::Button(forwarding ? "Cancel fast-forward" : "Skip 100 generations", ImVec2(forwarding ? full : half, h))) {
            queueFastForward(100);
        }
        if (!forwarding) {
            hint("J in game. Runs as fast as the GPU allows, then returns to the set speed.");
            ImGui::SameLine();
            if (ImGui::Button("Skip 1,000", ImVec2(half, h))) queueFastForward(1000);
            hint("Shift+J in game");
        }
    }

    void drawPauseMenu() {
        if (beginCard("##pause", 340)) {
            cardHeader("3D Life", "Paused");
            statChips({{"RULE", rule().name}, {"GENERATION", withCommas(generation)}, {"ALIVE", withCommas(population)}});
            ImGui::Dummy(ImVec2(0, px(2)));
            if (menuItem("Resume", "Esc", ButtonKind::Primary)) resumeGame();
            ImGui::Dummy(ImVec2(0, px(2)));
            sectionLabel("Simulation");
            simulationControls(runningBeforePause);
            ImGui::Dummy(ImVec2(0, px(2)));
            ImGui::Separator();
            ImGui::Dummy(ImVec2(0, px(2)));
            if (menuItem("Stamps & Rules", "Tab")) screen = Screen::Inventory;
            if (menuItem("Tutorial")) openTutorial(tutorialPanel.lessonIndex());
            if (menuItem("New World...")) openNewWorldScreen();
            if (menuItem("Save World", "Ctrl+S")) saveWorldWithMessage();
            if (menuItem("Load World", "Ctrl+O")) loadWorldWithMessage();
            if (menuItem("Settings")) screen = Screen::Settings;
            ImGui::Dummy(ImVec2(0, px(2)));
            ImGui::Separator();
            ImGui::Dummy(ImVec2(0, px(2)));
            if (menuItem("Quit", "Ctrl+Q", ButtonKind::Danger)) glfwSetWindowShouldClose(window, GLFW_TRUE);
            drawUpdatePanel();
        }
        ImGui::End();
    }

    // Update status and actions at the bottom of the pause menu.
    void drawUpdatePanel() {
        if (!updater) return;
        using State = gol3d::Updater::State;
        State state = updater->state();
        if (state == State::Checking || state == State::Idle) return;
        gol3d::ReleaseInfo release = updater->release();
        ImGui::Dummy(ImVec2(0, px(2)));
        ImGui::Separator();
        ImGui::Dummy(ImVec2(0, px(2)));
        switch (state) {
            case State::Available: {
                note("Update available: 3D Life " + release.version + " (you have " + updater->currentVersion() + ")",
                     color(255, 236, 150));
                bool installs = updater->method() != gol3d::InstallMethod::OpenPage;
                if (installs && menuItem("Download and Install", nullptr, ButtonKind::Primary)) updater->installAsync();
                if (menuItem(installs ? "Release Notes" : "Open Download Page")) gol3d::openInBrowser(release.pageUrl);
                break;
            }
            case State::Downloading:
                note("Downloading and verifying 3D Life " + release.version + "...", color(255, 236, 150));
                break;
            case State::Ready:
                if (updater->method() == gol3d::InstallMethod::AppImage) {
                    note("Updated to " + release.version + ". Restart to use it.", accentColor());
                    if (menuItem("Restart Now", nullptr, ButtonKind::Primary)) {
                        restartPath = updater->appImagePath();
                        glfwSetWindowShouldClose(window, GLFW_TRUE);
                    }
                } else {
                    note("3D Life " + release.version + " is downloaded and verified.", accentColor());
                    if (menuItem("Install and Restart", nullptr, ButtonKind::Primary) && updater->launchInstaller()) {
                        glfwSetWindowShouldClose(window, GLFW_TRUE);
                    }
                }
                break;
            case State::Failed:
                note(updater->error(), color(255, 160, 150));
                if (menuItem("Open Download Page")) gol3d::openInBrowser(gol3d::RELEASES_PAGE);
                break;
            case State::UpToDate:
                mutedText("3D Life " + updater->currentVersion() + " is up to date.");
                break;
            case State::Checking:
            case State::Idle:
                break;
        }
    }

    // Settings rows: a label column and a control column that fills the rest.
    void optionSection(const char* name, bool first = false) {
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        if (!first) ImGui::Dummy(ImVec2(0, px(4)));
        sectionLabel(name);
    }

    void optionRow(const char* label) {
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(label);
        ImGui::TableSetColumnIndex(1);
        ImGui::SetNextItemWidth(-FLT_MIN);
    }

    void drawSettingsMenu() {
        if (beginCard("##settings", 440)) {
            cardHeader("Settings");
            if (ImGui::BeginTable("##options", 2)) {
                ImGui::TableSetupColumn("label", ImGuiTableColumnFlags_WidthFixed, px(150));
                ImGui::TableSetupColumn("control", ImGuiTableColumnFlags_WidthStretch);

                optionSection("View", true);
                optionRow("Field of view");
                ImGui::SliderFloat("##fov", &settings.fov, 30.0f, 110.0f, "%.0f");
                optionRow("Render distance");
                if (ImGui::SliderInt("##render", &settings.renderDistance, 64, 1024, "%d blocks")) refreshPending = true;
                optionRow("Fullscreen");
                bool wantFullscreen = fullscreen;
                if (ImGui::Checkbox("##fullscreen", &wantFullscreen)) toggleFullscreen();
                optionRow("Smooth lighting");
                ImGui::Checkbox("##ao", &settings.smoothLighting);
                hint("Shades block corners by the blocks around them.");
                optionRow("Animate changes");
                ImGui::Checkbox("##animate", &settings.animate);
                hint("At slow speeds, newborn cells grow in and dying ones shrink away.");

                optionSection("Mouse");
                optionRow("Sensitivity");
                ImGui::SliderInt("##sens", &settings.sensitivity, 10, 300, "%d%%");
                optionRow("Invert vertical");
                ImGui::Checkbox("##invert", &settings.invertY);

                optionSection("Interface");
                optionRow("GUI scale");
                ImGui::SliderInt("##gui", &settings.guiScale, 0, 4, settings.guiScale == 0 ? "Auto" : "%dx");
                optionRow("Show HUD");
                ImGui::Checkbox("##hud", &hudVisible);
                optionRow("Chunk borders");
                ImGui::Checkbox("##borders", &showChunkBorders);

                optionSection("Simulation");
                optionRow("Speed");
                std::string format = unlimitedSpeed() ? std::string("As fast as possible") : speedLabel();
                ImGui::SliderInt("##speed", &speedExponent, MIN_SPEED_EXPONENT, UNLIMITED_SPEED_EXPONENT, format.c_str());
                hint("[ and ] halve or double it in game; Shift jumps 8x.");
                optionRow("Time per frame");
                ImGui::SliderInt("##budget", &settings.simBudget, 2, 40, "%d ms");
                hint("Most time each frame may spend simulating. When a world needs more, the "
                     "simulation slows down instead of the frame rate. Fast-forward (J) and max "
                     "speed use at least 25 ms.");

                optionSection("Updates");
                optionRow("Check at startup");
                ImGui::Checkbox("##updates", &settings.checkUpdates);
                ImGui::SameLine();
                if (ImGui::Button("Check now")) {
                    if (!updater) updater = std::make_unique<gol3d::Updater>(GOL3D_VERSION_STRING, exeDir);
                    updateAnnounced = false;
                    updater->checkAsync();
                }
                ImGui::EndTable();
            }
            ImGui::Dummy(ImVec2(0, px(6)));
            switch (buttonPair("Reset to Defaults", "Done", ButtonKind::Primary)) {
                case 1: settings = Settings{}; break;
                case 2:
                    if (persistSettings) saveSettings(settings);
                    screen = Screen::Paused;
                    break;
            }
            if (persistSettings) mutedText("Saved to " + settingsPath().string());
        }
        ImGui::End();
    }

    // A tooltip on the control just drawn.
    static void hint(const char* text) {
        if (!ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) return;
        ImGui::SetNextWindowSize(ImVec2(ImGui::GetFontSize() * 18.0f, 0));
        ImGui::BeginTooltip();
        ImGui::TextWrapped("%s", text);
        ImGui::EndTooltip();
    }

    void drawStampIcon(ImDrawList* draw, ImVec2 min, float size, int stamp, ImU32 color) {
        float cell = std::floor(size * 0.14f);
        ImVec2 origin(min.x + std::floor((size - 5.0f * cell) * 0.5f), min.y + std::floor((size - 5.0f * cell) * 0.5f));
        for (int y = 0; y < 5; ++y)
            for (int x = 0; x < 5; ++x)
                if ((STAMP_ICONS[stamp] >> (24 - (y * 5 + x))) & 1u)
                    draw->AddRectFilled(ImVec2(origin.x + x * cell, origin.y + y * cell),
                                        ImVec2(origin.x + (x + 1) * cell, origin.y + (y + 1) * cell), color);
    }

    // An isometric cube: lit top, mid left face, dark right face.
    static void drawCubeIcon(ImDrawList* draw, ImVec2 center, float size, CellKind kind) {
        const uint8_t* rgb = cellType(kind).rgb;
        auto shade = [&](float f) { return color(int(rgb[0] * f), int(rgb[1] * f), int(rgb[2] * f)); };
        float w = size * 0.5f, h = size * 0.29f;
        ImVec2 top(center.x, center.y - 2.0f * h), left(center.x - w, center.y - h), right(center.x + w, center.y - h);
        ImVec2 mid(center.x, center.y), bottomLeft(center.x - w, center.y + h), bottomRight(center.x + w, center.y + h);
        ImVec2 bottom(center.x, center.y + 2.0f * h);
        draw->AddQuadFilled(top, right, mid, left, shade(1.0f));
        draw->AddQuadFilled(left, mid, bottom, bottomLeft, shade(0.72f));
        draw->AddQuadFilled(mid, right, bottomRight, bottom, shade(0.52f));
    }

    // What stamps are made of: Life follows the rule, the others are static blocks.
    void drawMaterialPicker() {
        sectionLabel("Material");
        mutedText("What stamps are made of. M cycles through them in game.");
        ImDrawList* draw = ImGui::GetWindowDrawList();
        const float gap = px(6), count = static_cast<float>(cellTypes().size());
        const float width = std::floor((ImGui::GetContentRegionAvail().x - gap * (count - 1.0f)) / count);
        const float height = ImGui::GetFrameHeight() * 1.9f;
        for (const CellType& type : cellTypes()) {
            if (type.kind != CellKind::Life) ImGui::SameLine(0, gap);
            ImGui::PushID(static_cast<int>(type.kind));
            ImVec2 min = ImGui::GetCursorScreenPos(), max(min.x + width, min.y + height);
            bool clicked = ImGui::InvisibleButton("material", ImVec2(width, height));
            bool hovered = ImGui::IsItemHovered(), selected = type.kind == material;
            draw->AddRectFilled(min, max, ImGui::GetColorU32(hovered ? ImGuiCol_FrameBgHovered : ImGuiCol_FrameBg), px(6));
            if (selected) {
                draw->AddRectFilled(min, max, materialColor(type.kind, 36), px(6));
                draw->AddRect(min, max, materialColor(type.kind), px(6), px(2));
            }
            float icon = height * 0.5f;
            drawCubeIcon(draw, ImVec2(min.x + px(10) + icon * 0.5f, min.y + height * 0.5f), icon, type.kind);
            ImVec2 text = ImGui::CalcTextSize(type.name);
            draw->AddText(ImVec2(min.x + px(18) + icon, min.y + (height - text.y) * 0.5f), IM_COL32(255, 255, 255, 255), type.name);
            if (hovered) {
                ImGui::SetNextWindowSize(ImVec2(px(300), 0));
                ImGui::BeginTooltip();
                ImGui::TextWrapped("%s", type.description);
                ImGui::EndTooltip();
            }
            if (clicked) material = type.kind;
            ImGui::PopID();
        }
        note(std::string(cellType(material).name) + ": " + cellType(material).description, IM_COL32(255, 255, 255, 255));
    }

    void drawInventory() {
        if (beginCard("##inventory", 760)) {
            cardHeader("Stamps & Rules", "Tab to close");
            // Two columns: what to build on the left, the rule on the right.
            ImGui::BeginTable("##inventory-columns", 2, ImGuiTableFlags_BordersInnerV);
            ImGui::TableSetupColumn("build", ImGuiTableColumnFlags_WidthStretch, 1.0f);
            ImGui::TableSetupColumn("rule", ImGuiTableColumnFlags_WidthStretch, 1.0f);
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            sectionLabel("Stamps");
            mutedText("Left click places, right click removes. Q/E rotate, Z/C tilt around x.");
            ImDrawList* draw = ImGui::GetWindowDrawList();
            const int perRow = 5;
            const float gap = px(6);
            const float tile = std::floor((ImGui::GetContentRegionAvail().x - gap * (perRow - 1)) / perRow);
            for (int i = -1; i < static_cast<int>(STAMP_NAMES.size()); ++i) {
                if ((i + 1) % perRow != 0) ImGui::SameLine(0, gap);
                ImGui::PushID(i);
                ImVec2 min = ImGui::GetCursorScreenPos();
                ImVec2 max(min.x + tile, min.y + tile);
                bool clicked = ImGui::InvisibleButton("tile", ImVec2(tile, tile));
                bool hovered = ImGui::IsItemHovered();
                bool selected = i == hotbarSlot;
                draw->AddRectFilled(min, max, ImGui::GetColorU32(hovered ? ImGuiCol_FrameBgHovered : ImGuiCol_FrameBg), px(6));
                if (selected) {
                    draw->AddRectFilled(min, max, accentColor(40), px(6));
                    draw->AddRect(min, max, accentColor(), px(6), px(2));
                }
                if (i >= 0) {
                    drawStampIcon(draw, min, tile, i, materialColor(material));
                } else { // empty hand: a slashed circle
                    ImVec2 center(min.x + tile * 0.5f, min.y + tile * 0.5f);
                    float radius = tile * 0.22f;
                    ImU32 muted = ImGui::GetColorU32(ImGuiCol_TextDisabled);
                    draw->AddCircle(center, radius, muted, 0, px(1.5f));
                    draw->AddLine(ImVec2(center.x - radius * 0.7f, center.y + radius * 0.7f),
                                  ImVec2(center.x + radius * 0.7f, center.y - radius * 0.7f), muted, px(1.5f));
                }
                if (hovered) {
                    ImGui::SetTooltip("%s%s\n%s", i >= 0 ? (std::to_string(i + 1) + ". ").c_str() : "",
                                      i >= 0 ? STAMP_NAMES[i] : "Empty hand",
                                      i >= 0 ? STAMP_DESCRIPTIONS[i] : "Nothing to place and no placement outline.");
                }
                if (clicked) selectSlot(i);
                ImGui::PopID();
            }
            note(hotbarSlot >= 0 ? std::string(STAMP_NAMES[hotbarSlot]) + ": " + STAMP_DESCRIPTIONS[hotbarSlot]
                                 : std::string("Empty hand: nothing to place."),
                 IM_COL32(255, 255, 255, 255));

            ImGui::Dummy(ImVec2(0, px(6)));
            drawMaterialPicker();

            ImGui::TableSetColumnIndex(1);
            sectionLabel("Rule");
            mutedText("Switching rules keeps the current cells.");
            float listHeight = ImGui::GetTextLineHeightWithSpacing() * static_cast<float>(lifeRules().size()) +
                               ImGui::GetStyle().WindowPadding.y * 2.0f;
            ImGui::BeginChild("##rules", ImVec2(0, listHeight), ImGuiChildFlags_Borders);
            for (size_t r = 0; r < lifeRules().size(); ++r) {
                const LifeRule& candidate = lifeRules()[r];
                std::string notation = describeRule(candidate);
                ImGui::PushID(static_cast<int>(r));
                if (ImGui::Selectable(candidate.name, r == ruleIndex)) {
                    ruleIndex = r;
                    notify(std::string("Rule: ") + candidate.name + " " + notation);
                }
                if (ImGui::IsItemHovered()) {
                    ImGui::SetNextWindowSize(ImVec2(px(320), 0));
                    ImGui::BeginTooltip();
                    ImGui::TextWrapped("%s", explainRule(candidate).c_str());
                    ImGui::Spacing();
                    ImGui::TextWrapped("%s", candidate.description);
                    ImGui::EndTooltip();
                }
                rightAlignNext(ImGui::CalcTextSize(notation.c_str()).x);
                ImGui::TextDisabled("%s", notation.c_str());
                ImGui::PopID();
            }
            ImGui::EndChild();
            note(std::string(rule().name) + ": " + explainRule(rule()), IM_COL32(255, 255, 255, 255));
            mutedText(rule().description);
            ImGui::EndTable();
            ImGui::Dummy(ImVec2(0, px(6)));
            switch (buttonPair("New World with This Rule", "Done", ButtonKind::Primary)) {
                case 1:
                    newWorld(true);
                    notify(std::string("New world: ") + rule().name);
                    screen = Screen::Playing;
                    setCursorCaptured(true);
                    break;
                case 2:
                    screen = Screen::Playing;
                    setCursorCaptured(true);
                    break;
            }
        }
        ImGui::End();
    }

    void drawNewWorldMenu() {
        if (beginCard("##newworld", 440)) {
            cardHeader("New World");
            if (ImGui::BeginTable("##newworld-options", 2)) {
                ImGui::TableSetupColumn("label", ImGuiTableColumnFlags_WidthFixed, px(100));
                ImGui::TableSetupColumn("control", ImGuiTableColumnFlags_WidthStretch);
                optionRow("Rule");
                if (ImGui::BeginCombo("##rule", lifeRules()[newWorldRule].name)) {
                    for (int r = 0; r < static_cast<int>(lifeRules().size()); ++r) {
                        if (ImGui::Selectable(lifeRules()[r].name, r == newWorldRule)) newWorldRule = r;
                        if (ImGui::IsItemHovered()) {
                            ImGui::SetNextWindowSize(ImVec2(px(300), 0));
                            ImGui::BeginTooltip();
                            ImGui::TextWrapped("%s", explainRule(lifeRules()[r]).c_str());
                            ImGui::EndTooltip();
                        }
                    }
                    ImGui::EndCombo();
                }
                optionRow("Seed");
                ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - px(80));
                ImGui::InputInt("##seed", &newWorldSeed, 0);
                ImGui::SameLine();
                if (ImGui::Button("Random", ImVec2(-FLT_MIN, 0))) newWorldSeed = static_cast<int>(rng() & 0x7FFFFFFF);
                optionRow("Start with");
                if (ImGui::RadioButton("Rule's seed soup", !newWorldEmpty)) newWorldEmpty = false;
                ImGui::SameLine();
                if (ImGui::RadioButton("Empty world", newWorldEmpty)) newWorldEmpty = true;
                ImGui::EndTable();
            }
            ImGui::Dummy(ImVec2(0, px(4)));
            note(explainRule(lifeRules()[newWorldRule]), IM_COL32(255, 255, 255, 255));
            mutedText(lifeRules()[newWorldRule].description);
            ImGui::Dummy(ImVec2(0, px(6)));
            switch (buttonPair("Cancel", "Create World", ButtonKind::Primary)) {
                case 1: screen = Screen::Paused; break;
                case 2:
                    ruleIndex = static_cast<size_t>(newWorldRule);
                    rng.seed(static_cast<uint32_t>(newWorldSeed));
                    newWorld(!newWorldEmpty);
                    notify(std::string("New world: ") + rule().name + (newWorldEmpty ? " (empty)" : ""));
                    resumeGame();
                    break;
            }
        }
        ImGui::End();
    }

    // --------------------------------------------------------------- tutorial

    // Lessons run in the normal game: the panel takes clicks until the player
    // clicks the world to look around.
    void openTutorial(size_t lesson) {
        tutorialPanel.open(lesson);
        screen = Screen::Playing;
        setCursorCaptured(false);
        loadTutorialScene();
    }

    void handleTutorialRequest(tutorial::Tutorial::Request request) {
        if (request == tutorial::Tutorial::Request::LoadScene) loadTutorialScene();
    }

    // Clears the world and sets up the lesson's rule, cells and camera, paused.
    void loadTutorialScene() {
        const tutorial::Lesson& lesson = tutorialPanel.lesson();
        ruleIndex = lesson.rule;
        resetChunks();
        generation = 0;
        for (const PatternCell& cell : lesson.cells) setLife(glm::ivec3(cell.x, cell.y, cell.z), true);
        rebuild(Rebuild::Reset);
        eye = glm::vec3(lesson.eye[0], lesson.eye[1], lesson.eye[2]);
        tutorial::lookAngles(lesson, yaw, pitch);
        flying = true;
        verticalSpeed = 0.0f;
        simulationRunning = runningBeforePause = false;
        stepDebt = 0.0;
        fastForward = fastForwardTotal = 0;
        material = CellKind::Life;
        hotbarSlot = -1; // an empty hand keeps the placement outline out of the scene
    }

    tutorial::Tutorial::Status tutorialStatus() const {
        return {describeRule(rule()), generation, population, simulationRunning};
    }

    // ------------------------------------------------------------------ frame

    void mainLoop() {
        auto lastTime = std::chrono::steady_clock::now();
        while (!glfwWindowShouldClose(window)) {
            glfwPollEvents();
            auto now = std::chrono::steady_clock::now();
            float frameSeconds = std::chrono::duration<float>(now - lastTime).count();
            if (framesRendered > 2) worstFrameMs = std::max(worstFrameMs, frameSeconds * 1000.0f);
            float deltaTime = std::min(frameSeconds, 0.25f);
            lastTime = now;

            if (!worldFrozen()) {
                updateMovement(deltaTime);
                updateSimulation(deltaTime);
            }
            updateTarget();
            if (screen == Screen::Playing) updateHeldButtons(deltaTime);
            if (refreshPending) {
                rebuild(Rebuild::Edit);
                updateTarget();
            } else if (glm::distance(eye, lastBuildEye) > REBUILD_DISTANCE ||
                       glm::dot(forward(), lastBuildForward) < std::cos(glm::radians(0.75f * CULL_MARGIN_DEGREES))) {
                rebuild(Rebuild::Camera); // the block list only covers what the camera could see
            }
            if (options.exitAfterFrames && framesRendered + 1 == options.exitAfterFrames && !options.screenshotPath.empty()) {
                requestScreenshot(options.screenshotPath);
            }
            buildUi();
            drawFrame();
            updateHud(deltaTime);
            if (options.exitAfterFrames && framesRendered >= options.exitAfterFrames) break;
        }
        vkDeviceWaitIdle(device);
        float seconds = std::chrono::duration<float>(std::chrono::steady_clock::now() - startTime).count();
        std::cout << "Exit: " << framesRendered << " frames in " << std::fixed << std::setprecision(1) << seconds
                  << " s, generation " << generation << ", " << population << " alive, " << activeSlots.size()
                  << " chunks, " << swapchainExtent.width << "x" << swapchainExtent.height << ", feet at "
                  << std::setprecision(2) << eye.x << " " << eye.y - EYE_HEIGHT << " " << eye.z
                  << (flying ? " (flying)" : onGround ? " (on ground)" : " (airborne)") << std::endl;
        if (simulationRunning || generation > 0) {
            std::cout << "Speed: target " << speedLabel() << ", reached " << std::setprecision(1) << measuredRate
                      << " gen/s" << (tickLimited ? " (slowed to keep up)" : "") << ", " << std::setprecision(2)
                      << stepCostMs << " ms/gen + " << drawCostMs << " ms/block list on the GPU, worst frame "
                      << std::setprecision(1) << worstFrameMs << " ms" << std::endl;
        }
    }

    void updateHud(float deltaTime) {
        hudTimer += deltaTime;
        hudFrames++;
        if (hudTimer < 0.25f) return;
        fps = hudFrames / hudTimer;
        hudTimer = 0.0f;
        hudFrames = 0;
        std::ostringstream title;
        title << std::fixed << std::setprecision(1)
              << "3D Life  |  " << rule().name << " " << describeRule(rule())
              << "  |  gen " << generation << "  |  " << population << " alive"
              << "  |  " << activeSlots.size() << " chunks" << (chunkLimitHit ? " (LIMIT)" : "")
              << "  |  " << speedLabel() << " " << (simulationRunning ? "running" : "paused")
              << "  |  " << (flying ? "flying" : "walking") << " XYZ " << eye.x << " " << eye.y - EYE_HEIGHT << " " << eye.z
              << "  |  " << (hotbarSlot + 1) << " " << handName()
              << "  |  " << std::lround(fps) << " fps";
        glfwSetWindowTitle(window, title.str().c_str());
    }

    void requestScreenshot(const std::string& path) {
        if (!captureSupported) {
            std::cerr << "Screenshots are not supported by this swapchain." << std::endl;
            return;
        }
        pendingScreenshot = path;
    }

    uint32_t writeBoxes(Box* boxes) {
        uint32_t count = 0;
        auto add = [&](const Box& box) {
            if (count < MAX_BOXES) boxes[count++] = box;
        };
        if (hudVisible && target.hit) {
            add({glm::vec4(glm::vec3(target.block) - 0.004f, 0.03f), glm::vec4(glm::vec3(target.block) + 1.004f, 0.0f)});
        }
        if (hudVisible && target.canPlace && hotbarSlot >= 0) {
            // Outline around the whole (rotated) stamp, in the material's color.
            glm::ivec3 lo(std::numeric_limits<int>::max()), hi(std::numeric_limits<int>::lowest());
            for (const glm::ivec3& cell : stampCells(static_cast<Stamp>(hotbarSlot), target.place, target.normal, true)) {
                lo = glm::min(lo, cell);
                hi = glm::max(hi, cell);
            }
            float colorId = material == CellKind::Stone ? 7.0f : material == CellKind::Ember ? 8.0f : 2.0f;
            add({glm::vec4(glm::vec3(lo) + 0.02f, 0.03f), glm::vec4(glm::vec3(hi) + 0.98f, colorId)});
        }
        if (hudVisible && tutorialPanel.active()) {
            for (const tutorial::MarkedCell& mark : tutorialPanel.lesson().marks) {
                glm::vec3 cell(mark.cell.x, mark.cell.y, mark.cell.z);
                float colorId = static_cast<float>(tutorial::FIRST_MARK_COLOR_ID + static_cast<int>(mark.mark));
                add({glm::vec4(cell - 0.03f, 0.05f), glm::vec4(cell + 1.03f, colorId)}); // outside live blocks
            }
        }
        if (showChunkBorders) {
            // The chunks around the player; a large world has far more than fit.
            const float reach = 4.0f * CHUNK;
            for (uint32_t slot : activeSlots) {
                glm::vec3 lo(slotChunk[slot] * CHUNK);
                if (glm::distance(glm::clamp(eye, lo, lo + float(CHUNK)), eye) > reach) continue;
                add({glm::vec4(lo, 0.08f), glm::vec4(lo + float(CHUNK), 1.0f)});
            }
        }
        return count;
    }

    void drawFrame() {
        waitFence(inFlightFences[currentFrame], "a frame");

        uint32_t imageIndex = 0;
        VkResult result = vkAcquireNextImageKHR(device, swapchain, GPU_TIMEOUT_NS, imageAvailableSemaphores[currentFrame],
                                                VK_NULL_HANDLE, &imageIndex);
        if (result == VK_ERROR_OUT_OF_DATE_KHR) {
            recreateSwapchain();
            return;
        }
        if (result == VK_TIMEOUT || result == VK_NOT_READY) return; // compositor is not ready; try next loop
        if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
            throw std::runtime_error("Failed to acquire swap chain image!");
        }
        vkResetFences(device, 1, &inFlightFences[currentFrame]);

        const double now = glfwGetTime();
        glm::mat4 viewProj = viewProjection();
        FrameUniforms uniforms{};
        uniforms.viewProj = viewProj;
        uniforms.invViewProj = glm::inverse(viewProj);
        uniforms.camera = glm::vec4(eye, std::chrono::duration<float>(std::chrono::steady_clock::now() - startTime).count());
        uniforms.viewport = glm::vec4(swapchainExtent.width, swapchainExtent.height, hudVisible ? 1.0f : 0.0f,
                                      float(std::min<uint64_t>(drawnBlocks, MAX_INSTANCES)));
        uniforms.hotbar = glm::ivec4(hotbarSlot, static_cast<int>(STAMP_NAMES.size()), static_cast<int>(material), 0);
        float renderDistance = static_cast<float>(settings.renderDistance);
        uniforms.fog = glm::vec4(0.45f * renderDistance, renderDistance, 0.0f, 0.0f);
        float progress = changeAnimationSeconds > 0.0f
                             ? static_cast<float>(std::clamp((now - lastChangeTime) / changeAnimationSeconds, 0.0, 1.0))
                             : 1.0f;
        uniforms.anim = glm::vec4(progress, settings.smoothLighting ? 1.0f : 0.0f, 0.0f, 0.0f);
        uniforms.sun = glm::vec4(SUN_DIRECTION, 0.0f);
        std::memcpy(uniformBuffers[currentFrame].mapped, &uniforms, sizeof(uniforms));
        uint32_t boxCount = writeBoxes(boxBuffers[currentFrame].as<Box>());

        std::string screenshot;
        screenshot.swap(pendingScreenshot);
        if (!screenshot.empty()) prepareCaptureBuffer();

        VkCommandBuffer cmd = commandBuffers[currentFrame];
        vkResetCommandBuffer(cmd, 0);
        recordCommandBuffer(cmd, imageIndex, boxCount, !screenshot.empty());

        VkSemaphoreSubmitInfo waitInfo{VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO};
        waitInfo.semaphore = imageAvailableSemaphores[currentFrame];
        waitInfo.stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
        VkSemaphoreSubmitInfo signalInfo{VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO};
        signalInfo.semaphore = renderFinishedSemaphores[imageIndex];
        signalInfo.stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
        VkCommandBufferSubmitInfo commandInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO};
        commandInfo.commandBuffer = cmd;
        VkSubmitInfo2 submitInfo{VK_STRUCTURE_TYPE_SUBMIT_INFO_2};
        submitInfo.waitSemaphoreInfoCount = 1;
        submitInfo.pWaitSemaphoreInfos = &waitInfo;
        submitInfo.commandBufferInfoCount = 1;
        submitInfo.pCommandBufferInfos = &commandInfo;
        submitInfo.signalSemaphoreInfoCount = 1;
        submitInfo.pSignalSemaphoreInfos = &signalInfo;
        if (vkQueueSubmit2(gpu.queue, 1, &submitInfo, inFlightFences[currentFrame]) != VK_SUCCESS) {
            throw std::runtime_error("Failed to submit draw command buffer!");
        }

        VkPresentInfoKHR presentInfo{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = &renderFinishedSemaphores[imageIndex];
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = &swapchain;
        presentInfo.pImageIndices = &imageIndex;
        result = vkQueuePresentKHR(gpu.queue, &presentInfo);

        if (!screenshot.empty()) saveCapture(screenshot, currentFrame);

        framesRendered++;
        currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || framebufferResized) {
            recreateSwapchain();
        } else if (result != VK_SUCCESS) {
            throw std::runtime_error("Failed to present swap chain image!");
        }
    }

    static void imageBarrier(VkCommandBuffer cmd, VkImage image, VkImageAspectFlags aspect, VkImageLayout from,
                             VkImageLayout to, VkPipelineStageFlags2 srcStage, VkAccessFlags2 srcAccess,
                             VkPipelineStageFlags2 dstStage, VkAccessFlags2 dstAccess) {
        VkImageMemoryBarrier2 barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2};
        barrier.srcStageMask = srcStage;
        barrier.srcAccessMask = srcAccess;
        barrier.dstStageMask = dstStage;
        barrier.dstAccessMask = dstAccess;
        barrier.oldLayout = from;
        barrier.newLayout = to;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = image;
        barrier.subresourceRange = {aspect, 0, 1, 0, 1};
        VkDependencyInfo dependency{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
        dependency.imageMemoryBarrierCount = 1;
        dependency.pImageMemoryBarriers = &barrier;
        vkCmdPipelineBarrier2(cmd, &dependency);
    }

    void recordCommandBuffer(VkCommandBuffer cmd, uint32_t imageIndex, uint32_t boxCount, bool capture) {
        VkCommandBufferBeginInfo beginInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        if (vkBeginCommandBuffer(cmd, &beginInfo) != VK_SUCCESS) {
            throw std::runtime_error("Failed to begin recording command buffer!");
        }
        VkImage image = swapchainImages[imageIndex];
        // The sky pass covers every pixel, so the old contents can be discarded.
        imageBarrier(cmd, image, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                     VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, 0, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                     VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT);
        imageBarrier(cmd, depthImage, VK_IMAGE_ASPECT_DEPTH_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
                     VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
                     VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                     VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
                     VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT);

        VkRenderingAttachmentInfo colorAttachment{VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
        colorAttachment.imageView = swapchainImageViews[imageIndex];
        colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        VkRenderingAttachmentInfo depthAttachment{VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
        depthAttachment.imageView = depthImageView;
        depthAttachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
        depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depthAttachment.clearValue.depthStencil = {1.0f, 0};
        VkRenderingInfo rendering{VK_STRUCTURE_TYPE_RENDERING_INFO};
        rendering.renderArea.extent = swapchainExtent;
        rendering.layerCount = 1;
        rendering.colorAttachmentCount = 1;
        rendering.pColorAttachments = &colorAttachment;
        rendering.pDepthAttachment = &depthAttachment;
        vkCmdBeginRendering(cmd, &rendering);

        VkViewport viewport{0.0f, 0.0f, float(swapchainExtent.width), float(swapchainExtent.height), 0.0f, 1.0f};
        VkRect2D scissor{{0, 0}, swapchainExtent};
        vkCmdSetViewport(cmd, 0, 1, &viewport);
        vkCmdSetScissor(cmd, 0, 1, &scissor);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &frameSets[currentFrame], 0, nullptr);
        auto setMode = [&](uint32_t mode) {
            vkCmdPushConstants(cmd, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                               sizeof(mode), &mode);
        };

        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, skyPipeline);
        setMode(0);
        vkCmdDraw(cmd, 3, 1, 0, 0);

        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, blockPipeline);
        vkCmdBindIndexBuffer(cmd, indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT16);
        vkCmdDrawIndexedIndirect(cmd, indirectBuffer.buffer, 0, 1, sizeof(VkDrawIndexedIndirectCommand));
        if (boxCount > 0) {
            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, boxPipeline);
            VkDeviceSize offset = 0;
            vkCmdBindVertexBuffers(cmd, 0, 1, &vertexBuffer.buffer, &offset);
            vkCmdDraw(cmd, static_cast<uint32_t>(cubeVertices.size()), boxCount * 12, 0, 0);
        }

        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, gridPipeline);
        setMode(1);
        vkCmdDraw(cmd, 3, 1, 0, 0);

        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, hudPipeline);
        setMode(2);
        vkCmdDraw(cmd, 3, 1, 0, 0);
        if (imguiReady) {
            ImDrawData* drawData = ImGui::GetDrawData();
            if (drawData && drawData->CmdLists.Size > 0) ImGui_ImplVulkan_RenderDrawData(drawData, cmd);
        }
        vkCmdEndRendering(cmd);

        if (capture) {
            imageBarrier(cmd, image, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                         VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                         VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_2_COPY_BIT, VK_ACCESS_2_TRANSFER_READ_BIT);
            VkBufferImageCopy region{};
            region.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
            region.imageExtent = {swapchainExtent.width, swapchainExtent.height, 1};
            vkCmdCopyImageToBuffer(cmd, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, captureBuffer.buffer, 1, &region);
            memoryBarrier(cmd, VK_PIPELINE_STAGE_2_COPY_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_2_HOST_BIT,
                          VK_ACCESS_2_HOST_READ_BIT);
            imageBarrier(cmd, image, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                         VK_PIPELINE_STAGE_2_COPY_BIT, 0, VK_PIPELINE_STAGE_2_NONE, 0);
        } else {
            imageBarrier(cmd, image, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                         VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                         VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_2_NONE, 0);
        }

        if (vkEndCommandBuffer(cmd) != VK_SUCCESS) {
            throw std::runtime_error("Failed to record command buffer!");
        }
    }

    // ------------------------------------------------------------ screenshots

    void prepareCaptureBuffer() {
        destroyBuffer(captureBuffer);
        createBuffer(captureBuffer, VkDeviceSize(swapchainExtent.width) * swapchainExtent.height * 4,
                     VK_BUFFER_USAGE_TRANSFER_DST_BIT, {HOST_CACHED, HOST});
    }

    void saveCapture(const std::string& path, size_t frame) {
        waitFence(inFlightFences[frame], "the screenshot frame");
        const uint32_t width = swapchainExtent.width, height = swapchainExtent.height;
        const uint8_t* pixels = captureBuffer.as<const uint8_t>();
        bool bgr = swapchainImageFormat == VK_FORMAT_B8G8R8A8_SRGB || swapchainImageFormat == VK_FORMAT_B8G8R8A8_UNORM;
        std::vector<uint8_t> rgb(size_t(width) * height * 3);
        for (size_t i = 0; i < size_t(width) * height; ++i) {
            rgb[i * 3 + 0] = pixels[i * 4 + (bgr ? 2 : 0)];
            rgb[i * 3 + 1] = pixels[i * 4 + 1];
            rgb[i * 3 + 2] = pixels[i * 4 + (bgr ? 0 : 2)];
        }
        destroyBuffer(captureBuffer); // a full-screen buffer is too big to keep around
        if (writePng(path, width, height, rgb)) std::cout << "Saved screenshot " << path << std::endl;
        else std::cerr << "Failed to write screenshot " << path << std::endl;
    }

    // ---------------------------------------------------------------- cleanup

    void cleanup() {
        if (device == VK_NULL_HANDLE) {
            gpu.destroy();
            if (window) glfwDestroyWindow(window);
            window = nullptr;
            glfwTerminate();
            return;
        }
        vkDeviceWaitIdle(device);

        if (imguiReady) {
            ImGui_ImplVulkan_Shutdown();
            ImGui_ImplGlfw_Shutdown();
            ImGui::DestroyContext();
            imguiReady = false;
        }
        destroyBuffer(captureBuffer);
        vkDestroyFence(device, computeFence, nullptr);
        if (timestamps) vkDestroyQueryPool(device, timestamps, nullptr);
        for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            vkDestroySemaphore(device, imageAvailableSemaphores[i], nullptr);
            vkDestroyFence(device, inFlightFences[i], nullptr);
            destroyBuffer(uniformBuffers[i]);
            destroyBuffer(boxBuffers[i]);
        }
        for (VkPipeline pipeline : {stepPipeline, buildPipeline, blockPipeline, boxPipeline, skyPipeline, gridPipeline, hudPipeline}) {
            vkDestroyPipeline(device, pipeline, nullptr);
        }
        vkDestroyPipelineLayout(device, computePipelineLayout, nullptr);
        vkDestroyDescriptorPool(device, computeDescriptorPool, nullptr);
        vkDestroyDescriptorSetLayout(device, computeSetLayout, nullptr);
        vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
        vkDestroyDescriptorPool(device, descriptorPool, nullptr);
        vkDestroyDescriptorSetLayout(device, frameSetLayout, nullptr);
        destroyBuffer(vertexBuffer);
        destroyBuffer(indexBuffer);
        releaseChunkPool(pool);
        for (GpuBuffer* b : {&blockPool, &instanceBuffer, &indirectBuffer, &readbackBuffer}) destroyBuffer(*b);
        destroySwapchainResources();
        device = VK_NULL_HANDLE;
        gpu.destroy();
        if (window) glfwDestroyWindow(window);
        window = nullptr;
        glfwTerminate();
    }
};

int main(int argc, char** argv) {
    try {
        Options options = parseOptions(argc, argv);
        std::filesystem::path exeDir = executableDirectory(argv[0]);
        initVulkanLoader(exeDir);
        std::filesystem::path restart;
        int code = 0;
        {
            LifePrototypeApp app(options, findShaderDirectory(exeDir), exeDir);
            code = app.run();
            restart = app.restartPath;
        }
#if !defined(_WIN32)
        if (!restart.empty()) {
            // An updated AppImage replaced this one in place; start it.
            execl(restart.c_str(), restart.c_str(), static_cast<char*>(nullptr));
            std::cerr << "Could not restart " << restart << std::endl;
        }
#endif
        return code;
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
#if defined(_WIN32)
        MessageBoxA(nullptr, e.what(), "3D Life", MB_OK | MB_ICONERROR); // no console in release builds
#endif
        return 1;
    }
}

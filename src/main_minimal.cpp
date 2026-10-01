// 3D Game of Life - playable Vulkan prototype with a Minecraft-style world.
//
// The world is unbounded and stored as 16^3-cell chunks that exist only where
// life is (or is about to be). shaders/life3d_chunks.comp steps every active
// chunk, appends live cells to an instance list, and reports per-chunk
// population plus which neighbor chunks border cells touch; the CPU then
// allocates and frees chunks to follow the pattern. Cells are drawn as blocks
// with an indirect instanced draw. Controls follow Minecraft's defaults; see
// printControls().
//
// Build with GLM_FORCE_DEPTH_ZERO_TO_ONE defined for the whole target (a
// precompiled header may include GLM before this file).

#include <algorithm>
#include <array>
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
#include <unordered_map>
#include <unordered_set>
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

#include "WindowManager.h"
#include "VulkanContext.h"
#include "engine/vulkan/resources/ShaderManager.h"
#include "Life3DRules.h"
#include "Life3DPatterns.h"
#include "MenuFont.h"
#include "Updater.h"
#include "tutorial/Tutorial.h"

using namespace VulkanHIP;

namespace {

constexpr int CHUNK = 16;
constexpr uint32_t CHUNK_CELLS = CHUNK * CHUNK * CHUNK;
constexpr uint32_t NO_CHUNK = 0xFFFFFFFFu;
constexpr uint32_t MAX_INSTANCES = 1u << 20; // drawn blocks; more are simulated but not drawn
constexpr uint64_t GPU_TIMEOUT_NS = 2'000'000'000; // treat a longer wait as a lost device

struct IVec3Hash {
    size_t operator()(const glm::ivec3& v) const {
        return (size_t(uint32_t(v.x)) * 73856093u) ^ (size_t(uint32_t(v.y)) * 19349663u) ^
               (size_t(uint32_t(v.z)) * 83492791u);
    }
};

glm::ivec3 chunkOf(const glm::ivec3& cell) { return cell >> 4; } // floor division by 16
uint32_t localIndex(const glm::ivec3& cell) {
    glm::ivec3 l = cell & (CHUNK - 1);
    return static_cast<uint32_t>((l.z * CHUNK + l.y) * CHUNK + l.x);
}
glm::ivec3 neighborOffset(int k) { return glm::ivec3(k % 3 - 1, (k / 3) % 3 - 1, k / 9 - 1); }

// Scripted input for end-to-end checks, applied in order before the first frame.
struct ScriptAction {
    enum Kind { Position, Look, Place, Break, Slot, Resize, Push, Rotate, Tilt } kind;
    glm::vec3 value{0.0f};
};

struct Options {
    size_t rule = 0; // Life 5766, the closest 3D analog of Conway's Life
    uint32_t seed = std::random_device{}();
    uint32_t chunkCapacity = 2048;
    bool empty = false;
    bool run = false;
    bool chunkBorders = false;
    uint32_t warmupSteps = 0;
    std::string screenshotPath;
    uint32_t exitAfterFrames = 0;
    bool verify = false;
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
                 "  --chunks N        chunk budget, 64-16000 (default 2048; 16^3 cells each)\n"
                 "  --empty           start with an empty world\n"
                 "  --run             start with the simulation running\n"
                 "  --steps N         advance N generations before the first frame\n"
                 "  --borders         show chunk borders (F3+G in game)\n"
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
                 "  --tilt N          tilt the stamp N quarter turns around x (like pressing C N times)\n";
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
        else if (arg == "--chunks") options.chunkCapacity = std::clamp<uint32_t>(std::stoul(value(i)), 64, 16000);
        else if (arg == "--empty") options.empty = true;
        else if (arg == "--run") options.run = true;
        else if (arg == "--borders") options.chunkBorders = true;
        else if (arg == "--steps") options.warmupSteps = static_cast<uint32_t>(std::stoul(value(i)));
        else if (arg == "--screenshot") options.screenshotPath = value(i);
        else if (arg == "--frames") options.exitAfterFrames = static_cast<uint32_t>(std::stoul(value(i)));
        else if (arg == "--verify") options.verify = true;
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
        if (std::filesystem::exists(dir / "life3d_chunks.comp.spv", ec)) return dir;
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
            else if (key == "renderDistance") settings.renderDistance = std::clamp(std::stoi(value), 64, 512);
            else if (key == "guiScale") settings.guiScale = std::clamp(std::stoi(value), 0, 4);
            else if (key == "checkUpdates") settings.checkUpdates = value != "false";
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
        << "\ncheckUpdates:" << (settings.checkUpdates ? "true" : "false") << "\n";
}

} // namespace

class LifePrototypeApp {
public:
    LifePrototypeApp(Options options, std::filesystem::path shaderDir, std::filesystem::path exeDir)
        : options(std::move(options)), shaderDir(std::move(shaderDir)), exeDir(std::move(exeDir)), rng(this->options.seed) {
        ruleIndex = this->options.rule;
        simulationRunning = this->options.run;
        showChunkBorders = this->options.chunkBorders;
        chunkCapacity = this->options.chunkCapacity;
        // Scripted and test runs use defaults so results do not depend on the player's options.
        persistSettings = this->options.script.empty() && this->options.screenshotPath.empty() &&
                          !this->options.verify && this->options.menu.empty();
        if (persistSettings) settings = loadSettings();
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
        for (uint32_t i = 0; i < options.warmupSteps; ++i) runPass(true);
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
            for (uint32_t i = 0; i < options.warmupSteps; ++i) runPass(true);
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
    };

    // Matches the push constants in shaders/life3d_chunks.comp.
    struct PassConstants {
        uint32_t activeCount;
        uint32_t surviveMask;
        uint32_t birthMask;
        uint32_t applyRule;
        uint32_t maxInstances;
    };

    struct Box {
        glm::vec4 min; // w = edge thickness
        glm::vec4 max; // w = color id (0 target, 1 chunk border, 2 air placement)
    };

    // What the crosshair points at within reach.
    struct Target {
        bool hit = false;       // a live block is targeted
        glm::ivec3 block{0};    // the targeted block
        bool canPlace = false;
        glm::ivec3 place{0};    // where a stamp's anchor goes
        glm::ivec3 normal{0, 1, 0}; // direction stamps grow into
    };

    static constexpr int MAX_FRAMES_IN_FLIGHT = 2;
    static constexpr std::array<float, 8> SPEEDS = {0.5f, 1.0f, 2.0f, 4.0f, 8.0f, 15.0f, 30.0f, 60.0f};
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

    Options options;
    std::filesystem::path shaderDir;
    std::filesystem::path exeDir;
    std::mt19937 rng;

    WindowManager* windowManager = nullptr;
    VulkanContext* vulkanContext = nullptr;
    VkDevice device = VK_NULL_HANDLE;
    std::unique_ptr<ShaderManager> shaderManager;

    // Swapchain and targets
    VkSwapchainKHR swapchain = VK_NULL_HANDLE;
    std::vector<VkImage> swapchainImages;
    std::vector<VkImageView> swapchainImageViews;
    std::vector<VkFramebuffer> framebuffers;
    VkFormat swapchainImageFormat = VK_FORMAT_UNDEFINED;
    VkExtent2D swapchainExtent{};
    bool captureSupported = false;
    VkImage depthImage = VK_NULL_HANDLE;
    VkDeviceMemory depthImageMemory = VK_NULL_HANDLE;
    VkImageView depthImageView = VK_NULL_HANDLE;
    VkFormat depthFormat = VK_FORMAT_UNDEFINED;
    VkRenderPass renderPass = VK_NULL_HANDLE;
    bool framebufferResized = false;

    // Rendering: all pipelines share one layout and one descriptor set per frame.
    VkDescriptorSetLayout frameSetLayout = VK_NULL_HANDLE;
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    VkPipeline worldPipeline = VK_NULL_HANDLE;
    VkPipeline skyPipeline = VK_NULL_HANDLE;
    VkPipeline gridPipeline = VK_NULL_HANDLE;
    VkPipeline hudPipeline = VK_NULL_HANDLE;
    VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
    std::array<VkDescriptorSet, MAX_FRAMES_IN_FLIGHT> frameSets{};
    std::vector<Vertex> cubeVertices;
    VkBuffer vertexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory vertexBufferMemory = VK_NULL_HANDLE;
    std::array<VkBuffer, MAX_FRAMES_IN_FLIGHT> uniformBuffers{};
    std::array<VkDeviceMemory, MAX_FRAMES_IN_FLIGHT> uniformBuffersMemory{};
    std::array<void*, MAX_FRAMES_IN_FLIGHT> uniformBuffersMapped{};
    std::array<VkBuffer, MAX_FRAMES_IN_FLIGHT> boxBuffers{};
    std::array<VkDeviceMemory, MAX_FRAMES_IN_FLIGHT> boxBuffersMemory{};
    std::array<Box*, MAX_FRAMES_IN_FLIGHT> boxBuffersMapped{};

    // Frame synchronization. Render-finished semaphores are per swapchain image
    // because presentation may still hold them after the frame fence signals.
    std::array<VkCommandBuffer, MAX_FRAMES_IN_FLIGHT> commandBuffers{};
    std::array<VkSemaphore, MAX_FRAMES_IN_FLIGHT> imageAvailableSemaphores{};
    std::array<VkFence, MAX_FRAMES_IN_FLIGHT> inFlightFences{};
    std::vector<VkSemaphore> renderFinishedSemaphores;
    size_t currentFrame = 0;

    // Chunk pool. cellBuffers ping-pong between generations; the tables are
    // indexed by slot and rewritten by the CPU between passes.
    uint32_t chunkCapacity = 2048;
    std::array<VkBuffer, 2> cellBuffers{};
    std::array<VkDeviceMemory, 2> cellMemory{};
    std::array<uint32_t*, 2> cellsMapped{};
    uint32_t currentCells = 0;
    VkBuffer neighborBuffer = VK_NULL_HANDLE, activeBuffer = VK_NULL_HANDLE, originBuffer = VK_NULL_HANDLE;
    VkDeviceMemory neighborMemory = VK_NULL_HANDLE, activeMemory = VK_NULL_HANDLE, originMemory = VK_NULL_HANDLE;
    uint32_t* neighborsMapped = nullptr;
    uint32_t* activeMapped = nullptr;
    glm::ivec4* originsMapped = nullptr;
    VkBuffer statsBuffer = VK_NULL_HANDLE;
    VkDeviceMemory statsMemory = VK_NULL_HANDLE;
    glm::uvec2* statsMapped = nullptr;
    VkBuffer instanceBuffer = VK_NULL_HANDLE, indirectBuffer = VK_NULL_HANDLE;
    VkDeviceMemory instanceMemory = VK_NULL_HANDLE, indirectMemory = VK_NULL_HANDLE;

    std::unordered_map<glm::ivec3, uint32_t, IVec3Hash> chunkSlots;
    std::vector<glm::ivec3> slotChunk;
    std::vector<uint32_t> freeSlots;
    std::vector<uint32_t> activeSlots;
    bool tablesDirty = true;
    bool chunkLimitHit = false;
    bool pausedAtLimit = false; // pause once per world when the chunk budget runs out
    bool refreshPending = false;

    VkDescriptorSetLayout computeSetLayout = VK_NULL_HANDLE;
    VkPipelineLayout computePipelineLayout = VK_NULL_HANDLE;
    VkPipeline computePipeline = VK_NULL_HANDLE;
    VkDescriptorPool computeDescriptorPool = VK_NULL_HANDLE;
    std::array<VkDescriptorSet, 2> computeSets{}; // [which cell buffer is current]
    VkCommandBuffer computeCommandBuffer = VK_NULL_HANDLE;
    VkFence computeFence = VK_NULL_HANDLE;

    // Simulation
    size_t ruleIndex = 0;
    bool simulationRunning = false;
    size_t speedIndex = 1; // SPEEDS[1]: one generation per second
    float stepAccumulator = 0.0f;
    uint64_t generation = 0;
    uint64_t population = 0;

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
    VkBuffer captureBuffer = VK_NULL_HANDLE;
    VkDeviceMemory captureMemory = VK_NULL_HANDLE;

    // HUD / timing
    std::chrono::steady_clock::time_point startTime = std::chrono::steady_clock::now();
    float hudTimer = 0.0f;
    uint32_t hudFrames = 0;
    float fps = 0.0f;
    uint64_t framesRendered = 0;

    const LifeRule& rule() const { return lifeRules()[ruleIndex]; }

    void waitFence(VkFence fence, const char* what) {
        VkResult result = vkWaitForFences(device, 1, &fence, VK_TRUE, GPU_TIMEOUT_NS);
        if (result == VK_TIMEOUT) throw std::runtime_error(std::string("GPU did not finish ") + what + " within 2 s");
        if (result != VK_SUCCESS) throw std::runtime_error(std::string("Lost the GPU while waiting for ") + what);
    }

    // ------------------------------------------------------------------ setup

    void initWindow() {
        windowManager = &WindowManager::getInstance();
        WindowManager::WindowConfig config{};
        config.width = windowedWidth;
        config.height = windowedHeight;
        config.title = "3D Game of Life";
        windowManager->init(config);

        GLFWwindow* window = windowManager->getWindow();
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
        vulkanContext = &VulkanContext::getInstance();
        uint32_t glfwExtensionCount = 0;
        const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
        std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);
        vulkanContext->init(extensions);
        device = vulkanContext->getDevice();

        VkPhysicalDeviceProperties properties;
        vkGetPhysicalDeviceProperties(vulkanContext->getPhysicalDevice(), &properties);
        std::cout << "GPU: " << properties.deviceName << std::endl;
    }

    void initRendering() {
        shaderManager = std::make_unique<ShaderManager>(vulkanContext);
        createSwapchain();
        createImageViews();
        depthFormat = findDepthFormat();
        createDepthResources();
        createRenderPass();
        createFramebuffers();
        createPerImageSemaphores();

        createWorldBuffers();
        createVertexBuffer();
        createFrameBuffers();
        createFrameSetLayout();
        createGraphicsPipelines();
        createFrameSets();
        createComputePipeline();
        createComputeSets();
        createCommandBuffers();
        createSyncObjects();
    }

    std::optional<uint32_t> findMemoryType(uint32_t typeBits, VkMemoryPropertyFlags wanted) {
        VkPhysicalDeviceMemoryProperties properties;
        vkGetPhysicalDeviceMemoryProperties(vulkanContext->getPhysicalDevice(), &properties);
        for (uint32_t i = 0; i < properties.memoryTypeCount; ++i) {
            if ((typeBits & (1u << i)) && (properties.memoryTypes[i].propertyFlags & wanted) == wanted) return i;
        }
        return std::nullopt;
    }

    // Allocates with the first memory property set in `preferences` that is available.
    void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, std::initializer_list<VkMemoryPropertyFlags> preferences,
                      VkBuffer& buffer, VkDeviceMemory& memory, void** mapped = nullptr) {
        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = size;
        bufferInfo.usage = usage;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        if (vkCreateBuffer(device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create buffer!");
        }
        VkMemoryRequirements requirements;
        vkGetBufferMemoryRequirements(device, buffer, &requirements);
        std::optional<uint32_t> type;
        for (VkMemoryPropertyFlags wanted : preferences) {
            if ((type = findMemoryType(requirements.memoryTypeBits, wanted))) break;
        }
        if (!type) throw std::runtime_error("No suitable memory type for buffer!");
        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = requirements.size;
        allocInfo.memoryTypeIndex = *type;
        if (vkAllocateMemory(device, &allocInfo, nullptr, &memory) != VK_SUCCESS) {
            throw std::runtime_error("Failed to allocate buffer memory!");
        }
        vkBindBufferMemory(device, buffer, memory, 0);
        if (mapped && vkMapMemory(device, memory, 0, VK_WHOLE_SIZE, 0, mapped) != VK_SUCCESS) {
            throw std::runtime_error("Failed to map buffer memory!");
        }
    }

    static constexpr VkMemoryPropertyFlags HOST = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    static constexpr VkMemoryPropertyFlags HOST_CACHED = HOST | VK_MEMORY_PROPERTY_HOST_CACHED_BIT;
    static constexpr VkMemoryPropertyFlags DEVICE_HOST = HOST | VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    static constexpr VkMemoryPropertyFlags DEVICE = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

    void createSwapchain() {
        SwapChainSupportDetails support = vulkanContext->querySwapChainSupport(vulkanContext->getPhysicalDevice());
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
            glfwGetFramebufferSize(windowManager->getWindow(), &width, &height);
            extent.width = std::clamp(static_cast<uint32_t>(width), support.capabilities.minImageExtent.width,
                                      support.capabilities.maxImageExtent.width);
            extent.height = std::clamp(static_cast<uint32_t>(height), support.capabilities.minImageExtent.height,
                                       support.capabilities.maxImageExtent.height);
        }

        uint32_t imageCount = support.capabilities.minImageCount + 1;
        if (support.capabilities.maxImageCount > 0) imageCount = std::min(imageCount, support.capabilities.maxImageCount);

        captureSupported = support.capabilities.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

        VkSwapchainCreateInfoKHR createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        createInfo.surface = vulkanContext->getSurface();
        createInfo.minImageCount = imageCount;
        createInfo.imageFormat = surfaceFormat.format;
        createInfo.imageColorSpace = surfaceFormat.colorSpace;
        createInfo.imageExtent = extent;
        createInfo.imageArrayLayers = 1;
        createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                                (captureSupported ? VK_IMAGE_USAGE_TRANSFER_SRC_BIT : 0);

        QueueFamilyIndices indices = vulkanContext->getQueueFamilyIndices();
        uint32_t queueFamilyIndices[] = {indices.graphicsFamily.value(), indices.presentFamily.value()};
        if (indices.graphicsFamily != indices.presentFamily) {
            createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
            createInfo.queueFamilyIndexCount = 2;
            createInfo.pQueueFamilyIndices = queueFamilyIndices;
        } else {
            createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        }
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
            VkImageViewCreateInfo createInfo{};
            createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
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
        for (VkFormat format : {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT}) {
            VkFormatProperties props;
            vkGetPhysicalDeviceFormatProperties(vulkanContext->getPhysicalDevice(), format, &props);
            if (props.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) return format;
        }
        throw std::runtime_error("Failed to find supported depth format!");
    }

    void createDepthResources() {
        VkImageCreateInfo imageInfo{};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
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
        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = requirements.size;
        allocInfo.memoryTypeIndex = *type;
        if (vkAllocateMemory(device, &allocInfo, nullptr, &depthImageMemory) != VK_SUCCESS) {
            throw std::runtime_error("Failed to allocate depth image memory!");
        }
        vkBindImageMemory(device, depthImage, depthImageMemory, 0);

        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = depthImage;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = depthFormat;
        viewInfo.subresourceRange = {VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1};
        if (vkCreateImageView(device, &viewInfo, nullptr, &depthImageView) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create depth image view!");
        }
    }

    void createRenderPass() {
        VkAttachmentDescription colorAttachment{};
        colorAttachment.format = swapchainImageFormat;
        colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
        colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE; // the sky pass covers every pixel
        colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        VkAttachmentDescription depthAttachment{};
        depthAttachment.format = depthFormat;
        depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
        depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        VkAttachmentReference colorRef{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
        VkAttachmentReference depthRef{1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};
        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &colorRef;
        subpass.pDepthStencilAttachment = &depthRef;

        VkSubpassDependency dependency{};
        dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        dependency.dstSubpass = 0;
        dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
        dependency.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

        std::array<VkAttachmentDescription, 2> attachments = {colorAttachment, depthAttachment};
        VkRenderPassCreateInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
        renderPassInfo.pAttachments = attachments.data();
        renderPassInfo.subpassCount = 1;
        renderPassInfo.pSubpasses = &subpass;
        renderPassInfo.dependencyCount = 1;
        renderPassInfo.pDependencies = &dependency;
        if (vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create render pass!");
        }
    }

    void createFramebuffers() {
        framebuffers.resize(swapchainImageViews.size());
        for (size_t i = 0; i < swapchainImageViews.size(); i++) {
            std::array<VkImageView, 2> attachments = {swapchainImageViews[i], depthImageView};
            VkFramebufferCreateInfo framebufferInfo{};
            framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            framebufferInfo.renderPass = renderPass;
            framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
            framebufferInfo.pAttachments = attachments.data();
            framebufferInfo.width = swapchainExtent.width;
            framebufferInfo.height = swapchainExtent.height;
            framebufferInfo.layers = 1;
            if (vkCreateFramebuffer(device, &framebufferInfo, nullptr, &framebuffers[i]) != VK_SUCCESS) {
                throw std::runtime_error("Failed to create framebuffer!");
            }
        }
    }

    void createPerImageSemaphores() {
        VkSemaphoreCreateInfo semaphoreInfo{};
        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
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
        for (auto framebuffer : framebuffers) vkDestroyFramebuffer(device, framebuffer, nullptr);
        framebuffers.clear();
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
        glfwGetFramebufferSize(windowManager->getWindow(), &width, &height);
        while ((width == 0 || height == 0) && !windowManager->shouldClose()) {
            glfwWaitEvents(); // minimized
            glfwGetFramebufferSize(windowManager->getWindow(), &width, &height);
        }
        vkDeviceWaitIdle(device);
        destroySwapchainResources();
        createSwapchain();
        createImageViews();
        createDepthResources();
        createFramebuffers();
        createPerImageSemaphores();
        framebufferResized = false;
    }

    void createWorldBuffers() {
        VkDeviceSize cellBytes = VkDeviceSize(chunkCapacity) * CHUNK_CELLS * sizeof(uint32_t);
        for (int i = 0; i < 2; ++i) {
            // Device-local and CPU-visible (resizable BAR) when available: the GPU
            // does the heavy reads, the CPU only touches a few cells for editing.
            createBuffer(cellBytes, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, {DEVICE_HOST, HOST}, cellBuffers[i], cellMemory[i],
                         reinterpret_cast<void**>(&cellsMapped[i]));
        }
        createBuffer(VkDeviceSize(chunkCapacity) * 27 * sizeof(uint32_t), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                     {DEVICE_HOST, HOST}, neighborBuffer, neighborMemory, reinterpret_cast<void**>(&neighborsMapped));
        createBuffer(VkDeviceSize(chunkCapacity) * sizeof(uint32_t), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                     {DEVICE_HOST, HOST}, activeBuffer, activeMemory, reinterpret_cast<void**>(&activeMapped));
        createBuffer(VkDeviceSize(chunkCapacity) * sizeof(glm::ivec4), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                     {DEVICE_HOST, HOST}, originBuffer, originMemory, reinterpret_cast<void**>(&originsMapped));
        createBuffer(VkDeviceSize(chunkCapacity) * sizeof(glm::uvec2),
                     VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, {HOST_CACHED, HOST},
                     statsBuffer, statsMemory, reinterpret_cast<void**>(&statsMapped));
        createBuffer(VkDeviceSize(MAX_INSTANCES) * sizeof(glm::ivec4), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, {DEVICE, HOST},
                     instanceBuffer, instanceMemory);
        createBuffer(sizeof(VkDrawIndirectCommand),
                     VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                     {DEVICE, HOST}, indirectBuffer, indirectMemory);

        slotChunk.assign(chunkCapacity, glm::ivec3(0));
        std::fill(neighborsMapped, neighborsMapped + size_t(chunkCapacity) * 27, NO_CHUNK);
        resetChunks();
    }

    void createVertexBuffer() {
        // One unit cube as 12 triangles with per-face normals. Culling is off,
        // so winding does not matter.
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
        void* data = nullptr;
        createBuffer(size, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, {DEVICE_HOST, HOST}, vertexBuffer, vertexBufferMemory, &data);
        std::memcpy(data, cubeVertices.data(), size);
    }

    void createFrameBuffers() {
        for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            createBuffer(sizeof(FrameUniforms), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, {DEVICE_HOST, HOST},
                         uniformBuffers[i], uniformBuffersMemory[i], &uniformBuffersMapped[i]);
            createBuffer(VkDeviceSize(chunkCapacity + 1) * sizeof(Box), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                         {DEVICE_HOST, HOST}, boxBuffers[i], boxBuffersMemory[i],
                         reinterpret_cast<void**>(&boxBuffersMapped[i]));
        }
    }

    void createFrameSetLayout() {
        std::array<VkDescriptorSetLayoutBinding, 3> bindings{};
        bindings[0] = {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, nullptr};
        bindings[1] = {1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, nullptr};
        bindings[2] = {2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, nullptr};
        VkDescriptorSetLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
        layoutInfo.pBindings = bindings.data();
        if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &frameSetLayout) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create descriptor set layout!");
        }

        VkPushConstantRange pushRange{VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(uint32_t)};
        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = 1;
        pipelineLayoutInfo.pSetLayouts = &frameSetLayout;
        pipelineLayoutInfo.pushConstantRangeCount = 1;
        pipelineLayoutInfo.pPushConstantRanges = &pushRange;
        if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create pipeline layout!");
        }
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
        VkPipelineShaderStageCreateInfo vertStage{};
        VkPipelineShaderStageCreateInfo fragStage{};
        shaderManager->createShaderStages((shaderDir / spec.vertexShader).string(),
                                          (shaderDir / spec.fragmentShader).string(), vertStage, fragStage);
        VkPipelineShaderStageCreateInfo stages[] = {vertStage, fragStage};

        VkVertexInputBindingDescription binding{0, sizeof(Vertex), VK_VERTEX_INPUT_RATE_VERTEX};
        std::array<VkVertexInputAttributeDescription, 2> attributes{{
            {0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, pos)},
            {1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, normal)},
        }};
        VkPipelineVertexInputStateCreateInfo vertexInput{};
        vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        if (spec.cubeVertices) {
            vertexInput.vertexBindingDescriptionCount = 1;
            vertexInput.pVertexBindingDescriptions = &binding;
            vertexInput.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributes.size());
            vertexInput.pVertexAttributeDescriptions = attributes.data();
        }

        VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
        inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

        VkPipelineViewportStateCreateInfo viewportState{};
        viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewportState.viewportCount = 1;
        viewportState.scissorCount = 1;

        VkPipelineRasterizationStateCreateInfo rasterizer{};
        rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
        rasterizer.lineWidth = 1.0f;
        rasterizer.cullMode = VK_CULL_MODE_NONE;
        rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;

        VkPipelineMultisampleStateCreateInfo multisampling{};
        multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

        VkPipelineDepthStencilStateCreateInfo depthStencil{};
        depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
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
        VkPipelineColorBlendStateCreateInfo colorBlending{};
        colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlending.attachmentCount = 1;
        colorBlending.pAttachments = &blendAttachment;

        std::array<VkDynamicState, 2> dynamicStates = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
        VkPipelineDynamicStateCreateInfo dynamicState{};
        dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
        dynamicState.pDynamicStates = dynamicStates.data();

        VkGraphicsPipelineCreateInfo pipelineInfo{};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipelineInfo.stageCount = 2;
        pipelineInfo.pStages = stages;
        pipelineInfo.pVertexInputState = &vertexInput;
        pipelineInfo.pInputAssemblyState = &inputAssembly;
        pipelineInfo.pViewportState = &viewportState;
        pipelineInfo.pRasterizationState = &rasterizer;
        pipelineInfo.pMultisampleState = &multisampling;
        pipelineInfo.pDepthStencilState = &depthStencil;
        pipelineInfo.pColorBlendState = &colorBlending;
        pipelineInfo.pDynamicState = &dynamicState;
        pipelineInfo.layout = pipelineLayout;
        pipelineInfo.renderPass = renderPass;
        VkPipeline pipeline = VK_NULL_HANDLE;
        if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create graphics pipeline!");
        }
        return pipeline;
    }

    void createGraphicsPipelines() {
        worldPipeline = createGraphicsPipeline({"life3d_world.vert.spv", "life3d_world.frag.spv", true, true, true, false});
        skyPipeline = createGraphicsPipeline({"life3d_screen.vert.spv", "life3d_screen.frag.spv", false, false, false, false});
        gridPipeline = createGraphicsPipeline({"life3d_screen.vert.spv", "life3d_screen.frag.spv", false, true, false, true});
        hudPipeline = createGraphicsPipeline({"life3d_screen.vert.spv", "life3d_screen.frag.spv", false, false, false, true});
    }

    void writeBufferDescriptor(VkDescriptorSet set, uint32_t binding, VkDescriptorType type, VkBuffer buffer) {
        VkDescriptorBufferInfo bufferInfo{buffer, 0, VK_WHOLE_SIZE};
        VkWriteDescriptorSet write{};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
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
            {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, MAX_FRAMES_IN_FLIGHT * 2},
        }};
        VkDescriptorPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
        poolInfo.pPoolSizes = poolSizes.data();
        poolInfo.maxSets = MAX_FRAMES_IN_FLIGHT;
        if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create descriptor pool!");
        }
        for (int frame = 0; frame < MAX_FRAMES_IN_FLIGHT; ++frame) {
            VkDescriptorSetAllocateInfo allocInfo{};
            allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
            allocInfo.descriptorPool = descriptorPool;
            allocInfo.descriptorSetCount = 1;
            allocInfo.pSetLayouts = &frameSetLayout;
            if (vkAllocateDescriptorSets(device, &allocInfo, &frameSets[frame]) != VK_SUCCESS) {
                throw std::runtime_error("Failed to allocate descriptor set!");
            }
            writeBufferDescriptor(frameSets[frame], 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, uniformBuffers[frame]);
            writeBufferDescriptor(frameSets[frame], 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, instanceBuffer);
            writeBufferDescriptor(frameSets[frame], 2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, boxBuffers[frame]);
        }
    }

    void createComputePipeline() {
        std::array<VkDescriptorSetLayoutBinding, 8> bindings{};
        for (uint32_t i = 0; i < bindings.size(); ++i) {
            bindings[i] = {i, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr};
        }
        VkDescriptorSetLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
        layoutInfo.pBindings = bindings.data();
        if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &computeSetLayout) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create compute descriptor set layout!");
        }

        VkPushConstantRange pushRange{VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(PassConstants)};
        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = 1;
        pipelineLayoutInfo.pSetLayouts = &computeSetLayout;
        pipelineLayoutInfo.pushConstantRangeCount = 1;
        pipelineLayoutInfo.pPushConstantRanges = &pushRange;
        if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &computePipelineLayout) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create compute pipeline layout!");
        }

        VkComputePipelineCreateInfo pipelineInfo{};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        pipelineInfo.layout = computePipelineLayout;
        pipelineInfo.stage = shaderManager->createComputeStage((shaderDir / "life3d_chunks.comp.spv").string());
        if (vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &computePipeline) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create compute pipeline!");
        }
    }

    // computeSets[p] reads cellBuffers[p] and writes the other cell buffer.
    void createComputeSets() {
        VkDescriptorPoolSize poolSize{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 16};
        VkDescriptorPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.poolSizeCount = 1;
        poolInfo.pPoolSizes = &poolSize;
        poolInfo.maxSets = 2;
        if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &computeDescriptorPool) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create compute descriptor pool!");
        }
        for (int parity = 0; parity < 2; ++parity) {
            VkDescriptorSetAllocateInfo allocInfo{};
            allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
            allocInfo.descriptorPool = computeDescriptorPool;
            allocInfo.descriptorSetCount = 1;
            allocInfo.pSetLayouts = &computeSetLayout;
            if (vkAllocateDescriptorSets(device, &allocInfo, &computeSets[parity]) != VK_SUCCESS) {
                throw std::runtime_error("Failed to allocate compute descriptor set!");
            }
            const std::array<VkBuffer, 8> buffers = {cellBuffers[parity], cellBuffers[1 - parity], neighborBuffer,
                                                     activeBuffer, originBuffer, statsBuffer, instanceBuffer, indirectBuffer};
            for (uint32_t binding = 0; binding < buffers.size(); ++binding) {
                writeBufferDescriptor(computeSets[parity], binding, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, buffers[binding]);
            }
        }
    }

    void createCommandBuffers() {
        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = vulkanContext->getGraphicsCommandPool();
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
        VkSemaphoreCreateInfo semaphoreInfo{};
        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        VkFenceCreateInfo fenceInfo{};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
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
    }

    // ----------------------------------------------------------------- chunks

    void resetChunks() {
        chunkSlots.clear();
        freeSlots.clear();
        for (uint32_t slot = chunkCapacity; slot-- > 0;) freeSlots.push_back(slot); // hand out low slots first
        activeSlots.clear();
        tablesDirty = true;
        chunkLimitHit = false;
        pausedAtLimit = false;
    }

    uint32_t findChunk(const glm::ivec3& key) const {
        auto it = chunkSlots.find(key);
        return it == chunkSlots.end() ? NO_CHUNK : it->second;
    }

    uint32_t ensureChunk(const glm::ivec3& key) {
        uint32_t slot = findChunk(key);
        if (slot != NO_CHUNK) return slot;
        if (freeSlots.empty()) {
            chunkLimitHit = true;
            return NO_CHUNK;
        }
        slot = freeSlots.back();
        freeSlots.pop_back();
        std::memset(cellsMapped[currentCells] + size_t(slot) * CHUNK_CELLS, 0, CHUNK_CELLS * sizeof(uint32_t));
        originsMapped[slot] = glm::ivec4(key * CHUNK, 0);
        slotChunk[slot] = key;
        chunkSlots.emplace(key, slot);
        tablesDirty = true;
        return slot;
    }

    void freeChunk(uint32_t slot) {
        chunkSlots.erase(slotChunk[slot]);
        freeSlots.push_back(slot);
        tablesDirty = true;
    }

    void rebuildChunkTables() {
        activeSlots.clear();
        for (const auto& entry : chunkSlots) activeSlots.push_back(entry.second);
        std::sort(activeSlots.begin(), activeSlots.end());
        std::copy(activeSlots.begin(), activeSlots.end(), activeMapped);
        for (uint32_t slot : activeSlots) {
            for (int k = 0; k < 27; ++k) {
                neighborsMapped[size_t(slot) * 27 + k] = findChunk(slotChunk[slot] + neighborOffset(k));
            }
        }
        tablesDirty = false;
    }

    // After a pass: keep chunks that hold life, add chunks that live border cells
    // can spread into, and free the rest.
    void maintainChunks() {
        population = 0;
        std::unordered_set<glm::ivec3, IVec3Hash> wanted;
        for (uint32_t slot : activeSlots) {
            glm::uvec2 stats = statsMapped[slot];
            population += stats.x;
            for (int k = 0; k < 27; ++k) {
                if (stats.y & (1u << k)) wanted.insert(slotChunk[slot] + neighborOffset(k));
            }
        }
        for (uint32_t slot : activeSlots) {
            if (statsMapped[slot].x == 0 && !wanted.count(slotChunk[slot])) freeChunk(slot);
        }
        for (const glm::ivec3& key : wanted) ensureChunk(key);
    }

    bool cellAlive(const glm::ivec3& cell) const {
        uint32_t slot = findChunk(chunkOf(cell));
        return slot != NO_CHUNK && cellsMapped[currentCells][size_t(slot) * CHUNK_CELLS + localIndex(cell)] != 0;
    }

    // CPU edits land in the current generation; the next pass picks them up.
    void setCell(const glm::ivec3& cell, bool alive) {
        uint32_t slot = alive ? ensureChunk(chunkOf(cell)) : findChunk(chunkOf(cell));
        if (slot == NO_CHUNK) return;
        cellsMapped[currentCells][size_t(slot) * CHUNK_CELLS + localIndex(cell)] = alive ? 1u : 0u;
        refreshPending = true;
    }

    void seedSoup(const glm::ivec3& minCorner, const glm::ivec3& size, float density) {
        std::uniform_real_distribution<float> chance(0.0f, 1.0f);
        for (int z = 0; z < size.z; ++z)
            for (int y = 0; y < size.y; ++y)
                for (int x = 0; x < size.x; ++x)
                    if (chance(rng) < density) setCell(minCorner + glm::ivec3(x, y, z), true);
    }

    void newWorld(bool seed) {
        tutorialPanel.close(); // its lesson no longer matches the world
        resetChunks();
        generation = 0;
        population = 0;
        int size = rule().seedSize;
        // The seed soup rests on the ground (y = 0), like a structure in a superflat world.
        if (seed) seedSoup(glm::ivec3(-size / 2, 0, -size / 2), glm::ivec3(size), rule().seedDensity);
        runPass(false);
        // Spawn on the ground at a distance, facing the seed.
        float distance = std::max(20.0f, 1.6f * static_cast<float>(size));
        eye = glm::vec3(0.6f * distance, EYE_HEIGHT, 0.8f * distance);
        glm::vec3 dir = glm::normalize(glm::vec3(0.0f, 0.4f * static_cast<float>(size), 0.0f) - eye);
        yaw = glm::degrees(std::atan2(dir.z, dir.x));
        pitch = glm::degrees(std::asin(dir.y));
        flying = false;
        verticalSpeed = 0.0f;
    }

    // Save format: "L3D1", rule index, generation, player pose, then live cells
    // as int32 x,y,z triples. All values little-endian as written by this machine.
    bool saveWorld(const std::string& path) {
        std::vector<glm::ivec3> cells;
        const uint32_t* current = cellsMapped[currentCells];
        for (const auto& [key, slot] : chunkSlots) {
            const uint32_t* chunk = current + size_t(slot) * CHUNK_CELLS;
            for (uint32_t i = 0; i < CHUNK_CELLS; ++i) {
                if (!chunk[i]) continue;
                glm::ivec3 local(i % CHUNK, (i / CHUNK) % CHUNK, i / (CHUNK * CHUNK));
                cells.push_back(key * CHUNK + local);
            }
        }
        std::ofstream out(path, std::ios::binary);
        auto put = [&](const auto& value) { out.write(reinterpret_cast<const char*>(&value), sizeof(value)); };
        out.write("L3D1", 4);
        put(static_cast<uint32_t>(ruleIndex));
        put(static_cast<uint64_t>(generation));
        put(eye);
        put(yaw);
        put(pitch);
        put(static_cast<uint64_t>(cells.size()));
        out.write(reinterpret_cast<const char*>(cells.data()), std::streamsize(cells.size() * sizeof(glm::ivec3)));
        if (!out) {
            std::cerr << "Failed to save " << path << std::endl;
            return false;
        }
        std::cout << "Saved " << cells.size() << " cells to " << std::filesystem::absolute(path).string() << std::endl;
        return true;
    }

    bool loadWorld(const std::string& path) {
        std::ifstream in(path, std::ios::binary);
        char magic[4] = {};
        uint32_t savedRule = 0;
        uint64_t savedGeneration = 0, count = 0;
        glm::vec3 savedEye(0.0f);
        float savedYaw = 0.0f, savedPitch = 0.0f;
        auto get = [&](auto& value) { in.read(reinterpret_cast<char*>(&value), sizeof(value)); };
        in.read(magic, 4);
        get(savedRule);
        get(savedGeneration);
        get(savedEye);
        get(savedYaw);
        get(savedPitch);
        get(count);
        if (!in || std::string(magic, 4) != "L3D1" || savedRule >= lifeRules().size() || count > (1ull << 32)) {
            std::cerr << "Not a 3D Life save: " << path << std::endl;
            return false;
        }
        std::vector<glm::ivec3> cells(count);
        in.read(reinterpret_cast<char*>(cells.data()), std::streamsize(count * sizeof(glm::ivec3)));
        if (!in) {
            std::cerr << "Truncated save: " << path << std::endl;
            return false;
        }
        tutorialPanel.close();
        resetChunks();
        for (const glm::ivec3& cell : cells) setCell(cell, true);
        ruleIndex = savedRule;
        generation = savedGeneration;
        eye = savedEye;
        yaw = savedYaw;
        pitch = savedPitch;
        verticalSpeed = 0.0f;
        runPass(false);
        std::cout << "Loaded " << population << " cells from " << path
                  << (chunkLimitHit ? " (chunk budget reached; some cells were dropped)" : "") << std::endl;
        return true;
    }

    // One GPU pass: advance a generation (applyRule) or just rebuild the drawn
    // instances and chunk stats after edits.
    void runPass(bool applyRule) {
        if (tablesDirty) rebuildChunkTables();
        const uint32_t activeCount = static_cast<uint32_t>(activeSlots.size());

        vkResetCommandBuffer(computeCommandBuffer, 0);
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkBeginCommandBuffer(computeCommandBuffer, &beginInfo);

        // Earlier frames may still be drawing the instance list this pass rewrites.
        VkMemoryBarrier before{VK_STRUCTURE_TYPE_MEMORY_BARRIER, nullptr, VK_ACCESS_SHADER_WRITE_BIT,
                               VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT};
        vkCmdPipelineBarrier(computeCommandBuffer,
                             VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                             VK_PIPELINE_STAGE_TRANSFER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &before, 0, nullptr, 0, nullptr);
        vkCmdFillBuffer(computeCommandBuffer, statsBuffer, 0, VK_WHOLE_SIZE, 0);
        const VkDrawIndirectCommand emptyDraw{static_cast<uint32_t>(cubeVertices.size()), 0, 0, 0};
        vkCmdUpdateBuffer(computeCommandBuffer, indirectBuffer, 0, sizeof(emptyDraw), &emptyDraw);
        VkMemoryBarrier cleared{VK_STRUCTURE_TYPE_MEMORY_BARRIER, nullptr, VK_ACCESS_TRANSFER_WRITE_BIT,
                                VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT};
        vkCmdPipelineBarrier(computeCommandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                             0, 1, &cleared, 0, nullptr, 0, nullptr);

        if (activeCount > 0) {
            vkCmdBindPipeline(computeCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, computePipeline);
            vkCmdBindDescriptorSets(computeCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, computePipelineLayout, 0, 1,
                                    &computeSets[currentCells], 0, nullptr);
            PassConstants constants{activeCount, rule().surviveMask, rule().birthMask, applyRule ? 1u : 0u, MAX_INSTANCES};
            vkCmdPushConstants(computeCommandBuffer, computePipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0,
                               sizeof(constants), &constants);
            vkCmdDispatch(computeCommandBuffer, CHUNK / 4, CHUNK / 4, (CHUNK / 4) * activeCount);
        }

        // Make results visible to the indirect draw, the vertex shader and the CPU.
        VkMemoryBarrier after{VK_STRUCTURE_TYPE_MEMORY_BARRIER, nullptr, VK_ACCESS_SHADER_WRITE_BIT,
                              VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_INDIRECT_COMMAND_READ_BIT | VK_ACCESS_HOST_READ_BIT};
        vkCmdPipelineBarrier(computeCommandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                             VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT | VK_PIPELINE_STAGE_HOST_BIT,
                             0, 1, &after, 0, nullptr, 0, nullptr);
        vkEndCommandBuffer(computeCommandBuffer);

        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &computeCommandBuffer;
        vkResetFences(device, 1, &computeFence);
        if (vkQueueSubmit(vulkanContext->getGraphicsQueue(), 1, &submitInfo, computeFence) != VK_SUCCESS) {
            throw std::runtime_error("Failed to submit compute work!");
        }
        waitFence(computeFence, "the simulation pass");

        if (applyRule) {
            currentCells = 1 - currentCells;
            generation++;
        }
        refreshPending = false;
        maintainChunks();
    }

    void updateSimulation(float deltaTime) {
        if (!simulationRunning) return;
        stepAccumulator += deltaTime * SPEEDS[speedIndex];
        int steps = 0;
        while (stepAccumulator >= 1.0f && steps < 4) {
            runPass(true);
            stepAccumulator -= 1.0f;
            ++steps;
            if (chunkLimitHit && !pausedAtLimit) {
                // Past the budget, growth freezes at the edge; stop and say so instead.
                pausedAtLimit = true;
                simulationRunning = false;
                std::cout << "Chunk budget of " << chunkCapacity << " reached at generation " << generation
                          << ": paused. Press G to keep going (growth stops at the edge), or start with --chunks N."
                          << std::endl;
                break;
            }
        }
        stepAccumulator = std::min(stepAccumulator, 1.0f); // drop backlog instead of spiraling
    }

    // Compares the chunked GPU world with the dense CPU reference. The soup is
    // centered on a chunk corner so neighbor lookups and chunk growth are exercised.
    bool verifyAgainstReference() {
        constexpr int BOX = 80, HALF = BOX / 2, SOUP = 12, STEPS = 20;
        bool ok = true;
        std::vector<uint32_t> expected(BOX * BOX * BOX), scratch;
        for (size_t r = 0; r < lifeRules().size(); ++r) {
            ruleIndex = r;
            resetChunks();
            generation = 0;
            seedSoup(glm::ivec3(-SOUP / 2), glm::ivec3(SOUP), std::max(rule().seedDensity, 0.25f));
            runPass(false);
            for (int z = 0; z < BOX; ++z)
                for (int y = 0; y < BOX; ++y)
                    for (int x = 0; x < BOX; ++x)
                        expected[(z * BOX + y) * BOX + x] = cellAlive(glm::ivec3(x, y, z) - HALF) ? 1u : 0u;
            size_t mismatches = 0;
            for (int step = 0; step < STEPS; ++step) {
                stepLifeReference(expected, scratch, BOX, BOX, BOX, rule());
                expected.swap(scratch);
                runPass(true);
                uint64_t expectedPopulation = 0;
                for (int z = 0; z < BOX; ++z)
                    for (int y = 0; y < BOX; ++y)
                        for (int x = 0; x < BOX; ++x) {
                            uint32_t want = expected[(z * BOX + y) * BOX + x];
                            expectedPopulation += want;
                            mismatches += (cellAlive(glm::ivec3(x, y, z) - HALF) ? 1u : 0u) != want;
                        }
                mismatches += expectedPopulation != population; // catches stray cells outside the box
            }
            std::cout << "verify " << rule().name << " " << describeRule(rule()) << ": "
                      << (mismatches ? "FAIL" : "ok") << " (" << mismatches << " mismatches, population " << population
                      << ", " << chunkSlots.size() << " chunks)" << std::endl;
            ok = ok && mismatches == 0;
        }
        return ok;
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

    // Keyboard input only counts while the mouse is captured; gravity always applies.
    void updateMovement(float deltaTime) {
        GLFWwindow* window = windowManager->getWindow();
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

    // Moves one axis at a time (y first, like Minecraft) and stops at live blocks
    // the player was not already inside. Blocks born inside the player never trap
    // it. While walking, the y = 0 ground is solid.
    // True when a live block or the ground is directly under the player's feet.
    bool standingOnSomething() const {
        constexpr float PROBE = 0.01f;
        glm::vec3 lo = playerMin(eye), hi = playerMax(eye);
        if (lo.y >= 0.0f && lo.y < PROBE) return true;
        int below = static_cast<int>(std::floor(lo.y - PROBE));
        if (below >= static_cast<int>(std::floor(lo.y))) return false; // feet are not near a block top
        for (int z = static_cast<int>(std::floor(lo.z)); z < static_cast<int>(std::ceil(hi.z)); ++z)
            for (int x = static_cast<int>(std::floor(lo.x)); x < static_cast<int>(std::ceil(hi.x)); ++x)
                if (cellAlive(glm::ivec3(x, below, z))) return true;
        return false;
    }

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
                        if (wasInside || !cellAlive(glm::ivec3(x, yy, z))) continue;
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
            if (cellAlive(cell)) {
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

    void placeStamp() {
        if (!target.canPlace || hotbarSlot < 0) return;
        for (const glm::ivec3& cell : stampCells(static_cast<Stamp>(hotbarSlot), target.place, target.normal)) {
            if (!overlapsPlayer(cell)) setCell(cell, true);
        }
    }

    void breakBlock() {
        if (target.hit) setCell(target.block, false);
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
            case ScriptAction::Push:
                moveWithCollision(action.value);
                std::cout << "push: feet at " << eye.x << " " << eye.y - EYE_HEIGHT << " " << eye.z << std::endl;
                break;
            case ScriptAction::Resize:
                glfwSetWindowSize(windowManager->getWindow(), static_cast<int>(action.value.x), static_cast<int>(action.value.y));
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
                if (refreshPending) runPass(false);
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
        glfwSetInputMode(windowManager->getWindow(), GLFW_CURSOR, captured ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
        if (!captured) breakHeld = placeHeld = false;
    }

    void toggleFullscreen() {
        GLFWwindow* window = windowManager->getWindow();
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
        stepAccumulator = 0.0f;
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
        GLFWwindow* window = windowManager->getWindow();
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
        if (action == GLFW_REPEAT && key == GLFW_KEY_N) {
            runPass(true);
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
                    stepAccumulator = 0.0f;
                }
                break;
            case GLFW_KEY_N:
                if (ctrl) {
                    newWorld(!shift);
                    notify(shift ? "New empty world" : std::string("New world: ") + rule().name);
                } else {
                    runPass(true);
                }
                break;
            case GLFW_KEY_R: {
                size_t count = lifeRules().size();
                ruleIndex = shift ? (ruleIndex + count - 1) % count : (ruleIndex + 1) % count;
                notify(std::string("Rule: ") + rule().name + " " + describeRule(rule()));
                break;
            }
            case GLFW_KEY_EQUAL:
            case GLFW_KEY_KP_ADD:
            case GLFW_KEY_RIGHT_BRACKET:
                speedIndex = std::min(speedIndex + 1, SPEEDS.size() - 1);
                break;
            case GLFW_KEY_MINUS:
            case GLFW_KEY_KP_SUBTRACT:
            case GLFW_KEY_LEFT_BRACKET:
                speedIndex = speedIndex > 0 ? speedIndex - 1 : 0;
                break;
            case GLFW_KEY_F1: hudVisible = !hudVisible; break;
            case GLFW_KEY_F2: requestScreenshot(timestampedScreenshotName()); break;
            case GLFW_KEY_F11: toggleFullscreen(); break;
            case GLFW_KEY_H: printControls(); break;
            default: break;
        }
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
                     "  G  run/pause generations   N  single generation   [ ]  slower/faster\n"
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
        ImGui_ImplGlfw_InitForVulkan(windowManager->getWindow(), true);

        VkInstance instance = vulkanContext->getVkInstance();
        ImGui_ImplVulkan_LoadFunctions(VK_API_VERSION_1_0, [](const char* name, void* user) {
            return vkGetInstanceProcAddr(*static_cast<VkInstance*>(user), name);
        }, &instance);
        ImGui_ImplVulkan_InitInfo info{};
        info.ApiVersion = VK_API_VERSION_1_0;
        info.Instance = vulkanContext->getVkInstance();
        info.PhysicalDevice = vulkanContext->getPhysicalDevice();
        info.Device = device;
        info.QueueFamily = vulkanContext->getQueueFamilyIndices().graphicsFamily.value();
        info.Queue = vulkanContext->getGraphicsQueue();
        info.DescriptorPoolSize = IMGUI_IMPL_VULKAN_MINIMUM_IMAGE_SAMPLER_POOL_SIZE + 4;
        info.RenderPass = renderPass;
        info.MinImageCount = 2;
        info.ImageCount = std::max<uint32_t>(2, static_cast<uint32_t>(swapchainImages.size()));
        info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
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

    float effectiveUiScale() const {
        if (settings.guiScale > 0) return static_cast<float>(settings.guiScale);
        return std::max(1.0f, std::round(static_cast<float>(swapchainExtent.height) / 500.0f));
    }

    // Rebuilds the fonts at the GUI scale so text stays crisp.
    void updateUiScale() {
        float scale = effectiveUiScale();
        if (scale == uiScale) return;
        if (uiScale != 0.0f) {
            vkDeviceWaitIdle(device);
            ImGui_ImplVulkan_DestroyFontsTexture(); // recreated by the next NewFrame
        }
        ImGuiIO& io = ImGui::GetIO();
        io.Fonts->Clear();
        ImFontConfig font;
        font.FontDataOwnedByAtlas = false; // the TTF lives in the executable (MenuFont.h)
        font.OversampleH = 2;
        void* ttf = const_cast<unsigned char*>(MENU_FONT_TTF);
        io.Fonts->AddFontFromMemoryTTF(ttf, MENU_FONT_TTF_SIZE, 15.0f * scale, &font);
        titleFont = io.Fonts->AddFontFromMemoryTTF(ttf, MENU_FONT_TTF_SIZE, 22.0f * scale, &font);
        ImGui::GetStyle() = baseStyle;
        ImGui::GetStyle().ScaleAllSizes(scale);
        uiScale = scale;
    }

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

    void drawHudOverlay() {
        if (!hudVisible) return;
        ImDrawList* draw = ImGui::GetForegroundDrawList();
        ImVec2 size = ImGui::GetIO().DisplaySize;
        float line = ImGui::GetTextLineHeight();

        // Status line, and more when F3 is on.
        std::ostringstream status;
        status << rule().name << "  |  gen " << generation << "  |  " << SPEEDS[speedIndex] << " gen/s "
               << (simulationRunning ? "running" : "paused (G)");
        std::vector<std::string> lines = {status.str()};
        if (showDebug) {
            std::ostringstream a, b, c, d;
            a << std::fixed << std::setprecision(3) << "XYZ: " << eye.x << " / " << eye.y - EYE_HEIGHT << " / " << eye.z;
            glm::ivec3 chunk = chunkOf(glm::ivec3(glm::floor(eye)));
            b << "Chunk: " << chunk.x << " " << chunk.y << " " << chunk.z << "  |  " << (flying ? "flying" : "walking")
              << (onGround ? ", on ground" : "");
            c << std::fixed << std::setprecision(1) << "Facing: yaw " << yaw << ", pitch " << pitch;
            d << population << " alive" << (population > MAX_INSTANCES ? " (draw capped)" : "") << "  |  "
              << chunkSlots.size() << " / " << chunkCapacity << " chunks" << (chunkLimitHit ? " (LIMIT)" : "")
              << "  |  " << std::lround(fps) << " fps";
            lines.insert(lines.end(), {describeRule(rule()), a.str(), b.str(), c.str(), d.str()});
            if (target.hit) lines.push_back("Targeted block: " + std::to_string(target.block.x) + " " +
                                            std::to_string(target.block.y) + " " + std::to_string(target.block.z));
        }
        // One rounded panel behind all lines, with an accent bar on the left.
        float width = 0.0f;
        for (const std::string& text : lines) width = std::max(width, ImGui::CalcTextSize(text.c_str()).x);
        ImVec2 min(px(6), px(6));
        ImVec2 max(min.x + width + px(16), min.y + line * static_cast<float>(lines.size()) + px(8));
        draw->AddRectFilled(min, max, color(14, 18, 28, 170), px(6));
        draw->AddRectFilled(min, ImVec2(min.x + px(3), max.y), accentColor(), px(6), ImDrawFlags_RoundCornersLeft);
        for (size_t i = 0; i < lines.size(); ++i) {
            ImVec2 pos(min.x + px(10), min.y + px(4) + line * static_cast<float>(i));
            draw->AddText(pos, i == 0 ? IM_COL32(255, 255, 255, 255) : color(200, 210, 225), lines[i].c_str());
        }

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
        draw->AddRect(min, max, accentColor(alpha * 2 / 3), radius, 0, px(1));
        draw->AddText(pos, IM_COL32(255, 255, 255, alpha), toast.c_str());
    }

    // ------------------------------------------------------- menu building blocks

    // Dims the world, then opens a card of the given width centered on screen.
    // Always pair with ImGui::End().
    bool beginCard(const char* id, float width) {
        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->Pos);
        ImGui::SetNextWindowSize(viewport->Size);
        ImGui::SetNextWindowBgAlpha(0.5f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::Begin((std::string(id) + "-backdrop").c_str(), nullptr,
                     ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
                         ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNav);
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
        ImFont* font = titleFont ? titleFont : ImGui::GetFont();
        float height = font->FontSize;
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
        ImGui::PushFont(font);
        ImGui::TextUnformatted(title);
        ImGui::PopFont();
        if (subtitle) {
            ImVec2 size = ImGui::CalcTextSize(subtitle);
            ImGui::SameLine(ImGui::GetContentRegionMax().x - size.x);
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (height - size.y) * 0.5f);
            ImGui::TextDisabled("%s", subtitle);
        }
        ImGui::Dummy(ImVec2(0, px(2)));
        ImGui::Separator();
        ImGui::Dummy(ImVec2(0, px(2)));
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

    void drawPauseMenu() {
        if (beginCard("##pause", 300)) {
            cardHeader("3D Life", "Paused");
            std::ostringstream status;
            status << rule().name << "  |  gen " << generation << "  |  " << population << " alive";
            mutedText(status.str());
            ImGui::Dummy(ImVec2(0, px(4)));
            if (menuItem("Resume", "Esc", ButtonKind::Primary)) resumeGame();
            ImGui::Dummy(ImVec2(0, px(4)));
            if (menuItem("Stamps & Rules", "Tab")) screen = Screen::Inventory;
            if (menuItem("Tutorial")) openTutorial(tutorialPanel.lessonIndex());
            if (menuItem("New World...")) openNewWorldScreen();
            if (menuItem("Save World", "Ctrl+S")) saveWorldWithMessage();
            if (menuItem("Load World", "Ctrl+O")) loadWorldWithMessage();
            if (menuItem("Settings")) screen = Screen::Settings;
            ImGui::Dummy(ImVec2(0, px(2)));
            ImGui::Separator();
            ImGui::Dummy(ImVec2(0, px(2)));
            if (menuItem("Quit", "Ctrl+Q", ButtonKind::Danger)) glfwSetWindowShouldClose(windowManager->getWindow(), GLFW_TRUE);
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
                        glfwSetWindowShouldClose(windowManager->getWindow(), GLFW_TRUE);
                    }
                } else {
                    note("3D Life " + release.version + " is downloaded and verified.", accentColor());
                    if (menuItem("Install and Restart", nullptr, ButtonKind::Primary) && updater->launchInstaller()) {
                        glfwSetWindowShouldClose(windowManager->getWindow(), GLFW_TRUE);
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
                ImGui::SliderInt("##render", &settings.renderDistance, 64, 512, "%d blocks");
                optionRow("Fullscreen");
                bool wantFullscreen = fullscreen;
                if (ImGui::Checkbox("##fullscreen", &wantFullscreen)) toggleFullscreen();

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
                int speed = static_cast<int>(speedIndex);
                std::string format = formatSpeed(SPEEDS[speedIndex]) + " gen/s";
                if (ImGui::SliderInt("##speed", &speed, 0, static_cast<int>(SPEEDS.size()) - 1, format.c_str())) {
                    speedIndex = static_cast<size_t>(speed);
                }

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

    static std::string formatSpeed(float speed) {
        std::ostringstream out;
        out << speed;
        return out.str();
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

    void drawInventory() {
        if (beginCard("##inventory", 470)) {
            cardHeader("Stamps & Rules", "Tab to close");
            sectionLabel("Stamps");
            mutedText("Left click places, right click removes. Q/E rotate, Z/C tilt around x.");
            ImDrawList* draw = ImGui::GetWindowDrawList();
            const int tiles = static_cast<int>(STAMP_NAMES.size()) + 1;
            const float gap = px(6);
            const float tile = std::floor((ImGui::GetContentRegionAvail().x - gap * (tiles - 1)) / tiles);
            for (int i = -1; i < static_cast<int>(STAMP_NAMES.size()); ++i) {
                if (i != -1) ImGui::SameLine(0, gap);
                ImGui::PushID(i);
                ImVec2 min = ImGui::GetCursorScreenPos();
                ImVec2 max(min.x + tile, min.y + tile);
                bool clicked = ImGui::InvisibleButton("tile", ImVec2(tile, tile));
                bool hovered = ImGui::IsItemHovered();
                bool selected = i == hotbarSlot;
                draw->AddRectFilled(min, max, ImGui::GetColorU32(hovered ? ImGuiCol_FrameBgHovered : ImGuiCol_FrameBg), px(6));
                if (selected) {
                    draw->AddRectFilled(min, max, accentColor(40), px(6));
                    draw->AddRect(min, max, accentColor(), px(6), 0, px(2));
                }
                if (i >= 0) {
                    drawStampIcon(draw, min, tile, i, accentColor());
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
                ImGui::SameLine(ImGui::GetContentRegionMax().x - ImGui::CalcTextSize(notation.c_str()).x);
                ImGui::TextDisabled("%s", notation.c_str());
                ImGui::PopID();
            }
            ImGui::EndChild();
            note(std::string(rule().name) + ": " + explainRule(rule()), IM_COL32(255, 255, 255, 255));
            mutedText(rule().description);
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
        for (const PatternCell& cell : lesson.cells) setCell(glm::ivec3(cell.x, cell.y, cell.z), true);
        runPass(false);
        eye = glm::vec3(lesson.eye[0], lesson.eye[1], lesson.eye[2]);
        tutorial::lookAngles(lesson, yaw, pitch);
        flying = true;
        verticalSpeed = 0.0f;
        simulationRunning = runningBeforePause = false;
        stepAccumulator = 0.0f;
        hotbarSlot = -1; // an empty hand keeps the placement outline out of the scene
    }

    tutorial::Tutorial::Status tutorialStatus() const {
        return {describeRule(rule()), generation, population, simulationRunning};
    }

    // ------------------------------------------------------------------ frame

    void mainLoop() {
        auto lastTime = std::chrono::steady_clock::now();
        while (!windowManager->shouldClose()) {
            glfwPollEvents();
            auto now = std::chrono::steady_clock::now();
            float deltaTime = std::min(std::chrono::duration<float>(now - lastTime).count(), 0.25f);
            lastTime = now;

            if (!worldFrozen()) {
                updateMovement(deltaTime);
                updateSimulation(deltaTime);
            }
            updateTarget();
            if (screen == Screen::Playing) updateHeldButtons(deltaTime);
            if (refreshPending) {
                runPass(false);
                updateTarget();
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
                  << " s, generation " << generation << ", " << population << " alive, " << chunkSlots.size()
                  << " chunks, " << swapchainExtent.width << "x" << swapchainExtent.height << ", feet at "
                  << std::setprecision(2) << eye.x << " " << eye.y - EYE_HEIGHT << " " << eye.z
                  << (flying ? " (flying)" : onGround ? " (on ground)" : " (airborne)") << std::endl;
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
              << (population > MAX_INSTANCES ? " (draw capped)" : "")
              << "  |  " << chunkSlots.size() << " chunks" << (chunkLimitHit ? " (LIMIT)" : "")
              << "  |  " << SPEEDS[speedIndex] << " gen/s " << (simulationRunning ? "running" : "paused")
              << "  |  " << (flying ? "flying" : "walking") << " XYZ " << eye.x << " " << eye.y - EYE_HEIGHT << " " << eye.z
              << "  |  " << (hotbarSlot + 1) << " " << handName()
              << "  |  " << std::lround(fps) << " fps";
        glfwSetWindowTitle(windowManager->getWindow(), title.str().c_str());
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
        if (hudVisible && target.hit) {
            boxes[count++] = {glm::vec4(glm::vec3(target.block) - 0.004f, 0.03f), glm::vec4(glm::vec3(target.block) + 1.004f, 0.0f)};
        }
        if (hudVisible && target.canPlace && hotbarSlot >= 0) {
            // White outline around the whole (rotated) stamp.
            glm::ivec3 lo(std::numeric_limits<int>::max()), hi(std::numeric_limits<int>::lowest());
            for (const glm::ivec3& cell : stampCells(static_cast<Stamp>(hotbarSlot), target.place, target.normal, true)) {
                lo = glm::min(lo, cell);
                hi = glm::max(hi, cell);
            }
            boxes[count++] = {glm::vec4(glm::vec3(lo) + 0.02f, 0.03f), glm::vec4(glm::vec3(hi) + 0.98f, 2.0f)};
        }
        if (showChunkBorders) {
            for (const auto& entry : chunkSlots) {
                glm::vec3 lo(entry.first * CHUNK);
                boxes[count++] = {glm::vec4(lo, 0.08f), glm::vec4(lo + float(CHUNK), 1.0f)};
            }
        }
        if (hudVisible && tutorialPanel.active()) {
            for (const tutorial::MarkedCell& mark : tutorialPanel.lesson().marks) {
                if (count > chunkCapacity) break; // the box buffer holds chunkCapacity + 1 boxes
                glm::vec3 cell(mark.cell.x, mark.cell.y, mark.cell.z);
                float colorId = static_cast<float>(tutorial::FIRST_MARK_COLOR_ID + static_cast<int>(mark.mark));
                boxes[count++] = {glm::vec4(cell - 0.03f, 0.05f), glm::vec4(cell + 1.03f, colorId)}; // outside live blocks
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

        glm::mat4 viewProj = viewProjection();
        FrameUniforms uniforms{};
        uniforms.viewProj = viewProj;
        uniforms.invViewProj = glm::inverse(viewProj);
        uniforms.camera = glm::vec4(eye, std::chrono::duration<float>(std::chrono::steady_clock::now() - startTime).count());
        uniforms.viewport = glm::vec4(swapchainExtent.width, swapchainExtent.height, hudVisible ? 1.0f : 0.0f, float(MAX_INSTANCES));
        uniforms.hotbar = glm::ivec4(hotbarSlot, static_cast<int>(STAMP_NAMES.size()), 0, 0);
        float renderDistance = static_cast<float>(settings.renderDistance);
        uniforms.fog = glm::vec4(0.45f * renderDistance, renderDistance, 0.0f, 0.0f);
        std::memcpy(uniformBuffersMapped[currentFrame], &uniforms, sizeof(uniforms));
        uint32_t boxCount = writeBoxes(boxBuffersMapped[currentFrame]);

        std::string screenshot;
        screenshot.swap(pendingScreenshot);
        if (!screenshot.empty()) prepareCaptureBuffer();

        VkCommandBuffer cmd = commandBuffers[currentFrame];
        vkResetCommandBuffer(cmd, 0);
        recordCommandBuffer(cmd, imageIndex, boxCount, !screenshot.empty());

        VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = &imageAvailableSemaphores[currentFrame];
        submitInfo.pWaitDstStageMask = &waitStage;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &cmd;
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = &renderFinishedSemaphores[imageIndex];
        if (vkQueueSubmit(vulkanContext->getGraphicsQueue(), 1, &submitInfo, inFlightFences[currentFrame]) != VK_SUCCESS) {
            throw std::runtime_error("Failed to submit draw command buffer!");
        }

        VkPresentInfoKHR presentInfo{};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = &renderFinishedSemaphores[imageIndex];
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = &swapchain;
        presentInfo.pImageIndices = &imageIndex;
        result = vkQueuePresentKHR(vulkanContext->getPresentQueue(), &presentInfo);

        if (!screenshot.empty()) saveCapture(screenshot, currentFrame);

        framesRendered++;
        currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || framebufferResized) {
            recreateSwapchain();
        } else if (result != VK_SUCCESS) {
            throw std::runtime_error("Failed to present swap chain image!");
        }
    }

    void recordCommandBuffer(VkCommandBuffer cmd, uint32_t imageIndex, uint32_t boxCount, bool capture) {
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        if (vkBeginCommandBuffer(cmd, &beginInfo) != VK_SUCCESS) {
            throw std::runtime_error("Failed to begin recording command buffer!");
        }

        std::array<VkClearValue, 2> clearValues{};
        clearValues[1].depthStencil = {1.0f, 0};
        VkRenderPassBeginInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass = renderPass;
        renderPassInfo.framebuffer = framebuffers[imageIndex];
        renderPassInfo.renderArea.extent = swapchainExtent;
        renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
        renderPassInfo.pClearValues = clearValues.data();
        vkCmdBeginRenderPass(cmd, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

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

        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, worldPipeline);
        VkDeviceSize offset = 0;
        vkCmdBindVertexBuffers(cmd, 0, 1, &vertexBuffer, &offset);
        setMode(0);
        vkCmdDrawIndirect(cmd, indirectBuffer, 0, 1, sizeof(VkDrawIndirectCommand));
        if (boxCount > 0) {
            setMode(1);
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
            if (drawData && drawData->CmdListsCount > 0) ImGui_ImplVulkan_RenderDrawData(drawData, cmd);
        }
        vkCmdEndRenderPass(cmd);

        if (capture) recordCapture(cmd, swapchainImages[imageIndex]);

        if (vkEndCommandBuffer(cmd) != VK_SUCCESS) {
            throw std::runtime_error("Failed to record command buffer!");
        }
    }

    // ------------------------------------------------------------ screenshots

    void prepareCaptureBuffer() {
        destroyBuffer(captureBuffer, captureMemory);
        createBuffer(VkDeviceSize(swapchainExtent.width) * swapchainExtent.height * 4, VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                     {HOST_CACHED, HOST}, captureBuffer, captureMemory);
    }

    void recordCapture(VkCommandBuffer cmd, VkImage image) {
        VkImageMemoryBarrier toTransfer{};
        toTransfer.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        toTransfer.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        toTransfer.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        toTransfer.oldLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        toTransfer.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        toTransfer.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        toTransfer.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        toTransfer.image = image;
        toTransfer.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
                             0, nullptr, 0, nullptr, 1, &toTransfer);

        VkBufferImageCopy region{};
        region.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
        region.imageExtent = {swapchainExtent.width, swapchainExtent.height, 1};
        vkCmdCopyImageToBuffer(cmd, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, captureBuffer, 1, &region);

        VkImageMemoryBarrier toPresent = toTransfer;
        toPresent.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        toPresent.dstAccessMask = 0;
        toPresent.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        toPresent.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        VkBufferMemoryBarrier toHost{};
        toHost.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
        toHost.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        toHost.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
        toHost.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        toHost.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        toHost.buffer = captureBuffer;
        toHost.size = VK_WHOLE_SIZE;
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
                             VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT | VK_PIPELINE_STAGE_HOST_BIT, 0,
                             0, nullptr, 1, &toHost, 1, &toPresent);
    }

    void saveCapture(const std::string& path, size_t frame) {
        waitFence(inFlightFences[frame], "the screenshot frame");
        const uint32_t width = swapchainExtent.width, height = swapchainExtent.height;
        void* data = nullptr;
        vkMapMemory(device, captureMemory, 0, VK_WHOLE_SIZE, 0, &data);
        const uint8_t* pixels = static_cast<const uint8_t*>(data);
        bool bgr = swapchainImageFormat == VK_FORMAT_B8G8R8A8_SRGB || swapchainImageFormat == VK_FORMAT_B8G8R8A8_UNORM;
        std::vector<uint8_t> rgb(size_t(width) * height * 3);
        for (size_t i = 0; i < size_t(width) * height; ++i) {
            rgb[i * 3 + 0] = pixels[i * 4 + (bgr ? 2 : 0)];
            rgb[i * 3 + 1] = pixels[i * 4 + 1];
            rgb[i * 3 + 2] = pixels[i * 4 + (bgr ? 0 : 2)];
        }
        vkUnmapMemory(device, captureMemory);
        if (writePng(path, width, height, rgb)) std::cout << "Saved screenshot " << path << std::endl;
        else std::cerr << "Failed to write screenshot " << path << std::endl;
    }

    // ---------------------------------------------------------------- cleanup

    void destroyBuffer(VkBuffer& buffer, VkDeviceMemory& memory) {
        vkDestroyBuffer(device, buffer, nullptr);
        vkFreeMemory(device, memory, nullptr);
        buffer = VK_NULL_HANDLE;
        memory = VK_NULL_HANDLE;
    }

    void cleanup() {
        if (!vulkanContext || device == VK_NULL_HANDLE) {
            if (windowManager) windowManager->cleanup();
            return;
        }
        vkDeviceWaitIdle(device);

        destroyBuffer(captureBuffer, captureMemory);
        vkDestroyFence(device, computeFence, nullptr);
        for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            vkDestroySemaphore(device, imageAvailableSemaphores[i], nullptr);
            vkDestroyFence(device, inFlightFences[i], nullptr);
            destroyBuffer(uniformBuffers[i], uniformBuffersMemory[i]);
            destroyBuffer(boxBuffers[i], boxBuffersMemory[i]);
        }
        vkDestroyPipeline(device, computePipeline, nullptr);
        vkDestroyPipelineLayout(device, computePipelineLayout, nullptr);
        vkDestroyDescriptorPool(device, computeDescriptorPool, nullptr);
        vkDestroyDescriptorSetLayout(device, computeSetLayout, nullptr);
        for (VkPipeline pipeline : {worldPipeline, skyPipeline, gridPipeline, hudPipeline}) {
            vkDestroyPipeline(device, pipeline, nullptr);
        }
        vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
        vkDestroyDescriptorPool(device, descriptorPool, nullptr);
        vkDestroyDescriptorSetLayout(device, frameSetLayout, nullptr);
        destroyBuffer(vertexBuffer, vertexBufferMemory);
        for (int i = 0; i < 2; ++i) destroyBuffer(cellBuffers[i], cellMemory[i]);
        destroyBuffer(neighborBuffer, neighborMemory);
        destroyBuffer(activeBuffer, activeMemory);
        destroyBuffer(originBuffer, originMemory);
        destroyBuffer(statsBuffer, statsMemory);
        destroyBuffer(instanceBuffer, instanceMemory);
        destroyBuffer(indirectBuffer, indirectMemory);
        destroySwapchainResources();
        vkDestroyRenderPass(device, renderPass, nullptr);

        if (imguiReady) {
            ImGui_ImplVulkan_Shutdown();
            ImGui_ImplGlfw_Shutdown();
            ImGui::DestroyContext();
            imguiReady = false;
        }
        if (shaderManager) {
            shaderManager->cleanup();
            shaderManager.reset();
        }
        vulkanContext->cleanup();
        windowManager->cleanup();
        device = VK_NULL_HANDLE;
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

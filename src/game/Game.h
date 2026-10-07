#pragma once

// The game: owns the window and every subsystem, runs the frame loop, and holds
// the gameplay state that ties them together (the rule, the generation count,
// the hotbar, which menu is open...).
//
// The implementation is split by concern:
//   Game.cpp           setup, the frame loop, world lifecycle, edits, saves
//   GameSimulation.cpp running generations and the tick governor
//   GameInput.cpp      keyboard and mouse, the hotbar, scripted input
//   GameHud.cpp        the HUD drawn over the world, the window title
//   GameMenus.cpp      the pause, settings, inventory and new-world menus
//   GameChecks.cpp     --verify (GPU against the CPU reference) and --bench
//
// Each frame (mainLoop):
//   1. input: glfwPollEvents() runs the GLFW callbacks, which update the player,
//      the hotbar and the open screen;
//   2. movement and simulation: updateMovement(), then updateSimulation() runs
//      as many generations as the frame's time budget allows;
//   3. edits and camera moves since the last build trigger a block-list rebuild;
//   4. buildUi() lays out the HUD and menus, drawFrame() renders everything.

#include <chrono>
#include <filesystem>
#include <memory>
#include <random>
#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "imgui.h"

#include "game/CommandLine.h"
#include "game/Player.h"
#include "game/Settings.h"
#include "game/Stamps.h"
#include "game/TickGovernor.h"
#include "gpu/GpuBuffer.h"
#include "gpu/GpuContext.h"
#include "gpu/ImmediateCommands.h"
#include "life/CellTypes.h"
#include "life/LifeRules.h"
#include "render/Renderer.h"
#include "tutorial/Tutorial.h"
#include "update/Updater.h"
#include "world/ChunkWorld.h"
#include "world/SimulationPasses.h"

struct GLFWwindow;

namespace gol3d {

// What a batch of generations does besides stepping (Game::runBatch).
struct BatchOptions {
    bool animate = false;       // flag births and keep the last deaths, to animate them
    bool writeBlockList = true; // rebuild the block list; batches nobody will see skip it
    bool collectStats = true;   // chunk stats and bookkeeping; forced on when stepping
};

class Game {
public:
    Game(Options options, std::filesystem::path shaderDir, std::filesystem::path exeDir);
    ~Game();
    Game(const Game&) = delete;
    Game& operator=(const Game&) = delete;

    // Runs until the player quits (or a test run ends); returns the exit code.
    int run();

    // Set when an AppImage was updated in place and the player chose to restart.
    const std::filesystem::path& restartPath() const { return restartPath_; }

private:
    // The block list only covers chunks near the camera and around its view, so
    // it is rebuilt after the camera travels this many blocks...
    static constexpr float REBUILD_DISTANCE = 16.0f;
    // ...or turns most of the way out of the culling view, which is this much
    // wider than the camera's on every side.
    static constexpr float CULL_MARGIN_DEGREES = 20.0f;
    // Because the camera may travel REBUILD_DISTANCE before the next rebuild,
    // culling keeps blocks this far past the fog and starts this far behind
    // the camera.
    static constexpr float CULL_SLACK = 2.0f * REBUILD_DISTANCE;
    // At max speed or during a fast-forward the simulation may take at least
    // this much of each frame, whatever the "time per frame" setting says.
    static constexpr int FLAT_OUT_MIN_BUDGET_MS = 25;
    // J and Shift+J fast-forward this many generations, as do the pause menu's
    // Skip buttons (whose labels spell the numbers out).
    static constexpr uint64_t FAST_FORWARD_GENERATIONS = 100;
    static constexpr uint64_t LONG_FAST_FORWARD_GENERATIONS = 1000;

    // Which screen is up. Pause, Settings and New World freeze the world like
    // Minecraft's single-player pause; the inventory does not.
    enum class Screen { Playing, Paused, Settings, Inventory, NewWorld };

    // Why the block list is rebuilt without a generation step.
    enum class Rebuild {
        Edit,   // the player changed blocks: animate them if animations are on
        Camera, // the camera moved: keep any animation in progress
        Reset,  // a new world, a load or a tutorial scene: nothing animates
    };

    // What the next click places.
    struct Brush {
        static constexpr int EMPTY_HAND = -1;
        int hotbarSlot = 0; // a Stamp, or EMPTY_HAND: nothing to place, no outline
        CellKind material = CellKind::Life;
        int rotation = 0; // quarter turns around the placement surface (Q/E)
        int tilt = 0;     // quarter turns around the world x axis (Z/C)

        bool emptyHand() const { return hotbarSlot == EMPTY_HAND; }
    };

    // A mouse button that repeats its action while held.
    struct HeldButton {
        bool held = false;
        float sinceLastRepeat = 0.0f; // seconds
    };

    // The last change's birth/death animation.
    struct ChangeAnimation {
        double startTime = -100.0;      // glfwGetTime() seconds; long ago until the first change
        float seconds = 0.0f;           // 0 = the last change is not animated
        bool lastBuildAnimated = false; // the current block list has birth/death flags
    };

    // The New World screen's form.
    struct NewWorldForm {
        int rule = 0;
        int seed = 0;
        bool empty = false;
    };

    // ---- Game.cpp: setup and teardown
    void initWindow();
    void initGpu();
    void shutdown();
    void openStartupMenu(); // --menu
    void startUpdateCheck();

    // ---- Game.cpp: the frame loop
    void mainLoop();
    void rebuildIfOutdated(const IsSolid& isSolid);
    void drawFrame();
    FrameUniforms frameUniforms() const;
    std::vector<Box> outlineBoxes();
    void printExitSummary() const;

    // ---- Game.cpp: the world
    const LifeRule& rule() const { return lifeRules()[ruleIndex_]; }
    void resetWorld();
    // A fresh world with the current rule, its seed soup if `seeded`, and the
    // player standing back from it.
    void newWorld(bool seeded);
    void seedSoup(const glm::ivec3& minCorner, const glm::ivec3& size, float density);
    // Call before the first edit of a frame (see the definition).
    void beginEdit();
    // Puts a block of `kind` into an empty cell; never replaces what is there.
    bool placeCell(const glm::ivec3& cell, CellKind kind);
    void clearCell(const glm::ivec3& cell);
    bool saveWorld(const std::string& path);
    bool loadWorld(const std::string& path);
    void saveWorldWithMessage();
    void loadWorldWithMessage();
    // Lessons run in the normal game: the panel takes clicks until the player
    // clicks the world to look around.
    void openTutorial(size_t lesson);
    void loadTutorialScene();
    void handleTutorialRequest(tutorial::Tutorial::Request request);

    // ---- GameSimulation.cpp

    // Runs `steps` generations (at most MAX_BATCH) in one GPU submission, then
    // updates the chunk set and rebuilds the block list as `options` say.
    void runBatch(uint32_t steps, BatchOptions options = {});
    void rebuild(Rebuild reason);
    // Runs `generations` as fast as possible, in full batches (scripts, --steps).
    void advanceGenerations(uint64_t generations);
    void updateSimulation(float deltaTime);
    uint64_t generationsDue(float deltaTime);
    uint64_t runDueGenerations(uint64_t due, bool flatOut, double budgetMs,
                               std::chrono::steady_clock::time_point start);
    void settleDebt(uint64_t due, uint64_t done, bool flatOut, double now);
    void pauseAtChunkLimit();
    // One generation (N), animated at the speed of a slow tick.
    void stepOnce();
    void changeSpeed(int steps);
    void queueFastForward(uint64_t generations);
    void setRunning(bool running);
    // Forgets owed generations and any fast-forward (new worlds, loads, lessons).
    void resetSimulationClock();
    void announceRule();
    void recordPopulation();
    bool animationsEnabled() const { return settings_.animate && !capturing_; }
    glm::mat4 viewProjection() const;
    glm::mat4 cullingViewProjection() const;

    // ---- GameInput.cpp
    void installInputCallbacks();
    void onKey(int key, int action, int mods);
    void onPlayingKey(int key, int mods);
    void onMouseButton(int button, int action);
    void onCursorMove(double x, double y);
    void onScroll(double yOffset);
    void onFocusChange(bool focused);
    void updateMovement(float deltaTime);
    void updateHeldButtons(float deltaTime);
    void applyScriptAction(const ScriptAction& action);
    void placeStamp();
    void breakBlock();
    StampPlacement placementAtTarget(bool solid) const;
    void selectSlot(int hotbarSlot);
    // The world as the player's collision code sees it.
    IsSolid solidCells() const;
    int newRandomSeed();
    void rotateBrush(int quarterTurns);
    void tiltBrush(int quarterTurns);
    void cycleMaterial(int direction);
    void setCursorCaptured(bool captured);
    void toggleFullscreen();
    void openPauseMenu();
    void resumeGame();
    void openInventory();
    void closeInventory();
    void openNewWorldScreen();
    bool worldFrozen() const;
    void printControls() const;

    // ---- GameHud.cpp
    struct HudLine {
        std::string text;
        ImU32 color;
    };
    void drawHud();
    HudLine speedLine() const;
    std::vector<HudLine> debugLines() const;
    void drawStatusPanel(const std::string& title, ImU32 accent, const std::vector<HudLine>& lines);
    void drawPopulationGraph(ImDrawList* draw, ImVec2 min, ImVec2 max);
    void drawSelectedStampName(ImDrawList* draw);
    void drawToast();
    // Shows a message near the top of the screen (and prints it).
    void notify(const std::string& message);
    std::string handName() const;
    std::string handLabel() const;
    void updateWindowTitle(float deltaTime);

    // ---- GameMenus.cpp
    void initImGui();
    void shutdownImGui();
    void updateUiScale();
    void buildUi();
    void drawPauseMenu();
    void drawUpdatePanel();
    void drawSettingsMenu();
    void drawViewOptions(); // the sections of the settings menu, in order
    void drawMouseOptions();
    void drawInterfaceOptions();
    void drawSimulationOptions();
    void drawUpdateOptions();
    void drawInventory();
    void drawStampPicker();
    void drawMaterialPicker();
    void drawRulePicker();
    void drawNewWorldMenu();
    void drawSimulationControls(bool& running);
    void saveSettingsIfPersistent();

    // ---- GameChecks.cpp
    bool verifyAgainstReference();
    size_t verifyRule(uint32_t batch);
    int runBenchmark(uint64_t generations);

    // ---------------------------------------------------------------- state

    Options options_;
    std::filesystem::path shaderDir_;
    std::filesystem::path exeDir_;
    std::filesystem::path restartPath_;
    std::mt19937 rng_;
    Settings settings_;
    bool persistSettings_ = false; // false for scripted and test runs
    bool capturing_ = false;       // a run that exits after a few frames: no animations

    // Window and GPU.
    GLFWwindow* window_ = nullptr;
    bool fullscreen_ = false;
    // The window's place and size before going fullscreen, restored when leaving it.
    int windowedX_ = 100;
    int windowedY_ = 100;
    int windowedWidth_ = 1280;
    int windowedHeight_ = 800;
    GpuContext gpu_;
    BufferAllocator buffers_;
    ImmediateCommands commands_;
    Renderer renderer_;

    // The world and its simulation.
    ChunkWorld world_;
    SimulationPasses passes_;
    size_t ruleIndex_ = 0;
    uint64_t generation_ = 0;
    uint64_t population_ = 0;
    uint64_t visibleBlocks_ = 0; // in the last block list, before the draw cap
    uint64_t drawnBlocks_ = 0;
    bool refreshPending_ = false; // edits or settings changes need a new block list
    // The previous-generation buffer holds the world before this frame's edits.
    bool editOpen_ = false;
    bool blockListStale_ = false; // the last batch skipped the block list
    bool pausedAtLimit_ = false;  // paused once already at the chunk limit
    // Where the camera was when the chunks were last sorted by distance (far away
    // at first, so the first block list sorts), and when the block list was built.
    glm::vec3 lastSortEye_{1e9f};
    glm::vec3 lastBuildEye_{0.0f};
    glm::vec3 lastBuildForward_{1.0f, 0.0f, 0.0f};
    ChangeAnimation animation_;
    std::vector<float> populationHistory_; // one entry per step, for the HUD graph
    double benchCpuMs_ = 0.0;              // time in chunk bookkeeping (--bench)

    // Simulation speed and the tick governor.
    bool running_ = false;
    SimulationSpeed speed_;
    double stepDebt_ = 0.0;    // generations owed to the target speed
    uint64_t fastForward_ = 0; // generations left in a J / Shift+J fast-forward
    uint64_t fastForwardTotal_ = 0;
    double simCooldownUntil_ = 0.0; // after a frame far over budget, skip simulating until then
    RateMeter rateMeter_;           // the speed actually reached, for the HUD
    SlowdownIndicator slowdown_;    // whether the governor recently fell behind the target
    float lastSimMs_ = 0.0f;        // wall time the last frame spent simulating

    // The player and their tools.
    Player player_;
    CrosshairTarget target_;
    Brush brush_;
    HeldButton placeButton_;
    HeldButton breakButton_;
    double lastSpacePress_ = -1.0; // for double-tap Space

    // Mouse and keys.
    bool cursorCaptured_ = false;
    bool haveCursorPosition_ = false;
    double lastCursorX_ = 0.0;
    double lastCursorY_ = 0.0;
    bool f3UsedInCombo_ = false; // F3+G used: releasing F3 does not toggle the debug overlay

    // HUD and menus.
    Screen screen_ = Screen::Playing;
    Screen shownScreen_ = Screen::Playing; // the screen drawn last frame, to see when one opens
    double menuOpenedAt_ = 0.0;            // menus fade in
    bool runningBeforePause_ = false;
    bool hudVisible_ = true;
    bool showDebug_ = false;
    bool showChunkBorders_ = false;
    bool imguiReady_ = false;
    float uiScale_ = 0.0f;
    ImFont* menuFont_ = nullptr; // Karla, the default font of every menu and the HUD
    ImGuiStyle baseStyle_;       // the style at scale 1, rescaled when the GUI scale changes
    double slotNameUntil_ = 0.0; // glfwGetTime() at which the selected stamp's name is gone
    std::string toast_;
    double toastUntil_ = 0.0;
    NewWorldForm newWorldForm_;
    tutorial::Tutorial tutorial_;
    std::unique_ptr<Updater> updater_;
    bool updateAnnounced_ = false;

    // Frame timing.
    std::chrono::steady_clock::time_point startTime_ = std::chrono::steady_clock::now();
    float titleTimer_ = 0.0f;  // seconds since the window title was last refreshed
    uint32_t titleFrames_ = 0; // frames since then: together they give fps_
    float fps_ = 0.0f;
    uint64_t framesRendered_ = 0;
    float worstFrameMs_ = 0.0f;
};

} // namespace gol3d

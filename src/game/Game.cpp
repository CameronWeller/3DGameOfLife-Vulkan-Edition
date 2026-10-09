// The Game class's spine: construction from the command line, window and GPU
// setup, the frame loop, and the world's lifecycle (new worlds, edits, saves,
// tutorial scenes). See Game.h for how the class is split across files.

#include "game/Game.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <stdexcept>

#include <GLFW/glfw3.h>

#include "platform/Paths.h"
#include "world/BlockInstances.h"
#include "world/SaveFile.h"

namespace gol3d {
namespace {

// Toward the sun (FrameUniforms::sun): high in the sky and off to one side.
const glm::vec3 SUN_DIRECTION = glm::normalize(glm::vec3(0.45f, 0.80f, 0.30f));

// The first frames include one-off start-up work, so the "worst frame" in the
// exit summary only counts frames after this many have been rendered.
constexpr uint64_t UNTIMED_STARTUP_FRAMES = 2;
// A longer frame (a breakpoint, a dragged window) is treated as this long, so a
// stall does not fling the player or pile up owed generations.
constexpr float MAX_FRAME_SECONDS = 0.25f;
// The block list is rebuilt once the camera turns through this fraction of the
// culling view's extra margin (CULL_MARGIN_DEGREES).
constexpr float REBUILD_TURN_FRACTION = 0.75f;
// Fog thickens from this fraction of the render distance to the full distance.
constexpr float FOG_START_FRACTION = 0.45f;

// The outline color of a placement preview in `material`.
BoxColor placementColor(CellKind material) {
    if (material == CellKind::Stone) return BoxColor::PlaceStone;
    if (material == CellKind::Ember) return BoxColor::PlaceEmber;
    return BoxColor::PlaceLife;
}

} // namespace

// ------------------------------------------------------- setup and teardown

Game::Game(Options options, std::filesystem::path shaderDir, std::filesystem::path exeDir)
    : options_(std::move(options)),
      shaderDir_(std::move(shaderDir)),
      exeDir_(std::move(exeDir)),
      rng_(options_.seed) {
    ruleIndex_ = options_.rule;
    running_ = options_.run;
    speed_.setExponent(options_.speedExponent);
    showChunkBorders_ = options_.chunkBorders;
    showDebug_ = options_.debugOverlay;
    hudVisible_ = !options_.hideHud;
    // Scripted and test runs use the default settings, so their results do not
    // depend on the player's options file, and never write it.
    persistSettings_ = options_.script.empty() && options_.screenshotPath.empty() &&
                       !options_.verify && options_.menu.empty() && !options_.benchGenerations;
    if (persistSettings_) settings_ = loadSettings(settingsFilePath());
    if (options_.renderDistance) settings_.renderDistance = options_.renderDistance;
    // Screenshots of scripted runs show finished states, not cells mid-animation.
    capturing_ = options_.exitAfterFrames != 0;
}

Game::~Game() {
    shutdown();
}

int Game::run() {
    initWindow();
    initGpu();
    if (options_.verify) return verifyAgainstReference() ? 0 : 1;

    initImGui();
    newWorld(!options_.empty);
    if (!options_.loadPath.empty() && !loadWorld(options_.loadPath)) return 1;
    player_.flying = options_.fly;
    advanceGenerations(options_.warmupSteps);
    if (options_.benchGenerations) return runBenchmark(options_.benchGenerations);

    for (const ScriptAction& action : options_.script) {
        applyScriptAction(action);
    }
    printControls();
    bool interactive =
        options_.script.empty() && options_.screenshotPath.empty() && options_.menu.empty();
    setCursorCaptured(interactive);
    openStartupMenu();
    startUpdateCheck();

    mainLoop();

    saveSettingsIfPersistent();
    if (!options_.savePath.empty() && !saveWorld(options_.savePath)) return 1;
    return 0;
}

void Game::initWindow() {
    if (!glfwInit()) throw std::runtime_error("Could not open a window (GLFW failed to start).");
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API); // Vulkan, not OpenGL
    window_ =
        glfwCreateWindow(windowedWidth_, windowedHeight_, "3D Game of Life", nullptr, nullptr);
    if (!window_) throw std::runtime_error("Could not open a window.");
    installInputCallbacks();
    // Unaccelerated mouse motion for looking around, where the platform has it.
    if (glfwRawMouseMotionSupported()) glfwSetInputMode(window_, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
}

void Game::initGpu() {
    gpu_.init(window_);
    std::cout << "GPU: " << gpu_.properties.deviceName << " (Vulkan "
              << VK_API_VERSION_MAJOR(gpu_.properties.apiVersion) << "."
              << VK_API_VERSION_MINOR(gpu_.properties.apiVersion) << ")" << std::endl;
    buffers_.init(gpu_.device, gpu_.memoryProperties);
    commands_.init(gpu_);
    renderer_.initSwapchain(gpu_, buffers_, window_);

    world_.onWarning = [this](const std::string& message) { notify(message); };
    world_.onBuffersReplaced = [this] {
        passes_.writeDescriptors(world_);
        renderer_.writeDescriptors(passes_.instanceBuffer(), world_.pool().origins);
    };
    world_.init(gpu_, buffers_, commands_, options_.chunkLimit);
    passes_.init(gpu_, buffers_, commands_, shaderDir_, world_);
    renderer_.initPipelines(shaderDir_, passes_.instanceBuffer(), passes_.indirectBuffer(),
                            world_.pool().origins);
    resetWorld();
}

void Game::shutdown() {
    if (gpu_.device != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(gpu_.device);
        shutdownImGui();
        renderer_.destroy();
        passes_.destroy();
        world_.destroy();
        commands_.destroy();
    }
    gpu_.destroy();
    if (window_) glfwDestroyWindow(window_);
    window_ = nullptr;
    glfwTerminate();
}

void Game::openStartupMenu() {
    const std::string& menu = options_.menu;
    if (menu.empty()) return;
    if (menu == "pause") {
        openPauseMenu();
    } else if (menu == "settings") {
        openPauseMenu();
        screen_ = Screen::Settings;
    } else if (menu == "inventory") {
        openInventory();
    } else if (menu == "newworld") {
        openPauseMenu();
        openNewWorldScreen();
    } else if (menu.starts_with("tutorial")) {
        // --menu tutorial:N opens lesson N (counting from 1; plain "tutorial" opens
        // the first); --steps then advances the lesson's scene.
        const std::string prefix = "tutorial:";
        size_t lesson = 0;
        if (menu.size() > prefix.size()) lesson = std::stoul(menu.substr(prefix.size())) - 1;
        openTutorial(lesson);
        advanceGenerations(options_.warmupSteps);
    } else {
        throw std::runtime_error("Unknown --menu " + menu);
    }
}

void Game::startUpdateCheck() {
    bool testFeed = !options_.updateFeed.empty();
    if (!testFeed && !(persistSettings_ && settings_.checkUpdates)) return;
    updater_ = std::make_unique<Updater>(GOL3D_VERSION_STRING, exeDir_,
                                         testFeed ? options_.updateFeed : RELEASES_API_LATEST);
    updater_->checkAsync();
}

// --------------------------------------------------------- the frame loop

void Game::mainLoop() {
    auto lastTime = std::chrono::steady_clock::now();
    const IsSolid isSolid = solidCells();
    while (!glfwWindowShouldClose(window_)) {
        glfwPollEvents();
        auto now = std::chrono::steady_clock::now();
        float frameSeconds = std::chrono::duration<float>(now - lastTime).count();
        lastTime = now;
        if (framesRendered_ > UNTIMED_STARTUP_FRAMES) {
            worstFrameMs_ = std::max(worstFrameMs_, frameSeconds * 1000.0f);
        }
        float deltaTime = std::min(frameSeconds, MAX_FRAME_SECONDS);

        if (!worldFrozen()) {
            updateMovement(deltaTime);
            updateSimulation(deltaTime);
        }
        target_ = player_.aim(isSolid);
        if (screen_ == Screen::Playing) updateHeldButtons(deltaTime);
        rebuildIfOutdated(isSolid);

        const bool lastFrame =
            options_.exitAfterFrames && framesRendered_ + 1 == options_.exitAfterFrames;
        if (lastFrame && !options_.screenshotPath.empty()) {
            renderer_.requestScreenshot(options_.screenshotPath);
        }
        buildUi();
        drawFrame();
        updateWindowTitle(deltaTime);
        if (options_.exitAfterFrames && framesRendered_ >= options_.exitAfterFrames) break;
    }
    vkDeviceWaitIdle(gpu_.device);
    printExitSummary();
}

// The block list only covers what the camera could see when it was built;
// rebuilds it after edits and once the camera has moved or turned far enough
// to uncover blocks it left out.
void Game::rebuildIfOutdated(const IsSolid& isSolid) {
    if (refreshPending_) {
        rebuild(Rebuild::Edit);
        target_ = player_.aim(isSolid); // the edit may have changed what the crosshair hits
        return;
    }
    bool movedFar = glm::distance(player_.eye, lastBuildEye_) > REBUILD_DISTANCE;
    bool turnedFar = glm::dot(player_.forward(), lastBuildForward_) <
                     std::cos(glm::radians(REBUILD_TURN_FRACTION * CULL_MARGIN_DEGREES));
    if (movedFar || turnedFar) rebuild(Rebuild::Camera);
}

void Game::drawFrame() {
    if (!renderer_.beginFrame()) return;
    renderer_.endFrame(frameUniforms(), outlineBoxes(), imguiReady_);
    framesRendered_++;
}

FrameUniforms Game::frameUniforms() const {
    const double now = glfwGetTime();
    const VkExtent2D extent = renderer_.swapchain().extent();
    const glm::mat4 worldToClip = viewProjection();
    const float secondsSinceStart =
        std::chrono::duration<float>(std::chrono::steady_clock::now() - startTime_).count();
    const float renderDistance = static_cast<float>(settings_.renderDistance);
    float animationProgress = 1.0f;
    if (animation_.seconds > 0.0f) {
        double progress = (now - animation_.startTime) / animation_.seconds;
        animationProgress = static_cast<float>(std::clamp(progress, 0.0, 1.0));
    }

    FrameUniforms uniforms{};
    uniforms.viewProjection = worldToClip;
    uniforms.inverseViewProjection = glm::inverse(worldToClip);
    uniforms.camera = glm::vec4(player_.eye, secondsSinceStart);
    uniforms.viewport =
        glm::vec4(extent.width, extent.height, hudVisible_ ? 1.0f : 0.0f,
                  static_cast<float>(std::min<uint64_t>(drawnBlocks_, MAX_BLOCK_INSTANCES)));
    uniforms.hotbar =
        glm::ivec4(brush_.hotbarSlot, STAMP_COUNT, static_cast<int>(brush_.material), 0);
    uniforms.fog = glm::vec4(FOG_START_FRACTION * renderDistance, renderDistance, 0.0f, 0.0f);
    uniforms.anim =
        glm::vec4(animationProgress, settings_.smoothLighting ? 1.0f : 0.0f, 0.0f, 0.0f);
    uniforms.sun = glm::vec4(SUN_DIRECTION, 0.0f);
    return uniforms;
}

// The wireframe boxes drawn this frame, in world units: the targeted block, the
// placement preview, tutorial marks and chunk borders. Each box is padded a
// little in or out of its cells so its edges do not z-fight with block faces.
std::vector<Box> Game::outlineBoxes() {
    std::vector<Box> boxes;
    auto add = [&](const glm::vec3& min, const glm::vec3& max, float thickness, float colorId) {
        if (boxes.size() < Renderer::MAX_BOXES) {
            boxes.push_back({glm::vec4(min, thickness), glm::vec4(max, colorId)});
        }
    };
    auto colorIdOf = [](BoxColor color) { return static_cast<float>(color); };

    if (hudVisible_ && target_.hit) {
        glm::vec3 block(target_.block);
        add(block - 0.004f, block + 1.004f, 0.03f, colorIdOf(BoxColor::Target));
    }
    if (hudVisible_ && target_.canPlace && !brush_.emptyHand()) {
        // Around the whole (rotated) stamp, just inside its cells, in the material's color.
        CellBounds bounds = boundsOf(stampCells(placementAtTarget(true), rule(), rng_));
        add(glm::vec3(bounds.min) + 0.02f, glm::vec3(bounds.max) + 0.98f, 0.03f,
            colorIdOf(placementColor(brush_.material)));
    }
    if (hudVisible_ && tutorial_.active()) {
        for (const tutorial::MarkedCell& mark : tutorial_.lesson().marks) {
            glm::vec3 cell(mark.cell.x, mark.cell.y, mark.cell.z);
            float markColor =
                static_cast<float>(tutorial::FIRST_MARK_COLOR_ID + static_cast<int>(mark.mark));
            add(cell - 0.03f, cell + 1.03f, 0.05f, markColor); // just outside live blocks
        }
    }
    if (showChunkBorders_) {
        // The chunks around the player; a large world has far more than fit.
        const float reach = 4.0f * CHUNK_SIZE;
        world_.forEachChunk([&](const glm::ivec3& chunk) {
            glm::vec3 low(chunk * CHUNK_SIZE);
            glm::vec3 high = low + static_cast<float>(CHUNK_SIZE);
            glm::vec3 nearest = glm::clamp(player_.eye, low, high);
            if (glm::distance(nearest, player_.eye) > reach) return;
            add(low, high, 0.08f, colorIdOf(BoxColor::ChunkBorder));
        });
    }
    return boxes;
}

// Where the run ended and how fast it went, printed when the game closes.
void Game::printExitSummary() const {
    const VkExtent2D extent = renderer_.swapchain().extent();
    const glm::vec3 feet = player_.feet();
    float seconds =
        std::chrono::duration<float>(std::chrono::steady_clock::now() - startTime_).count();
    const char* stance =
        player_.flying ? " (flying)" : (player_.onGround ? " (on ground)" : " (airborne)");
    std::cout << "Exit: " << framesRendered_ << " frames in " << std::fixed << std::setprecision(1)
              << seconds << " s, generation " << generation_ << ", " << population_ << " alive, "
              << world_.activeChunkCount() << " chunks, " << extent.width << "x" << extent.height
              << ", feet at " << std::setprecision(2) << feet.x << " " << feet.y << " " << feet.z
              << stance << std::endl;
    if (running_ || generation_ > 0) {
        const PassCosts& costs = passes_.costs();
        std::cout << "Speed: target " << speed_.label() << ", reached " << std::setprecision(1)
                  << rateMeter_.rate() << " gen/s"
                  << (slowdown_.active() ? " (slowed to keep up)" : "") << ", "
                  << std::setprecision(2) << costs.stepMs << " ms/gen + " << costs.drawMs
                  << " ms/block list on the GPU, worst frame " << std::setprecision(1)
                  << worstFrameMs_ << " ms" << std::endl;
    }
}

// --------------------------------------------------------------- the world

void Game::resetWorld() {
    cancelSlicing();
    world_.reset();
    generationsSinceStats_ = 0;
    pausedAtLimit_ = false;
    editOpen_ = false;
    populationHistory_.clear();
}

void Game::newWorld(bool seeded) {
    tutorial_.close(); // its lesson no longer matches the world
    resetWorld();
    generation_ = 0;
    population_ = 0;
    resetSimulationClock();
    const int size = rule().seedSize;
    const int depth = rule().seedDepth > 0 ? rule().seedDepth : size;
    // The seed soup rests on the ground (y = 0), like a structure in a superflat world.
    if (seeded) {
        seedSoup(glm::ivec3(-size / 2, 0, -depth / 2), glm::ivec3(size, size, depth),
                 rule().seedDensity);
    }
    rebuild(Rebuild::Reset);

    // Spawn on the ground at a distance, looking at a point 40% of the way up
    // the soup: from an angle, or face on when it is a single layer. Both
    // directions are unit length, so `distance` is the distance from the
    // soup's vertical axis.
    float distance = std::max(20.0f, 1.6f * static_cast<float>(size));
    const glm::vec2 fromSeed = depth < size ? glm::vec2(0.0f, 1.0f) : glm::vec2(0.6f, 0.8f);
    player_.eye = glm::vec3(fromSeed.x * distance, Player::EYE_HEIGHT, fromSeed.y * distance);
    const glm::vec3 lookTarget(0.0f, 0.4f * static_cast<float>(size), 0.0f);
    glm::vec3 toSeed = glm::normalize(lookTarget - player_.eye);
    player_.yaw = glm::degrees(std::atan2(toSeed.z, toSeed.x));
    player_.pitch = glm::degrees(std::asin(toSeed.y));
    player_.flying = false;
    player_.verticalSpeed = 0.0f;
}

void Game::seedSoup(const glm::ivec3& minCorner, const glm::ivec3& size, float density) {
    std::uniform_real_distribution<float> chance(0.0f, 1.0f);
    for (int z = 0; z < size.z; ++z) {
        for (int y = 0; y < size.y; ++y) {
            for (int x = 0; x < size.x; ++x) {
                if (chance(rng_) < density) world_.setLife(minCorner + glm::ivec3(x, y, z), true);
            }
        }
    }
    refreshPending_ = true;
}

// Before the first edit of a frame, copies the world into the previous-generation
// buffer, so the next block list sees placed cells as births (they grow in) and
// removed ones as deaths (they shrink away).
void Game::beginEdit() {
    settleSlicing(); // edits go into the newest generation
    refreshPending_ = true;
    if (editOpen_ || !animationsEnabled()) return;
    editOpen_ = true;
    passes_.finishAsync(); // a slice still running writes the buffer the copy goes into
    world_.copyCurrentToPrevious();
}

bool Game::placeCell(const glm::ivec3& cell, CellKind kind) {
    if (world_.isOccupied(cell)) return false;
    beginEdit();
    if (kind == CellKind::Life) {
        world_.setLife(cell, true);
        return true;
    }
    return world_.placeBlock(cell, kind);
}

void Game::clearCell(const glm::ivec3& cell) {
    std::optional<CellKind> kind = world_.cellKind(cell);
    if (!kind) return;
    beginEdit();
    if (*kind == CellKind::Life) {
        world_.setLife(cell, false);
    } else {
        world_.removeBlock(cell);
    }
}

bool Game::saveWorld(const std::string& path) {
    SavedWorld saved;
    saved.ruleIndex = static_cast<uint32_t>(ruleIndex_);
    saved.generation = generation_;
    saved.eye = player_.eye;
    saved.yaw = player_.yaw;
    saved.pitch = player_.pitch;
    world_.forEachCell([&](const glm::ivec3& cell, CellKind kind) {
        if (kind == CellKind::Life) {
            saved.cells.push_back(cell);
        } else {
            saved.blocks.push_back({cell, static_cast<int32_t>(kind)});
        }
    });
    if (!writeSaveFile(path, saved)) {
        std::cerr << "Could not save " << path << std::endl;
        return false;
    }
    std::cout << "Saved " << saved.cells.size() << " cells and " << saved.blocks.size()
              << " blocks to " << std::filesystem::absolute(path).string() << std::endl;
    return true;
}

bool Game::loadWorld(const std::string& path) {
    SavedWorld saved;
    switch (readSaveFile(path, lifeRules().size(), saved)) {
        case SaveFileStatus::NotASave:
            std::cerr << "Not a 3D Life save: " << path << std::endl;
            return false;
        case SaveFileStatus::Truncated:
            std::cerr << "Truncated save: " << path << std::endl;
            return false;
        case SaveFileStatus::Ok:
            break;
    }

    tutorial_.close();
    resetWorld();
    for (const SavedBlock& block : saved.blocks) {
        // Skip Life (kind 0, stored with the cells) and kinds from newer versions.
        bool knownBlockKind = block.kind > 0 && block.kind < static_cast<int>(cellTypes().size());
        if (knownBlockKind) placeCell(block.cell, static_cast<CellKind>(block.kind));
    }
    for (const glm::ivec3& cell : saved.cells) {
        world_.setLife(cell, true);
    }
    ruleIndex_ = saved.ruleIndex;
    generation_ = saved.generation;
    player_.eye = saved.eye;
    player_.yaw = saved.yaw;
    player_.pitch = saved.pitch;
    player_.verticalSpeed = 0.0f;
    resetSimulationClock();
    rebuild(Rebuild::Reset);
    std::cout << "Loaded " << population_ << " cells and " << saved.blocks.size() << " blocks from "
              << path
              << (world_.limitReached() ? " (chunk limit reached; some cells were dropped)" : "")
              << std::endl;
    return true;
}

void Game::saveWorldWithMessage() {
    if (saveWorld(saveFilePath())) {
        notify("Saved world to " + saveFilePath());
    } else {
        notify("Could not save the world");
    }
}

void Game::loadWorldWithMessage() {
    if (loadWorld(saveFilePath())) {
        notify("Loaded " + saveFilePath());
    } else {
        notify("Could not load " + saveFilePath());
    }
}

// ---------------------------------------------------------------- tutorial

void Game::openTutorial(size_t lesson) {
    tutorial_.open(lesson);
    screen_ = Screen::Playing;
    setCursorCaptured(false);
    loadTutorialScene();
}

void Game::handleTutorialRequest(tutorial::Tutorial::Request request) {
    if (request == tutorial::Tutorial::Request::LoadScene) loadTutorialScene();
}

// Clears the world and sets up the lesson's rule, cells and camera, paused.
void Game::loadTutorialScene() {
    const tutorial::Lesson& lesson = tutorial_.lesson();
    ruleIndex_ = lesson.rule;
    resetWorld();
    generation_ = 0;
    for (const PatternCell& cell : lesson.cells) {
        world_.setLife(glm::ivec3(cell.x, cell.y, cell.z), true);
    }
    rebuild(Rebuild::Reset);
    player_.eye = lesson.eye;
    tutorial::lookAngles(lesson, player_.yaw, player_.pitch);
    player_.flying = true;
    player_.verticalSpeed = 0.0f;
    running_ = false;
    runningBeforePause_ = false;
    resetSimulationClock();
    brush_.material = CellKind::Life;
    brush_.hotbarSlot = Brush::EMPTY_HAND; // keeps the placement outline out of the scene
}

} // namespace gol3d

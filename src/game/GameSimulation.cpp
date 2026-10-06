// Running generations: batches on the GPU, the block-list rebuilds that follow
// edits and camera moves, and the tick governor that fits it all into frames.

#include <algorithm>
#include <cmath>

#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>

#include "game/Game.h"
#include "util/Units.h"
#include "world/BlockInstances.h"

static_assert(GLM_CONFIG_CLIP_CONTROL & GLM_CLIP_CONTROL_ZO_BIT,
              "Define GLM_FORCE_DEPTH_ZERO_TO_ONE for this target: Vulkan depth is 0..1");

namespace gol3d {
namespace {

constexpr float EDIT_ANIMATION_SECONDS = 0.16f; // placed blocks grow in, removed ones shrink
constexpr size_t POPULATION_HISTORY = 240;      // generations shown in the HUD graph
constexpr float NEAR_PLANE = 0.05f;
constexpr float FAR_PLANE = 1000.0f;         // fog ends well before this
constexpr float CULLING_FAR_PLANE = 4096.0f; // beyond the largest render distance

// Speeds up to this many generations per second animate every generation's
// births and deaths, over STEP_ANIMATION_SECONDS or 3/4 of a tick if shorter.
constexpr double MAX_ANIMATED_RATE = 16.0;
constexpr double STEP_ANIMATION_SECONDS = 0.22;
constexpr double STEP_ANIMATION_TICK_FRACTION = 0.75;

// At unlimited speed the debt is "everything": far more than any frame can run.
constexpr double UNLIMITED_DEBT = 1e12;

// A frame whose simulation took more than this times the budget is followed by
// idle time, so frames get at least half of the time.
constexpr double OVERRUN_FACTOR = 1.5;

} // namespace

// ------------------------------------------------------------------ batches

void Game::runBatch(uint32_t steps, BatchOptions options) {
    // Every step changes the world, so chunk bookkeeping must follow it.
    const bool collectStats = options.collectStats || steps > 0;
    const bool writeBlockList = options.writeBlockList;
    if (writeBlockList && glm::distance(player_.eye, lastSortEye_) > REBUILD_DISTANCE) {
        world_.sortByDistance(player_.eye);
        lastSortEye_ = player_.eye;
    }

    BatchRequest request;
    request.steps = steps;
    request.animate = options.animate;
    request.writeBlockList = writeBlockList;
    request.collectStats = collectStats;
    request.rule = &rule();
    request.cullViewProjection = cullingViewProjection();
    request.eye = player_.eye;
    // A little past the fog, so blocks do not pop in before the next rebuild.
    request.cullDistance = static_cast<float>(settings_.renderDistance) + 2.0f * REBUILD_DISTANCE;
    passes_.run(request, world_);

    generation_ += steps;
    editOpen_ = false;
    blockListStale_ = !writeBlockList;
    if (writeBlockList) {
        visibleBlocks_ = passes_.visibleBlocks();
        drawnBlocks_ = std::min<uint64_t>(visibleBlocks_, MAX_BLOCK_INSTANCES);
        refreshPending_ = false;
        lastBuildEye_ = player_.eye;
        lastBuildForward_ = player_.forward();
        animation_.lastBuildAnimated = options.animate;
    }
    if (!collectStats) return;

    const auto bookkeepingStart = std::chrono::steady_clock::now();
    population_ = world_.maintain();
    benchCpuMs_ += millisecondsSince(bookkeepingStart);
    if (steps > 0) recordPopulation();
}

void Game::rebuild(Rebuild reason) {
    const double now = glfwGetTime();
    bool animate = false;
    if (reason == Rebuild::Edit && editOpen_) {
        animation_.startTime = now;
        animation_.seconds = EDIT_ANIMATION_SECONDS;
        animate = true;
    } else if (reason == Rebuild::Camera) {
        animate = animation_.lastBuildAnimated && now < animation_.startTime + animation_.seconds;
    } else {
        animation_.seconds = 0.0f;
    }
    // A camera move changes what is visible, not the world: skip the stats.
    runBatch(0, {.animate = animate, .collectStats = reason != Rebuild::Camera});
}

void Game::recordPopulation() {
    populationHistory_.push_back(static_cast<float>(population_));
    if (populationHistory_.size() > POPULATION_HISTORY) {
        populationHistory_.erase(populationHistory_.begin());
    }
}

void Game::advanceGenerations(uint64_t n) {
    while (n > 0) {
        uint32_t batch = static_cast<uint32_t>(std::min<uint64_t>(n, MAX_BATCH));
        n -= batch;
        bool lastBatch = n == 0;
        runBatch(batch, {.writeBlockList = lastBatch}); // only the last block list is ever seen
    }
}

// ----------------------------------------------------------- the governor

// Each frame the simulation owes `rate * dt` generations (plus any
// fast-forward) and may spend at most the frame budget paying them, sizing
// batches by the measured cost of a generation. When the world is too big to
// keep up, the backlog is dropped, so the tick rate falls instead of the frame
// rate, and the HUD shows the speed actually reached. A single generation that
// blows the budget is followed by enough idle frames to keep the game at least
// half responsive.
void Game::updateSimulation(float deltaTime) {
    const double now = glfwGetTime();
    // Edits since the last pass must reach chunk bookkeeping before any step,
    // or births next to new cells could fall into chunks that don't exist yet.
    if (refreshPending_) rebuild(Rebuild::Edit);

    const uint64_t due = generationsDue(deltaTime);
    rateMeter_.sample(now, generation_);
    if (due == 0) return;
    if (now < simCooldownUntil_) {
        slowdown_.note(now, true);
        return;
    }

    // Flat out (max speed or fast-forward) the game may give up some frame rate.
    const bool flatOut = speed_.unlimited() || fastForward_ > 0;
    const double budgetMs =
        flatOut ? std::max(settings_.simBudget, FLAT_OUT_MIN_BUDGET_MS) : settings_.simBudget;
    // The GPU runs frames and the simulation in order; let the last frame
    // finish first so its drawing is not counted as simulation time.
    renderer_.waitForPreviousFrame();
    const auto start = std::chrono::steady_clock::now();

    const uint64_t done = runDueGenerations(due, flatOut, budgetMs, start);
    settleDebt(due, done, flatOut, now);

    lastSimMs_ = static_cast<float>(millisecondsSince(start));
    if (lastSimMs_ > OVERRUN_FACTOR * budgetMs) simCooldownUntil_ = now + lastSimMs_ / 1000.0;
}

// Adds this frame's share to the running debt and returns the generations owed
// now: the whole debt plus any fast-forward.
uint64_t Game::generationsDue(float deltaTime) {
    uint64_t due = fastForward_;
    if (running_) {
        stepDebt_ =
            speed_.unlimited() ? UNLIMITED_DEBT : stepDebt_ + deltaTime * speed_.targetRate();
        due += static_cast<uint64_t>(std::min(stepDebt_, UNLIMITED_DEBT));
    }
    return due;
}

// Runs up to `due` generations within the budget, then builds the block list
// the frame will show. Returns how many generations ran.
uint64_t Game::runDueGenerations(uint64_t due, bool flatOut, double budgetMs,
                                 std::chrono::steady_clock::time_point start) {
    // Steps run in batches without a block list; one build at the end draws the
    // result, so its cost is set aside first.
    const double reserveMs = passes_.costs().drawMs + passes_.costs().overheadMs;

    // At slow speeds a single generation's births and deaths are animated over
    // most of the tick. That generation draws in its own batch, before chunk
    // bookkeeping frees chunks whose last cells are still shrinking away.
    const bool animate =
        animationsEnabled() && !flatOut && due == 1 && speed_.targetRate() <= MAX_ANIMATED_RATE;
    uint64_t done = 0;
    if (animate) {
        runBatch(1, {.animate = true});
        done = 1;
    }
    while (done < due) {
        uint32_t batch = planBatch(due - done, done, millisecondsSince(start), budgetMs, reserveMs,
                                   passes_.costs(), MAX_BATCH);
        if (batch == 0) break;
        runBatch(batch, {.writeBlockList = false});
        done += batch;
        if (world_.limitReached() && !pausedAtLimit_) {
            pauseAtChunkLimit();
            break;
        }
    }

    if (animate) {
        animation_.startTime = glfwGetTime();
        animation_.seconds = static_cast<float>(
            std::min(STEP_ANIMATION_SECONDS, STEP_ANIMATION_TICK_FRACTION / speed_.targetRate()));
    } else {
        runBatch(0, {.collectStats = false}); // the block list for what this frame shows
        animation_.seconds = 0.0f;
    }
    return done;
}

// Past the chunk limit, growth freezes at the edge of the world; pause once and
// say so instead.
void Game::pauseAtChunkLimit() {
    pausedAtLimit_ = true;
    running_ = false;
    fastForward_ = 0;
    notify("Chunk limit of " + std::to_string(world_.chunkLimit()) + " reached at generation " +
           std::to_string(generation_) +
           ": paused. G continues (growth stops at the edge); --chunks N raises the limit.");
}

// Pays for the generations that ran: the fast-forward first, then the running
// debt. When the frame fell behind, the rest of the debt is forgiven.
void Game::settleDebt(uint64_t due, uint64_t done, bool flatOut, double now) {
    const uint64_t paidToFastForward = std::min(done, fastForward_);
    fastForward_ -= paidToFastForward;
    if (fastForward_ == 0) fastForwardTotal_ = 0;
    if (running_) {
        stepDebt_ = std::max(0.0, stepDebt_ - static_cast<double>(done - paidToFastForward));
    }
    const bool behind = done < due && !flatOut;
    if (behind) stepDebt_ = std::min(stepDebt_, 1.0); // slow down instead of spiraling
    if (speed_.unlimited()) stepDebt_ = 0.0;
    slowdown_.note(now, behind);
}

void Game::stepOnce() {
    if (refreshPending_) rebuild(Rebuild::Edit); // see updateSimulation
    bool animate = animationsEnabled();
    runBatch(1, {.animate = animate});
    animation_.startTime = glfwGetTime();
    animation_.seconds = animate ? static_cast<float>(STEP_ANIMATION_SECONDS) : 0.0f;
}

void Game::changeSpeed(int steps) {
    speed_.change(steps);
    stepDebt_ = std::min(stepDebt_, 1.0);
    notify("Speed: " + (speed_.unlimited() ? std::string("as fast as possible") : speed_.label()));
}

void Game::queueFastForward(uint64_t generations) {
    if (fastForward_ > 0) {
        fastForward_ = 0;
        fastForwardTotal_ = 0;
        notify("Fast-forward cancelled");
        return;
    }
    fastForward_ = generations;
    fastForwardTotal_ = generations;
    notify("Fast-forward " + std::to_string(generations) + " generations (J to cancel)");
}

void Game::setRunning(bool running) {
    running_ = running;
    stepDebt_ = 0.0;
}

void Game::resetSimulationClock() {
    stepDebt_ = 0.0;
    fastForward_ = 0;
    fastForwardTotal_ = 0;
}

void Game::announceRule() {
    notify(std::string("Rule: ") + rule().name + " " + describeRule(rule()));
}

// --------------------------------------------------------------- the camera

glm::mat4 Game::viewProjection() const {
    const float aspect = renderer_.swapchain().aspectRatio();
    glm::mat4 projection =
        glm::perspective(glm::radians(settings_.fov), aspect, NEAR_PLANE, FAR_PLANE);
    projection[1][1] *= -1; // Vulkan's clip space has y pointing down
    const glm::vec3 forward = player_.forward();
    return projection * glm::lookAt(player_.eye, player_.eye + forward, glm::vec3(0, 1, 0));
}

// The view the block list is culled to: CULL_MARGIN_DEGREES wider than the
// camera's on every side, and starting a little behind it, so turning a little
// or stepping back does not uncover missing blocks before the next rebuild.
glm::mat4 Game::cullingViewProjection() const {
    const float aspect = renderer_.swapchain().aspectRatio();
    const float maxHalfAngle = glm::radians(85.0f);
    const float halfY = glm::radians(settings_.fov * 0.5f);
    const float halfX = std::atan(std::tan(halfY) * aspect);
    const float wideY = std::min(halfY + glm::radians(CULL_MARGIN_DEGREES), maxHalfAngle);
    const float wideX = std::min(halfX + glm::radians(CULL_MARGIN_DEGREES), maxHalfAngle);
    glm::mat4 projection = glm::perspective(2.0f * wideY, std::tan(wideX) / std::tan(wideY),
                                            NEAR_PLANE, CULLING_FAR_PLANE);
    const glm::vec3 forward = player_.forward();
    const glm::vec3 back = player_.eye - forward * (2.0f * REBUILD_DISTANCE);
    return projection * glm::lookAt(back, back + forward, glm::vec3(0, 1, 0));
}

} // namespace gol3d

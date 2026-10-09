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
const glm::vec3 WORLD_UP(0.0f, 1.0f, 0.0f);
// The culling view is never wider than this on either side of its center, so
// the widened projection stays finite (tan(90 degrees) is infinite).
constexpr float MAX_CULL_HALF_ANGLE_DEGREES = 85.0f;

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

// The smallest slice of a sliced generation, so even a frame with no budget
// left makes some progress: this many chunks, or enough that a generation
// takes at most MAX_SLICES frames.
constexpr uint32_t MIN_SLICE_CHUNKS = 64;
constexpr uint32_t MAX_SLICES = 32;
constexpr double MIN_STEP_MS = 1e-6; // a step never reads as free, so slices stay finite

} // namespace

// ------------------------------------------------------------------ batches

void Game::runBatch(uint32_t steps, BatchOptions options) {
    settleSlicing(); // sliced work from the last frame lands first
    // Life may spread past the chunks that exist once MAX_BATCH generations ran
    // since the last stats: collect them first.
    if (steps > 0 && generationsSinceStats_ + steps > MAX_BATCH) {
        runBatch(0, {.writeBlockList = false, .collectStats = true});
    }
    passes_.run(buildRequest(steps, options), world_);

    generation_ += steps;
    if (!options.collectStats) generationsSinceStats_ += steps;
    editOpen_ = false;
    if (steps > 0 || options.writeBlockList) blockListStale_ = !options.writeBlockList;
    if (options.writeBlockList) noteBlockListBuilt(options.animate);
    if (options.collectStats) collectChunkStats(steps);
}

// The pass request for a batch: `steps` generations, then a build as `options`
// say, culled to the current view.
BatchRequest Game::buildRequest(uint32_t steps, BatchOptions options) {
    // Sorting reorders the active list, which a sliced generation walks in order.
    const bool maySort = sliced_.phase == SlicePhase::Idle;
    if (options.writeBlockList && maySort &&
        glm::distance(player_.eye, lastSortEye_) > REBUILD_DISTANCE) {
        world_.sortByDistance(player_.eye);
        lastSortEye_ = player_.eye;
    }
    BatchRequest request;
    request.steps = steps;
    request.animate = options.animate;
    request.writeBlockList = options.writeBlockList;
    request.collectStats = options.collectStats;
    request.rule = &rule();
    request.cullViewProjection = cullingViewProjection();
    request.eye = player_.eye;
    // A little past the fog, so blocks do not pop in before the next rebuild.
    request.cullDistance = static_cast<float>(settings_.renderDistance) + CULL_SLACK;
    return request;
}

void Game::noteBlockListBuilt(bool animated) {
    visibleBlocks_ = passes_.visibleBlocks();
    drawnBlocks_ = std::min<uint64_t>(visibleBlocks_, MAX_BLOCK_INSTANCES);
    refreshPending_ = false;
    lastBuildEye_ = player_.eye;
    lastBuildForward_ = player_.forward();
    animation_.lastBuildAnimated = animated;
}

// After a stats pass: chunk bookkeeping and the population. `steps` is how many
// generations the pass came after (the population graph gets a point if any).
void Game::collectChunkStats(uint32_t steps) {
    generationsSinceStats_ = 0;
    const auto bookkeepingStart = std::chrono::steady_clock::now();
    population_ = world_.maintain();
    const double bookkeepingMs = millisecondsSince(bookkeepingStart);
    benchCpuMs_ += bookkeepingMs;
    passes_.recordMaintenanceCost(bookkeepingMs);
    if (steps > 0 || generation_ != lastRecordedGeneration_) recordPopulation();
}

void Game::rebuild(Rebuild reason) {
    const double now = glfwGetTime();
    bool animate = false;
    if (reason == Rebuild::Edit && editOpen_) {
        animation_.startTime = now;
        animation_.seconds = EDIT_ANIMATION_SECONDS;
        animate = true;
    } else if (reason == Rebuild::Camera) {
        // (A sliced generation is overwriting the previous one, which animating compares with.)
        animate = sliced_.phase == SlicePhase::Idle && animation_.lastBuildAnimated &&
                  now < animation_.startTime + animation_.seconds;
    } else {
        animation_.seconds = 0.0f;
    }
    // A camera move changes what is visible, not the world: skip the stats.
    runBatch(0, {.animate = animate, .collectStats = reason != Rebuild::Camera});
}

void Game::recordPopulation() {
    lastRecordedGeneration_ = generation_;
    populationHistory_.push_back(static_cast<float>(population_));
    if (populationHistory_.size() > POPULATION_HISTORY) {
        populationHistory_.erase(populationHistory_.begin());
    }
}

void Game::advanceGenerations(uint64_t generations) {
    uint64_t left = generations;
    while (left > 0) {
        uint32_t batch = static_cast<uint32_t>(std::min<uint64_t>(left, MAX_BATCH));
        left -= batch;
        bool lastBatch = left == 0;
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
    // Sliced work submitted last frame is done by now; take it in even while
    // paused, so the HUD and the crosshair agree with the blocks on screen.
    const uint64_t settled = settleSlicing();
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
    // Otherwise the simulation gets what drawing leaves of the frame.
    const bool flatOut = speed_.unlimited() || fastForward_ > 0;
    const double budgetMs =
        flatOut ? std::max(settings_.simBudget, FLAT_OUT_MIN_BUDGET_MS)
                : frameBudget(displayFrameMs(), renderer_.gpuFrameMs(), settings_.simBudget);
    lastBudgetMs_ = static_cast<float>(budgetMs);

    // A generation that does not fit the frame is stepped a slice per frame.
    const PassCosts& costs = passes_.costs();
    const bool generationFits =
        costs.stepMs <= 0.0 || costs.overheadMs + costs.stepMs + costs.drawMs <= budgetMs;
    const bool sliced = sliced_.phase != SlicePhase::Idle || !generationFits;
    // The GPU runs frames and the simulation in order. Work the CPU waits for
    // starts once the last frame has finished, so its drawing is not counted
    // as simulation time; slices run alongside the frames instead.
    if (!sliced) renderer_.waitForPreviousFrame();
    const auto start = std::chrono::steady_clock::now();
    drawingWaitMs_ = 0.0; // waits from here on are not the simulation's time

    const uint64_t done = sliced ? runSlicedGeneration(budgetMs, start)
                                 : runDueGenerations(due, flatOut, budgetMs, start);
    if (world_.limitReached() && !pausedAtLimit_) pauseAtChunkLimit();
    settleDebt(due, settled + done, flatOut, now);

    lastSimMs_ = static_cast<float>(simulationMsSince(start));
    // Idle for as long as the overrun took (milliseconds to seconds).
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

// Runs up to `due` generations within the budget, then makes sure the block
// list shows the result. Returns how many generations ran.
uint64_t Game::runDueGenerations(uint64_t due, bool flatOut, double budgetMs,
                                 std::chrono::steady_clock::time_point start) {
    // At slow speeds a single generation's births and deaths are animated over
    // most of the tick. That generation draws in its own batch, before chunk
    // bookkeeping frees chunks whose last cells are still shrinking away.
    const bool animate =
        animationsEnabled() && !flatOut && due == 1 && speed_.targetRate() <= MAX_ANIMATED_RATE;
    if (animate) {
        runBatch(1, {.animate = true});
        animation_.startTime = glfwGetTime();
        animation_.seconds = static_cast<float>(
            std::min(STEP_ANIMATION_SECONDS, STEP_ANIMATION_TICK_FRACTION / speed_.targetRate()));
        return 1;
    }

    uint64_t done = 0;
    bool blockListCurrent = true; // the block list shows the newest generation
    while (done < due) {
        FrameBudget frame;
        frame.generationsLeft = due - done;
        frame.generationsDone = done;
        frame.untilStats = MAX_BATCH - std::min(generationsSinceStats_, MAX_BATCH);
        frame.elapsedMs = millisecondsSince(start);
        frame.budgetMs = budgetMs;
        const BatchPlan plan = planBatch(frame, passes_.costs());
        if (!plan.runs()) break;
        // The frame's last batch builds the block list in the same pass.
        const bool draw = plan.lastOfFrame && plan.steps > 0;
        runBatch(plan.steps, {.writeBlockList = draw, .collectStats = plan.collectStats});
        done += plan.steps;
        if (plan.steps > 0) blockListCurrent = draw;
        if (world_.limitReached() && !pausedAtLimit_) {
            pauseAtChunkLimit();
            break;
        }
        if (plan.lastOfFrame) break;
    }

    if (!blockListCurrent) runBatch(0, {.collectStats = false}); // the block list for this frame
    if (done > 0) animation_.seconds = 0.0f;
    return done;
}

// A generation too big for one frame is stepped a slice per frame: as many
// chunks of the active list as fit in the budget, from the current cell buffer
// into the other one, submitted without waiting so the slice runs while the
// frame is drawn (the frame still shows the current generation). The slice
// that completes the generation also builds its block list; the next frame
// flips the buffers (settleSlicing). Chunk stats, when due, are collected the
// same way before a generation starts. Edits, loads and chunk changes (see
// ChunkWorld::version) start a generation over. Returns the generations
// completed.
uint64_t Game::runSlicedGeneration(double budgetMs, std::chrono::steady_clock::time_point start) {
    const uint64_t completed = settleSlicing();
    const bool worldChanged =
        sliced_.worldVersion != world_.version() || sliced_.ruleIndex != ruleIndex_;
    if (sliced_.phase == SlicePhase::Stepping && worldChanged) sliced_.phase = SlicePhase::Idle;
    if (sliced_.phase == SlicePhase::Idle) {
        if (generationsSinceStats_ >= MAX_BATCH) {
            // Stats are due before the next generation; it starts next frame.
            AsyncRequest request;
            request.build = buildRequest(0, {.writeBlockList = false, .collectStats = true});
            submitSimulation(request);
            sliced_.phase = SlicePhase::Stats;
            return completed;
        }
        if (world_.activeChunkCount() == 0) { // nothing to slice
            waitForDrawing();
            runBatch(1);
            return completed + 1;
        }
        sliced_.phase = SlicePhase::Stepping;
        sliced_.worldVersion = world_.version();
        sliced_.ruleIndex = ruleIndex_;
        sliced_.chunksDone = 0;
    }

    // Slices cost the CPU next to nothing: the budget is all GPU time.
    const uint32_t total = world_.activeChunkCount();
    const uint32_t left = total - sliced_.chunksDone;
    const PassCosts& costs = passes_.costs();
    const double msPerChunk = std::max(costs.stepMs, MIN_STEP_MS) / total;
    const double roomMs = budgetMs - simulationMsSince(start);
    const double fit = roomMs > 0.0 ? std::floor(roomMs / msPerChunk) : 0.0;
    const uint32_t minChunks = std::min(std::max(MIN_SLICE_CHUNKS, total / MAX_SLICES), left);
    const uint32_t chunks = static_cast<uint32_t>(
        std::clamp(fit, static_cast<double>(minChunks), static_cast<double>(left)));

    AsyncRequest request;
    request.rule = &rule();
    request.sliceFirst = sliced_.chunksDone;
    request.sliceCount = chunks;
    sliced_.chunksDone += chunks;
    if (sliced_.chunksDone == total) {
        // The last slice also builds the new generation's block list.
        request.build = buildRequest(0, {.collectStats = false});
        request.buildNextGeneration = true;
        sliced_.phase = SlicePhase::Finishing;
    }
    submitSimulation(request);
    return completed;
}

// Submits work to run alongside the frame. The call returns once the queue
// takes it, which can mean waiting until the frame queued ahead of it gets its
// swapchain image (vsync). That wait is drawing's, not the simulation's.
void Game::submitSimulation(const AsyncRequest& request) {
    const auto submitStart = std::chrono::steady_clock::now();
    passes_.submitAsync(request, world_);
    drawingWaitMs_ += millisecondsSince(submitStart);
}

// Takes in what the last frame's sliced work produced: the chunk stats, or a
// completed generation. Anything that reads or edits the world through the CPU
// or runs another pass settles first. Returns the generations completed.
uint64_t Game::settleSlicing() {
    if (sliced_.phase != SlicePhase::Stats && sliced_.phase != SlicePhase::Finishing) return 0;
    passes_.finishAsync();
    if (sliced_.phase == SlicePhase::Stats) {
        sliced_.phase = SlicePhase::Idle;
        collectChunkStats(0);
        return 0;
    }
    sliced_.phase = SlicePhase::Idle;
    world_.setCurrentBuffer(1 - world_.currentBuffer());
    ++generation_;
    ++generationsSinceStats_;
    editOpen_ = false;
    animation_.seconds = 0.0f;
    noteBlockListBuilt(false);
    return 1;
}

// Drops a sliced generation in progress (a new world, a load); the stats or
// generation it was finishing are no longer wanted.
void Game::cancelSlicing() {
    passes_.finishAsync();
    sliced_.phase = SlicePhase::Idle;
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
    // A sliced generation still on schedule is not behind.
    const bool slicingOnSchedule = sliced_.phase != SlicePhase::Idle && due <= 1;
    const bool behind = done < due && !flatOut && !slicingOnSchedule;
    if (behind) stepDebt_ = std::min(stepDebt_, 1.0); // slow down instead of spiraling
    if (speed_.unlimited()) stepDebt_ = 0.0;
    slowdown_.note(now, behind);
}

// Waits for the GPU to finish the last frame (see updateSimulation), keeping
// the wait out of the simulation's time.
void Game::waitForDrawing() {
    const auto waitStart = std::chrono::steady_clock::now();
    renderer_.waitForPreviousFrame();
    drawingWaitMs_ += millisecondsSince(waitStart);
}

double Game::simulationMsSince(std::chrono::steady_clock::time_point start) const {
    return millisecondsSince(start) - drawingWaitMs_;
}

// The display's frame time: one refresh of the window's monitor (or the
// primary one, for a window), 60 Hz when unknown.
double Game::displayFrameMs() const {
    constexpr double FALLBACK_HZ = 60.0;
    GLFWmonitor* monitor = glfwGetWindowMonitor(window_);
    if (!monitor) monitor = glfwGetPrimaryMonitor();
    const GLFWvidmode* mode = monitor ? glfwGetVideoMode(monitor) : nullptr;
    const double hz = mode && mode->refreshRate > 0 ? mode->refreshRate : FALLBACK_HZ;
    return 1000.0 / hz;
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
    return projection * glm::lookAt(player_.eye, player_.eye + forward, WORLD_UP);
}

// The view the block list is culled to: CULL_MARGIN_DEGREES wider than the
// camera's on every side, and starting a little behind it, so turning a little
// or stepping back does not uncover missing blocks before the next rebuild.
//
// The projection is not flipped like viewProjection()'s: the culling pass only
// tests whether a chunk is inside, which a y flip does not change.
glm::mat4 Game::cullingViewProjection() const {
    const float aspect = renderer_.swapchain().aspectRatio();
    const float maxHalfAngle = glm::radians(MAX_CULL_HALF_ANGLE_DEGREES);
    // Half-angles of the camera's view, vertically and horizontally.
    const float halfAngleY = glm::radians(settings_.fov * 0.5f);
    const float halfAngleX = std::atan(std::tan(halfAngleY) * aspect);
    // The same, widened by the margin.
    const float wideHalfAngleY =
        std::min(halfAngleY + glm::radians(CULL_MARGIN_DEGREES), maxHalfAngle);
    const float wideHalfAngleX =
        std::min(halfAngleX + glm::radians(CULL_MARGIN_DEGREES), maxHalfAngle);
    // glm::perspective takes a vertical field of view and the width/height
    // ratio of the image plane, which is the ratio of the half-angles' tangents.
    const float wideAspect = std::tan(wideHalfAngleX) / std::tan(wideHalfAngleY);
    glm::mat4 projection =
        glm::perspective(2.0f * wideHalfAngleY, wideAspect, NEAR_PLANE, CULLING_FAR_PLANE);
    const glm::vec3 forward = player_.forward();
    const glm::vec3 back = player_.eye - forward * CULL_SLACK;
    return projection * glm::lookAt(back, back + forward, WORLD_UP);
}

} // namespace gol3d

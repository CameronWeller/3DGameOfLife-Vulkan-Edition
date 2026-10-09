#pragma once

// The pieces of the tick governor that are plain arithmetic, kept apart from the
// GPU so they can be unit tested. Game::updateSimulation ties them together.
//
// The governor's job: each frame the simulation owes `rate * frame time`
// generations, and the frame has a time budget: what drawing leaves of the
// display's refresh, at most Settings::simBudget (frameBudget). Batches of
// generations run until the debt is paid or the budget is spent. When a world is
// too big to keep up, the *tick rate* drops instead of the frame rate: the
// leftover debt is forgiven and the HUD shows the rate actually reached.

#include <cstdint>
#include <deque>
#include <string>
#include <utility>

#include "world/PassCosts.h"

namespace gol3d {

// Simulation speed as a power of two: 2^exponent generations per second, from
// one generation every 32 s up to 8192 per second, then "as fast as possible".
class SimulationSpeed {
public:
    static constexpr int MIN_EXPONENT = -5;
    static constexpr int MAX_EXPONENT = 13;
    static constexpr int UNLIMITED_EXPONENT = MAX_EXPONENT + 1;

    explicit SimulationSpeed(int exponent = 0) { setExponent(exponent); }

    int exponent() const { return exponent_; }
    void setExponent(int exponent);
    // Doubles (steps > 0) or halves (steps < 0) the speed per step.
    void change(int steps) { setExponent(exponent_ + steps); }

    double targetRate() const; // generations per second
    bool unlimited() const { return exponent_ >= UNLIMITED_EXPONENT; }
    // "64 gen/s", "1/4 gen/s" or "max".
    std::string label() const;

    // "64", "1/4": rates below one as fractions.
    static std::string formatRate(double rate);

private:
    int exponent_ = 0;
};

// What the governor runs next. A batch is `steps` generations followed by a
// build pass that may collect the chunk stats (and then the CPU's chunk
// bookkeeping) and may write the block list the frame draws.
struct BatchPlan {
    uint32_t steps = 0;
    bool collectStats = false;
    bool lastOfFrame = false; // nothing more fits this frame: build the block list now
    bool runs() const { return steps > 0 || collectStats; }
};

// Where the frame stands when the next batch is planned.
struct FrameBudget {
    uint64_t generationsLeft = 0; // owed this frame and not run yet
    uint64_t generationsDone = 0; // run this frame so far
    // Generations that may run before the chunk stats must be collected again,
    // 0..MAX_BATCH (life can outrun the chunks after MAX_BATCH; see
    // SimulationPasses.h).
    uint32_t untilStats = 0;
    double elapsedMs = 0.0;
    double budgetMs = 0.0;
};

// Plans the next batch of the frame, or an empty plan when the frame's budget
// is used up. Costs that every batch pays (the submission, and the block list
// the frame ends with) are set aside first; then as many generations as fit
// run.
//
// The stats pass and the chunk bookkeeping are needed only once per MAX_BATCH
// generations. In small worlds they run with every batch, which keeps the HUD
// current. When they would crowd generations out of the budget, they wait until
// `untilStats` runs out, so a world too big for its budget pays for them on one
// frame in MAX_BATCH instead of on every frame. If they are due and nothing
// fits beside them, the frame runs only them.
//
// The first batch of a frame always makes progress (a generation, or the due
// stats), so the simulation never stalls completely.
BatchPlan planBatch(const FrameBudget& frame, const PassCosts& costs);

// The most the simulation may spend per frame without delaying the frame:
// the display's frame time less the GPU time drawing takes and some headroom
// for the CPU, at most `settingMs` (Settings::simBudget) and at least
// MIN_FRAME_BUDGET_MS. In a world so big that drawing fills the frame, this
// leaves the simulation a sliver, and generations are stepped a slice per
// frame (Game::runSlicedGeneration) instead of stalling the frame.
constexpr double MIN_FRAME_BUDGET_MS = 1.0;
constexpr double FRAME_HEADROOM_FRACTION = 0.15;
double frameBudget(double displayFrameMs, double drawingMs, double settingMs);

// Generations per second actually reached, over the last second or so.
class RateMeter {
public:
    void sample(double now, uint64_t generation);
    double rate() const { return rate_; }

private:
    std::deque<std::pair<double, uint64_t>> samples_; // (time, generation)
    double rate_ = 0.0;
};

// Whether the HUD says "slowed to keep up". It turns on at once and off only
// after a second without slowdowns, so it does not flicker.
class SlowdownIndicator {
public:
    void note(double now, bool slowedDown);
    bool active() const { return active_; }

private:
    bool active_ = false;
    double lastSlowdown_ = -1.0;
};

} // namespace gol3d

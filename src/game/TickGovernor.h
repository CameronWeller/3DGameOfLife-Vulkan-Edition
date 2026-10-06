#pragma once

// The pieces of the tick governor that are plain arithmetic, kept apart from the
// GPU so they can be unit tested. Game::updateSimulation ties them together.
//
// The governor's job: each frame the simulation owes `rate * frame time`
// generations, and the frame has a time budget (Settings::simBudget). Batches of
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

// The next batch size given what this frame has spent so far, or 0 when the
// frame's budget is used up. `reserveMs` is set aside for the block-list build
// that ends the frame. The first batch of a frame always runs (at least one
// generation), so the simulation never stalls completely.
uint32_t planBatch(uint64_t generationsLeft, uint64_t generationsDone, double elapsedMs,
                   double budgetMs, double reserveMs, const PassCosts& costs, uint32_t maxBatch);

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

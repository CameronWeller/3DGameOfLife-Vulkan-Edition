// The tick governor's arithmetic; see TickGovernor.h for how the pieces fit.

#include "game/TickGovernor.h"

#include <algorithm>
#include <cmath>
#include <sstream>

namespace gol3d {

void SimulationSpeed::setExponent(int exponent) {
    exponent_ = std::clamp(exponent, MIN_EXPONENT, UNLIMITED_EXPONENT);
}

double SimulationSpeed::targetRate() const {
    return std::ldexp(1.0, exponent_); // 2^exponent
}

std::string SimulationSpeed::label() const {
    return unlimited() ? std::string("max") : formatRate(targetRate()) + " gen/s";
}

std::string SimulationSpeed::formatRate(double rate) {
    std::ostringstream out;
    const bool printWhole = rate >= 1.0 || rate <= 0.0;
    if (printWhole) {
        out << std::lround(rate);
    } else {
        out << "1/" << std::lround(1.0 / rate); // the speeds below one are 1/2, 1/4, ...
    }
    return out.str();
}

BatchPlan planBatch(const FrameBudget& frame, const PassCosts& costs) {
    const bool firstBatch = frame.generationsDone == 0;
    if (frame.generationsLeft == 0) return {};
    if (!firstBatch && frame.elapsedMs >= frame.budgetMs) return {};
    const double statsMs = costs.statsAndMaintainMs();

    // Stats are due before any further step.
    if (frame.untilStats == 0) {
        const double roomMs = frame.budgetMs - frame.elapsedMs - costs.overheadMs;
        if (!firstBatch && roomMs < statsMs) return {};
        return {.steps = 0, .collectStats = true, .lastOfFrame = false};
    }

    const uint32_t fullBatch =
        static_cast<uint32_t>(std::min<uint64_t>(frame.generationsLeft, frame.untilStats));
    if (costs.stepMs <= 0.0) { // nothing measured yet: run a full batch and learn
        return {.steps = fullBatch,
                .collectStats = true,
                .lastOfFrame = fullBatch == frame.generationsLeft};
    }

    // Generations that fit in what is left of the budget after this batch's
    // submission and the frame's closing block-list build, with and without
    // the stats.
    const double roomMs = frame.budgetMs - frame.elapsedMs - costs.overheadMs - costs.drawMs;
    auto stepsThatFit = [&](double ms) {
        const double fit = ms > 0.0 ? std::floor(ms / costs.stepMs) : 0.0;
        return static_cast<uint32_t>(std::min(fit, static_cast<double>(fullBatch)));
    };
    const uint32_t withStats = stepsThatFit(roomMs - statsMs);
    const uint32_t withoutStats = stepsThatFit(roomMs);

    BatchPlan plan;
    if (withStats == fullBatch) {
        plan = {.steps = fullBatch, .collectStats = true}; // the stats cost nothing this frame
    } else if (withoutStats > 0) {
        plan = {.steps = withoutStats, .collectStats = false}; // they wait
    } else if (firstBatch) {
        plan = {.steps = 1, .collectStats = false}; // over budget, but keep moving
    } else {
        return {};
    }

    // Last when the debt is paid or not even one more generation would fit.
    const double batchMs =
        costs.overheadMs + plan.steps * costs.stepMs + (plan.collectStats ? statsMs : 0.0);
    const double roomAfterMs = roomMs - batchMs;
    plan.lastOfFrame =
        plan.steps == frame.generationsLeft || roomAfterMs < costs.overheadMs + costs.stepMs;
    return plan;
}

double frameBudget(double displayFrameMs, double drawingMs, double settingMs) {
    const double leftMs = displayFrameMs * (1.0 - FRAME_HEADROOM_FRACTION) - drawingMs;
    return std::clamp(leftMs, std::min(MIN_FRAME_BUDGET_MS, settingMs), settingMs);
}

void RateMeter::sample(double now, uint64_t generation) {
    constexpr double WINDOW_SECONDS = 1.5;
    constexpr double MIN_SPAN_SECONDS = 0.25; // shorter spans give noisy rates
    // Old samples age out of the window, but the newest two always stay, so a
    // rate can still be measured when samples arrive further apart than the
    // window (a stalled or very slow simulation).
    constexpr size_t MIN_SAMPLES = 2;
    samples_.emplace_back(now, generation);
    while (samples_.size() > MIN_SAMPLES && now - samples_.front().first > WINDOW_SECONDS) {
        samples_.pop_front();
    }
    const auto& [oldestTime, oldestGeneration] = samples_.front();
    const double span = now - oldestTime;
    if (span > MIN_SPAN_SECONDS) rate_ = static_cast<double>(generation - oldestGeneration) / span;
}

void SlowdownIndicator::note(double now, bool slowedDown) {
    constexpr double CLEAR_AFTER_SECONDS = 1.0;
    if (slowedDown) {
        lastSlowdown_ = now;
        active_ = true;
    } else if (active_ && now - lastSlowdown_ > CLEAR_AFTER_SECONDS) {
        active_ = false;
    }
}

} // namespace gol3d

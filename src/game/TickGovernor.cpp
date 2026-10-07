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

uint32_t planBatch(uint64_t generationsLeft, uint64_t generationsDone, double elapsedMs,
                   double budgetMs, double reserveMs, const PassCosts& costs, uint32_t maxBatch) {
    const bool firstBatch = generationsDone == 0;
    const uint32_t fullBatch = static_cast<uint32_t>(std::min<uint64_t>(generationsLeft, maxBatch));
    if (!firstBatch && elapsedMs >= budgetMs) return 0;
    if (costs.stepMs <= 0.0) return fullBatch; // nothing measured yet: run a full batch and learn

    // Generations that fit in what is left of the budget after the frame's
    // closing block-list build and this batch's own stats pass and overhead.
    const double roomMs = budgetMs - elapsedMs - reserveMs - costs.statsMs - costs.overheadMs;
    const double affordable = roomMs > 0.0 ? std::floor(roomMs / costs.stepMs) : 0.0;
    if (!firstBatch && affordable < 1.0) return 0;
    return static_cast<uint32_t>(std::clamp(affordable, 1.0, static_cast<double>(fullBatch)));
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

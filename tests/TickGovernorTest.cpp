// Checks for src/game/TickGovernor.h: speed steps, batch planning against the
// frame budget, the measured rate, and the "slowed to keep up" indicator.

#include "game/TickGovernor.h"
#include "TestSupport.h"

using namespace gol3d;
using testing::expect;

namespace {

void checkSpeed() {
    SimulationSpeed speed;
    expect(speed.targetRate() == 1.0 && speed.label() == "1 gen/s",
           "the default is one generation per second");
    speed.change(6);
    expect(speed.label() == "64 gen/s", "each step doubles");
    speed.setExponent(-2);
    expect(speed.label() == "1/4 gen/s", "slow speeds are fractions");
    speed.setExponent(100);
    expect(speed.unlimited() && speed.label() == "max", "the top step is unlimited");
    speed.setExponent(-100);
    expect(speed.exponent() == SimulationSpeed::MIN_EXPONENT,
           "the bottom is one generation every 32 s");
    expect(SimulationSpeed::formatRate(1234.4) == "1234", "measured rates round to whole numbers");
}

void checkPlanning() {
    constexpr uint32_t BATCH_LIMIT = 8; // most generations in one batch
    constexpr double BUDGET_MS = 8.0;   // the default Settings::simBudget
    constexpr double NO_RESERVE_MS = 0.0;
    constexpr double RESERVE_MS = 1.0;  // set aside for the frame's block-list build
    constexpr uint64_t FIRST_BATCH = 0; // generations already done this frame
    constexpr uint64_t LATER_BATCH = 5;

    const PassCosts unmeasured;
    expect(planBatch(100, FIRST_BATCH, 0.0, BUDGET_MS, NO_RESERVE_MS, unmeasured, BATCH_LIMIT) ==
               BATCH_LIMIT,
           "without measurements, run a full batch");
    expect(planBatch(3, FIRST_BATCH, 0.0, BUDGET_MS, NO_RESERVE_MS, unmeasured, BATCH_LIMIT) == 3,
           "never more than what is owed");

    PassCosts costs;
    costs.stepMs = 1.0;
    costs.statsMs = 0.5;
    costs.overheadMs = 0.5;
    // 8 ms budget, 2 ms spent, 1 ms reserved: 8 - 2 - 1 - 0.5 - 0.5 = 4 generations fit.
    expect(planBatch(100, LATER_BATCH, 2.0, BUDGET_MS, RESERVE_MS, costs, BATCH_LIMIT) == 4,
           "the batch fits the rest of the budget");
    expect(planBatch(100, LATER_BATCH, 8.0, BUDGET_MS, RESERVE_MS, costs, BATCH_LIMIT) == 0,
           "a spent budget stops the frame");
    // 7.5 ms spent leaves 8 - 7.5 - 1 - 0.5 - 0.5 < 0 ms.
    expect(planBatch(100, LATER_BATCH, 7.5, BUDGET_MS, RESERVE_MS, costs, BATCH_LIMIT) == 0,
           "too little room for one step stops it too");
    expect(planBatch(100, FIRST_BATCH, 50.0, BUDGET_MS, RESERVE_MS, costs, BATCH_LIMIT) == 1,
           "the first batch always runs one generation");

    costs.stepMs = 0.01;
    expect(planBatch(100, LATER_BATCH, 0.0, BUDGET_MS, RESERVE_MS, costs, BATCH_LIMIT) ==
               BATCH_LIMIT,
           "cheap steps are capped at a full batch");
}

void checkRateMeter() {
    // Samples are (time in seconds, generation count).
    RateMeter meter;
    meter.sample(0.0, 0);
    meter.sample(0.1, 10);
    expect(meter.rate() == 0.0, "too short a span gives no rate yet");
    meter.sample(1.0, 100);
    expect(meter.rate() == 100.0, "generations per second over the window");
    meter.sample(3.0, 100);
    expect(meter.rate() == 0.0, "a stalled simulation reads zero once old samples age out");
    meter.sample(3.5, 150);
    expect(meter.rate() == 100.0, "and picks up again from recent samples only");
}

void checkSlowdownIndicator() {
    // note(time in seconds, slowed down this frame).
    SlowdownIndicator indicator;
    indicator.note(0.0, false);
    expect(!indicator.active(), "off until a slowdown");
    indicator.note(1.0, true);
    expect(indicator.active(), "on at once");
    indicator.note(1.5, false);
    expect(indicator.active(), "stays on for a second");
    indicator.note(2.1, false);
    expect(!indicator.active(), "then turns off");
}

} // namespace

int main() {
    checkSpeed();
    checkPlanning();
    checkRateMeter();
    checkSlowdownIndicator();
    return testing::finish("tick governor");
}

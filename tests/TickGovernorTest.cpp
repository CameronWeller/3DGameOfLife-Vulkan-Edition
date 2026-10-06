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
    constexpr uint32_t MAX = 8;
    PassCosts unmeasured;
    expect(planBatch(100, 0, 0.0, 8.0, 0.0, unmeasured, MAX) == MAX,
           "without measurements, run a full batch");
    expect(planBatch(3, 0, 0.0, 8.0, 0.0, unmeasured, MAX) == 3, "never more than what is owed");

    PassCosts costs;
    costs.stepMs = 1.0;
    costs.statsMs = 0.5;
    costs.overheadMs = 0.5;
    // 8 ms budget, 2 ms spent, 1 ms reserved: 8 - 2 - 1 - 0.5 - 0.5 = 4 generations fit.
    expect(planBatch(100, 5, 2.0, 8.0, 1.0, costs, MAX) == 4,
           "the batch fits the rest of the budget");
    expect(planBatch(100, 5, 8.0, 8.0, 1.0, costs, MAX) == 0, "a spent budget stops the frame");
    expect(planBatch(100, 5, 7.5, 8.0, 1.0, costs, MAX) == 0,
           "too little room for one step stops it too");
    expect(planBatch(100, 0, 50.0, 8.0, 1.0, costs, MAX) == 1,
           "the first batch always runs one generation");

    costs.stepMs = 0.01;
    expect(planBatch(100, 5, 0.0, 8.0, 1.0, costs, MAX) == MAX,
           "cheap steps are capped at a full batch");
}

void checkRateMeter() {
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

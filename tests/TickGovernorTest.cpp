// Checks for src/game/TickGovernor.h: speed steps, batch planning against the
// frame budget, the measured rate, and the "slowed to keep up" indicator.

#include "game/TickGovernor.h"

#include <cmath>

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

// A frame with `left` generations owed, `done` run, `elapsedMs` spent of an
// 8 ms budget, and `untilStats` generations before stats are due.
FrameBudget frameAt(uint64_t left, uint64_t done, double elapsedMs, uint32_t untilStats = 8) {
    constexpr double BUDGET_MS = 8.0; // the default Settings::simBudget
    FrameBudget frame;
    frame.generationsLeft = left;
    frame.generationsDone = done;
    frame.untilStats = untilStats;
    frame.elapsedMs = elapsedMs;
    frame.budgetMs = BUDGET_MS;
    return frame;
}

void checkPlanning() {
    constexpr uint64_t FIRST_BATCH = 0; // generations already done this frame
    constexpr uint64_t LATER_BATCH = 5;

    const PassCosts unmeasured;
    BatchPlan plan = planBatch(frameAt(100, FIRST_BATCH, 0.0), unmeasured);
    expect(plan.steps == 8 && plan.collectStats && !plan.lastOfFrame,
           "without measurements, run a full batch with stats");
    plan = planBatch(frameAt(3, FIRST_BATCH, 0.0), unmeasured);
    expect(plan.steps == 3 && plan.lastOfFrame, "never more than what is owed");

    // A small world: stats and the block list are cheap.
    PassCosts costs;
    costs.stepMs = 1.0;
    costs.statsMs = 0.25;
    costs.maintainMs = 0.25;
    costs.drawMs = 1.0;
    costs.overheadMs = 0.5;
    // 8 - 2 spent - 0.5 overhead - 1 block list - 0.5 stats = 4 generations fit.
    plan = planBatch(frameAt(100, LATER_BATCH, 2.0), costs);
    expect(plan.steps == 4 && !plan.collectStats && plan.lastOfFrame,
           "stats that would crowd out a generation wait");
    plan = planBatch(frameAt(3, LATER_BATCH, 2.0), costs);
    expect(plan.steps == 3 && plan.collectStats && plan.lastOfFrame,
           "stats that fit beside everything owed run with it");
    plan = planBatch(frameAt(100, LATER_BATCH, 8.0), costs);
    expect(!plan.runs(), "a spent budget stops the frame");
    plan = planBatch(frameAt(100, LATER_BATCH, 7.0), costs);
    expect(!plan.runs(), "too little room for one step stops it too");
    plan = planBatch(frameAt(100, FIRST_BATCH, 50.0), costs);
    expect(plan.steps == 1 && !plan.collectStats,
           "the first batch always runs one generation, putting the stats off");
    plan = planBatch(frameAt(100, FIRST_BATCH, 0.0, 2), costs);
    expect(plan.steps == 2, "no more generations than stats allow");

    // A huge world: one generation and the block list fill most of the budget,
    // and stats plus bookkeeping cost as much again.
    costs.stepMs = 4.0;
    costs.statsMs = 2.5;
    costs.maintainMs = 1.0;
    costs.drawMs = 1.0;
    costs.overheadMs = 0.5;
    plan = planBatch(frameAt(64, FIRST_BATCH, 0.0, 5), costs);
    expect(plan.steps == 1 && !plan.collectStats && plan.lastOfFrame,
           "a world too big for stats every frame puts them off");
    plan = planBatch(frameAt(64, FIRST_BATCH, 0.0, 0), costs);
    expect(plan.steps == 0 && plan.collectStats,
           "due stats run alone when nothing fits beside them");
    plan = planBatch(frameAt(64, FIRST_BATCH, 4.0, 0), costs);
    expect(plan.runs(), "even on a slow frame, the first batch makes progress");
    plan = planBatch(frameAt(64, 1, 5.0, 0), costs); // 8 - 5 - 0.5 < 3.5
    expect(!plan.runs(), "but a later batch waits for the next frame");

    costs = PassCosts{};
    costs.stepMs = 0.01;
    plan = planBatch(frameAt(100, LATER_BATCH, 0.0), costs);
    expect(plan.steps == 8 && plan.collectStats && !plan.lastOfFrame,
           "cheap steps are capped at a full batch");
}

void checkFrameBudget() {
    constexpr double SIXTY_HZ_MS = 1000.0 / 60.0;
    constexpr double SETTING_MS = 8.0;
    expect(frameBudget(SIXTY_HZ_MS, 2.0, SETTING_MS) == SETTING_MS,
           "a cheap frame leaves the whole setting to the simulation");
    // 16.7 ms less 15% headroom is 14.2 ms; drawing takes 10 of it.
    expect(std::abs(frameBudget(SIXTY_HZ_MS, 10.0, SETTING_MS) - 4.17) < 0.01,
           "drawing's share of the frame comes off the budget");
    expect(frameBudget(SIXTY_HZ_MS, 30.0, SETTING_MS) == MIN_FRAME_BUDGET_MS,
           "even when drawing alone fills the frame, the simulation keeps a sliver");
    expect(frameBudget(SIXTY_HZ_MS, 2.0, 0.5) == 0.5, "never more than the setting");
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
    checkFrameBudget();
    checkRateMeter();
    checkSlowdownIndicator();
    return testing::finish("tick governor");
}

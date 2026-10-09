#pragma once

// What a batch costs, as the tick governor sees it. The GPU passes are measured
// with GPU timestamps after each batch (see SimulationPasses::run), the chunk
// bookkeeping on the CPU clock; all are smoothed, so one slow frame does not
// swing the plan.

namespace gol3d {

struct PassCosts {
    double stepMs = 0.0; // one generation step over the whole world
    // A build pass that writes the block list without gathering stats: the one
    // the governor reserves time for at the end of each frame.
    double drawMs = 0.0;
    // Gathering chunk stats: a build pass that only does that, or what it adds
    // to one that also writes the block list.
    double statsMs = 0.0;
    double overheadMs = 0.0; // CPU side of a submission: recording and waiting
    // The CPU's chunk bookkeeping after a stats pass (ChunkWorld::maintain).
    double maintainMs = 0.0;

    // A stats pass and the bookkeeping that follows it.
    double statsAndMaintainMs() const { return statsMs + maintainMs; }
};

// Exponential moving average: each new sample moves the estimate 20% of the way.
// The first sample is taken as is.
inline void smoothCost(double& estimate, double sample) {
    estimate = estimate > 0.0 ? 0.8 * estimate + 0.2 * sample : sample;
}

} // namespace gol3d

#pragma once

// What the GPU passes cost, as the tick governor sees it. Measured with GPU
// timestamps after each batch (see SimulationPasses::run) and smoothed, so one
// slow frame does not swing the plan.

namespace gol3d {

struct PassCosts {
    double stepMs = 0.0;     // one generation step over the whole world
    double drawMs = 0.0;     // a build pass that also writes the block list
    double statsMs = 0.0;    // a build pass that only gathers chunk stats
    double overheadMs = 0.0; // CPU side of a submission: recording, waiting, bookkeeping
};

// Exponential moving average: each new sample moves the estimate 20% of the way.
// The first sample is taken as is.
inline void smoothCost(double& estimate, double sample) {
    estimate = estimate > 0.0 ? 0.8 * estimate + 0.2 * sample : sample;
}

} // namespace gol3d

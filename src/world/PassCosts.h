#pragma once

// What the GPU passes cost, as the tick governor sees it. Measured with GPU
// timestamps after each batch (see SimulationPasses::run) and smoothed, so one
// slow frame does not swing the plan.

namespace gol3d {

struct PassCosts {
    double stepMs = 0.0; // one generation step over the whole world
    // A build pass that writes the block list without gathering stats: the one
    // the governor reserves time for at the end of each frame.
    double drawMs = 0.0;
    // A build pass that gathers chunk stats (with or without the block list):
    // the one that ends every batch of steps.
    double statsMs = 0.0;
    double overheadMs = 0.0; // CPU side of a submission: recording, waiting, bookkeeping
};

// Exponential moving average: each new sample moves the estimate 20% of the way.
// The first sample is taken as is.
inline void smoothCost(double& estimate, double sample) {
    estimate = estimate > 0.0 ? 0.8 * estimate + 0.2 * sample : sample;
}

} // namespace gol3d

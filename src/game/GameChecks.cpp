// Self-checks that need the real GPU: --verify compares the chunked GPU engine
// with the dense CPU reference, and --bench times the simulation alone.

#include <algorithm>
#include <chrono>
#include <iomanip>
#include <iostream>

#include "game/Game.h"
#include "util/Units.h"
#include "world/SimulationPasses.h"

namespace gol3d {
namespace {

// The CPU side of --verify: a dense box of cells around the origin, stepped
// with stepLifeReference.
struct ReferenceBox {
    static constexpr int SIZE = 96;
    static constexpr int HALF = SIZE / 2; // the box spans -48..47 on each axis

    std::vector<uint32_t> cells = std::vector<uint32_t>(static_cast<size_t>(SIZE) * SIZE * SIZE);
    std::vector<uint8_t> blocks = std::vector<uint8_t>(cells.size()); // CellKind values, 0 = none
    std::vector<uint32_t> scratch;

    static size_t index(int x, int y, int z) {
        return (static_cast<size_t>(z) * SIZE + y) * SIZE + x;
    }
    // The world cell at box position (x, y, z).
    static glm::ivec3 worldCell(int x, int y, int z) { return glm::ivec3(x, y, z) - HALF; }

    void step(const LifeRule& rule) {
        stepLifeReference(cells, scratch, SIZE, SIZE, SIZE, rule, &blocks);
        cells.swap(scratch);
    }
};

} // namespace

// Runs every rule from a soup with Stone and Ember blocks mixed in, one
// generation per batch and then full batches, and compares every cell of a
// 96^3 box with stepLifeReference after each batch. The soup is centered on a
// chunk corner, so neighbor lookups across chunks, chunk growth and the reach
// margin are all exercised.
bool Game::verifyAgainstReference() {
    bool allMatch = true;
    for (uint32_t batch : {1u, MAX_BATCH}) {
        for (size_t index = 0; index < lifeRules().size(); ++index) {
            ruleIndex_ = index;
            size_t mismatches = verifyRule(batch);
            std::cout << "verify " << rule().name << " " << describeRule(rule()) << ", " << batch
                      << (batch == 1 ? " generation" : " generations")
                      << " per batch: " << (mismatches ? "FAIL" : "ok") << " (" << mismatches
                      << " mismatches, population " << population_ << ", "
                      << world_.activeChunkCount() << " chunks)" << std::endl;
            allMatch = allMatch && mismatches == 0;
        }
    }
    return allMatch;
}

// Verifies the current rule; returns the number of mismatched cells.
size_t Game::verifyRule(uint32_t batch) {
    constexpr int SOUP = 14; // the soup spans -7..6 on each axis
    constexpr int STEPS = 24;

    // The soup: about 1.5% Stone, 1.5% Ember, the rest life at the rule's density.
    resetWorld();
    generation_ = 0;
    rng_.seed(options_.seed + static_cast<uint32_t>(ruleIndex_));
    std::uniform_real_distribution<float> chance(0.0f, 1.0f);
    const float density = std::max(rule().seedDensity, MIN_SOUP_DENSITY);
    for (int z = -SOUP / 2; z < SOUP / 2; ++z) {
        for (int y = -SOUP / 2; y < SOUP / 2; ++y) {
            for (int x = -SOUP / 2; x < SOUP / 2; ++x) {
                float roll = chance(rng_);
                if (roll < 0.015f) {
                    placeCell({x, y, z}, CellKind::Stone);
                } else if (roll < 0.03f) {
                    placeCell({x, y, z}, CellKind::Ember);
                } else if (roll < 0.03f + density) {
                    world_.setLife({x, y, z}, true);
                }
            }
        }
    }
    runBatch(0);

    // Copy the GPU world into the reference box.
    ReferenceBox reference;
    const int size = ReferenceBox::SIZE;
    for (int z = 0; z < size; ++z) {
        for (int y = 0; y < size; ++y) {
            for (int x = 0; x < size; ++x) {
                std::optional<CellKind> kind = world_.cellKind(ReferenceBox::worldCell(x, y, z));
                bool isBlock = kind && *kind != CellKind::Life;
                reference.cells[ReferenceBox::index(x, y, z)] = kind == CellKind::Life ? 1u : 0u;
                reference.blocks[ReferenceBox::index(x, y, z)] =
                    isBlock ? static_cast<uint8_t>(*kind) : 0;
            }
        }
    }

    // Step both and compare after every batch.
    size_t mismatches = 0;
    for (int step = 0; step < STEPS; step += static_cast<int>(batch)) {
        for (uint32_t i = 0; i < batch; ++i) {
            reference.step(rule());
        }
        runBatch(batch);
        uint64_t expectedPopulation = 0;
        for (int z = 0; z < size; ++z) {
            for (int y = 0; y < size; ++y) {
                for (int x = 0; x < size; ++x) {
                    uint32_t want = reference.cells[ReferenceBox::index(x, y, z)];
                    expectedPopulation += want;
                    bool got = world_.isAlive(ReferenceBox::worldCell(x, y, z));
                    if ((got ? 1u : 0u) != want) ++mismatches;
                }
            }
        }
        // A population mismatch catches stray cells outside the box.
        if (expectedPopulation != population_) ++mismatches;
    }
    return mismatches;
}

// --bench N: the simulation alone, as fast as it goes, from the starting world.
int Game::runBenchmark(uint64_t generations) {
    uint64_t peakChunks = world_.activeChunkCount();
    const uint64_t startPopulation = population_;
    const auto start = std::chrono::steady_clock::now();
    uint64_t done = 0;
    while (done < generations && !(world_.limitReached() && pausedAtLimit_)) {
        uint32_t batch = static_cast<uint32_t>(std::min<uint64_t>(generations - done, MAX_BATCH));
        done += batch;
        runBatch(batch, {.writeBlockList = done >= generations});
        peakChunks = std::max<uint64_t>(peakChunks, world_.activeChunkCount());
        if (world_.limitReached()) pausedAtLimit_ = true; // finish this batch, then stop
    }
    if (blockListStale_) runBatch(0);
    const double seconds =
        std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
    std::cout << std::fixed << std::setprecision(1) << "bench: " << rule().name << ", " << done
              << " generations in " << seconds << " s = " << static_cast<double>(done) / seconds
              << " gen/s; population " << startPopulation << " -> " << population_ << ", peak "
              << peakChunks << " chunks (" << world_.capacity() << " allocated), " << drawnBlocks_
              << " blocks drawn, GPU memory " << toMegabytes(buffers_.bytesAllocated()) << " MB"
              << (world_.limitReached() ? " (chunk limit reached)" : "") << std::endl;
    std::cout << "bench: " << passes_.totalGpuMilliseconds() << " ms in GPU passes, " << benchCpuMs_
              << " ms in chunk bookkeeping" << std::endl;
    return 0;
}

} // namespace gol3d

// Checks for the bit-sliced simulation (shaders/life3d_bits.glsl through
// src/life/BitLife.h): the same arithmetic the GPU runs, compared cell by cell
// with the plain reference in src/life/LifeRules.h. Also checks the chunk hash
// map the game uses to find chunks.
#include <iostream>
#include <map>
#include <random>
#include <string>
#include <tuple>
#include <vector>

#include "TestSupport.h"
#include "life/BitLife.h"
#include "world/ChunkMap.h"

using namespace gol3d;
using testing::expect;

namespace {

constexpr int LANES = 32;           // cells in one 32-bit row, one per bit
constexpr int COUNT_DIGITS = 5;     // bits in a neighbor count of 0..26
constexpr int NEIGHBOR_COUNTS = 27; // possible counts: 0..26
constexpr uint32_t RULE_MASK_BITS = (1u << NEIGHBOR_COUNTS) - 1; // one bit per count

// applyRule picks bit `count` of the survive or birth mask for every lane at
// once, with the count spread over five bit-sliced digits. Random masks, live
// lanes and counts, against reading the mask bit directly.
void checkRuleMultiplexer() {
    constexpr int TRIALS = 200;
    std::mt19937 rng(1);
    for (int trial = 0; trial < TRIALS; ++trial) {
        const uint32_t survive = rng() & RULE_MASK_BITS;
        const uint32_t birth = rng() & RULE_MASK_BITS;
        const uint32_t alive = rng();
        // A random count for each lane, written into the five sliced digits:
        // bit `lane` of digit d is bit d of that lane's count.
        uint32_t counts[LANES];
        bitlife::Sum5 sum{};
        uint32_t* digits[COUNT_DIGITS] = {&sum.b0, &sum.b1, &sum.b2, &sum.b3, &sum.b4};
        for (int lane = 0; lane < LANES; ++lane) {
            counts[lane] = rng() % NEIGHBOR_COUNTS;
            for (int digit = 0; digit < COUNT_DIGITS; ++digit) {
                *digits[digit] |= ((counts[lane] >> digit) & 1u) << lane;
            }
        }
        const uint32_t next = bitlife::applyRule(sum, alive, survive, birth);
        for (int lane = 0; lane < LANES; ++lane) {
            const bool laneAlive = (alive >> lane) & 1u;
            const uint32_t mask = laneAlive ? survive : birth;
            const uint32_t expected = (mask >> counts[lane]) & 1u;
            if (((next >> lane) & 1u) != expected) {
                expect(false, "applyRule lane " + std::to_string(lane) + " count " +
                                  std::to_string(counts[lane]));
                return;
            }
        }
    }
}

// Random soups (plus optional Stone and Ember blocks) straddling the corner where
// eight chunks meet, stepped by both engines; every cell must agree.
void checkAgainstReference(const LifeRule& rule, bool withBlocks, uint32_t seed) {
    // The reference runs in a dense box centered on the world origin, big enough
    // that the soup cannot reach its edges in STEPS generations (it grows at
    // most one cell per generation per side: 14 / 2 + 16 < 72 / 2).
    constexpr int BOX = 72;
    constexpr int HALF = BOX / 2;
    constexpr int SOUP = 14;
    constexpr int STEPS = 16;
    // Same floor on soup density as the game's soup stamps (MIN_SOUP_DENSITY).
    constexpr float MIN_DENSITY = 0.25f;
    // A cell's roll in [0, 1) picks what it becomes: below 3% a block (Stone
    // below 1.5%, Ember above), else life with probability `density`.
    constexpr uint32_t ROLL_STEPS = 10000;
    constexpr float BLOCK_CHANCE = 0.03f;
    constexpr float STONE_CHANCE = 0.015f;

    std::mt19937 rng(seed);
    std::vector<uint32_t> dense(static_cast<size_t>(BOX) * BOX * BOX, 0);
    std::vector<uint32_t> scratch;
    std::vector<uint8_t> blocks(dense.size(), 0);
    bitlife::BitWorld world;
    auto index = [&](int x, int y, int z) { return (static_cast<size_t>(z) * BOX + y) * BOX + x; };
    const float density = std::max(rule.seedDensity, MIN_DENSITY);
    for (int z = -SOUP / 2; z < SOUP / 2; ++z) {
        for (int y = -SOUP / 2; y < SOUP / 2; ++y) {
            for (int x = -SOUP / 2; x < SOUP / 2; ++x) {
                const float roll =
                    static_cast<float>(rng() % ROLL_STEPS) / static_cast<float>(ROLL_STEPS);
                const size_t i = index(x + HALF, y + HALF, z + HALF);
                if (withBlocks && roll < BLOCK_CHANCE) {
                    const CellKind kind = roll < STONE_CHANCE ? CellKind::Stone : CellKind::Ember;
                    blocks[i] = static_cast<uint8_t>(kind);
                    world.setBlock(x, y, z, kind);
                } else if (roll < density) {
                    dense[i] = 1;
                    world.setLife(x, y, z, true);
                }
            }
        }
    }

    for (int step = 0; step < STEPS; ++step) {
        stepLifeReference(dense, scratch, BOX, BOX, BOX, rule, withBlocks ? &blocks : nullptr);
        dense.swap(scratch);
        world.step(rule);

        uint64_t expectedPopulation = 0;
        uint64_t mismatches = 0;
        for (int z = 0; z < BOX; ++z) {
            for (int y = 0; y < BOX; ++y) {
                for (int x = 0; x < BOX; ++x) {
                    const uint32_t want = dense[index(x, y, z)];
                    expectedPopulation += want;
                    mismatches += world.alive(x - HALF, y - HALF, z - HALF) != (want != 0);
                }
            }
        }
        // A population that differs means stray cells outside the box.
        mismatches += world.population() != expectedPopulation;
        if (mismatches) {
            expect(false, std::string(rule.name) + (withBlocks ? " with blocks" : "") + ": " +
                              std::to_string(mismatches) + " mismatches at step " +
                              std::to_string(step + 1));
            return;
        }
    }
}

// Random inserts, erases and lookups against std::map, in a small coordinate
// range so probe runs collide, wrap and get shifted back on erase.
void checkChunkMap() {
    constexpr int OPERATIONS = 200000;
    // Coordinates in -20..19 for x and z and -6..5 for y: about 19,000 keys.
    constexpr uint32_t SPAN_XZ = 40;
    constexpr uint32_t SPAN_Y = 12;
    enum Operation { Insert, Erase, Lookup };
    constexpr uint32_t OPERATION_COUNT = Lookup + 1;

    std::mt19937 rng(5);
    ChunkMap map;
    std::map<std::tuple<int, int, int>, uint32_t> expected;
    for (int i = 0; i < OPERATIONS; ++i) {
        const int x = static_cast<int>(rng() % SPAN_XZ) - static_cast<int>(SPAN_XZ / 2);
        const int y = static_cast<int>(rng() % SPAN_Y) - static_cast<int>(SPAN_Y / 2);
        const int z = static_cast<int>(rng() % SPAN_XZ) - static_cast<int>(SPAN_XZ / 2);
        const auto key = std::make_tuple(x, y, z);
        switch (rng() % OPERATION_COUNT) {
            case Insert:
                if (!expected.count(key)) {
                    map.insert(x, y, z, static_cast<uint32_t>(i));
                    expected[key] = static_cast<uint32_t>(i);
                }
                break;
            case Erase:
                map.erase(x, y, z);
                expected.erase(key);
                break;
            default: { // Lookup
                auto it = expected.find(key);
                const uint32_t want = it == expected.end() ? ChunkMap::NONE : it->second;
                if (map.find(x, y, z) != want) {
                    expect(false, "ChunkMap lookup " + std::to_string(i));
                    return;
                }
            }
        }
    }
    expect(map.size() == expected.size(), "ChunkMap size");
    for (const auto& [key, slot] : expected) {
        auto [x, y, z] = key;
        if (map.find(x, y, z) != slot) {
            expect(false, "ChunkMap final contents");
            return;
        }
    }
    expect(map.find(ChunkMap::COORD_LIMIT + 1, 0, 0) == ChunkMap::NONE,
           "ChunkMap out-of-range lookup");
}

} // namespace

int main() {
    checkChunkMap();
    checkRuleMultiplexer();
    // Each rule gets two soups with their own seeds: life only, then with blocks.
    uint32_t seed = 11;
    for (const LifeRule& rule : lifeRules()) {
        checkAgainstReference(rule, false, seed++);
        checkAgainstReference(rule, true, seed++);
    }
    return testing::finish("bit-sliced engine (" + std::to_string(lifeRules().size()) + " rules)");
}

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

// Every count 0..26 against every rule mask bit, in all 32 lanes at once.
void checkRuleMultiplexer() {
    std::mt19937 rng(1);
    for (int trial = 0; trial < 200; ++trial) {
        uint32_t survive = rng() & ((1u << 27) - 1);
        uint32_t birth = rng() & ((1u << 27) - 1);
        uint32_t alive = rng();
        // A random count for each lane, written into the five sliced digits.
        uint32_t counts[32];
        bitlife::Sum5 n{};
        uint32_t* digits[5] = {&n.b0, &n.b1, &n.b2, &n.b3, &n.b4};
        for (int lane = 0; lane < 32; ++lane) {
            counts[lane] = rng() % 27;
            for (int digit = 0; digit < 5; ++digit) {
                *digits[digit] |= ((counts[lane] >> digit) & 1u) << lane;
            }
        }
        uint32_t got = bitlife::applyRule(n, alive, survive, birth);
        for (int lane = 0; lane < 32; ++lane) {
            bool laneAlive = (alive >> lane) & 1u;
            uint32_t mask = laneAlive ? survive : birth;
            if (((got >> lane) & 1u) != ((mask >> counts[lane]) & 1u)) {
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
    constexpr int BOX = 72;
    constexpr int HALF = BOX / 2;
    constexpr int SOUP = 14;
    constexpr int STEPS = 16;
    std::mt19937 rng(seed);
    std::vector<uint32_t> dense(static_cast<size_t>(BOX) * BOX * BOX, 0);
    std::vector<uint32_t> scratch;
    std::vector<uint8_t> blocks(dense.size(), 0);
    bitlife::BitWorld world;
    auto index = [&](int x, int y, int z) { return (static_cast<size_t>(z) * BOX + y) * BOX + x; };
    float density = std::max(rule.seedDensity, 0.25f);
    for (int z = -SOUP / 2; z < SOUP / 2; ++z) {
        for (int y = -SOUP / 2; y < SOUP / 2; ++y) {
            for (int x = -SOUP / 2; x < SOUP / 2; ++x) {
                float roll = static_cast<float>(rng() % 10000) / 10000.0f;
                size_t i = index(x + HALF, y + HALF, z + HALF);
                if (withBlocks && roll < 0.03f) {
                    CellKind kind = roll < 0.015f ? CellKind::Stone : CellKind::Ember;
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
        uint64_t expected = 0;
        uint64_t mismatches = 0;
        for (int z = 0; z < BOX; ++z) {
            for (int y = 0; y < BOX; ++y) {
                for (int x = 0; x < BOX; ++x) {
                    uint32_t want = dense[index(x, y, z)];
                    expected += want;
                    mismatches += world.alive(x - HALF, y - HALF, z - HALF) != (want != 0);
                }
            }
        }
        mismatches += world.population() != expected; // stray cells outside the box
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
    std::mt19937 rng(5);
    ChunkMap map;
    std::map<std::tuple<int, int, int>, uint32_t> expected;
    for (int i = 0; i < 200000; ++i) {
        int x = static_cast<int>(rng() % 40) - 20;
        int y = static_cast<int>(rng() % 12) - 6;
        int z = static_cast<int>(rng() % 40) - 20;
        auto key = std::make_tuple(x, y, z);
        switch (rng() % 3) {
            case 0:
                if (!expected.count(key)) {
                    map.insert(x, y, z, static_cast<uint32_t>(i));
                    expected[key] = static_cast<uint32_t>(i);
                }
                break;
            case 1:
                map.erase(x, y, z);
                expected.erase(key);
                break;
            default: {
                auto it = expected.find(key);
                uint32_t want = it == expected.end() ? ChunkMap::NONE : it->second;
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
    uint32_t seed = 11;
    for (const LifeRule& rule : lifeRules()) {
        checkAgainstReference(rule, false, seed++);
        checkAgainstReference(rule, true, seed++);
    }
    return testing::finish("bit-sliced engine (" + std::to_string(lifeRules().size()) + " rules)");
}

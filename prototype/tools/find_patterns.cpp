// Brute-force search for gliders and oscillators under one of the game's rules,
// used to find the patterns in include/Life3DPatterns.h. Random soups evolve in
// unbounded space; every small isolated object that appears is then run on its own
// and reported if it returns to its shape, moved (glider) or in place (oscillator).
//
//   cmake --build build/prototype --target life3d_find_patterns
//   ./build/prototype/life3d_find_patterns "Life 5766" 20000
//
// Stepping is sparse (a hash map of neighbor counts) with the same survive/birth
// masks as stepLifeReference; life3d_patterns_test verifies what it finds with
// the dense reference.
#include "Life3DRules.h"

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <mutex>
#include <random>
#include <set>
#include <string>
#include <thread>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace {

struct Cell {
    int x, y, z;
    auto operator<=>(const Cell&) const = default;
};
using Cells = std::vector<Cell>; // kept sorted

uint32_t surviveMask = 0, birthMask = 0;

uint64_t pack(int x, int y, int z) {
    constexpr int BIAS = 1 << 20;
    return (uint64_t(uint32_t(x + BIAS)) << 42) | (uint64_t(uint32_t(y + BIAS)) << 21) | uint64_t(uint32_t(z + BIAS));
}

Cell unpack(uint64_t k) {
    constexpr int BIAS = 1 << 20;
    return {int((k >> 42) & 0x1FFFFF) - BIAS, int((k >> 21) & 0x1FFFFF) - BIAS, int(k & 0x1FFFFF) - BIAS};
}

Cells step(const Cells& cells) {
    std::unordered_set<uint64_t> alive;
    std::unordered_map<uint64_t, int> counts;
    counts.reserve(cells.size() * 27);
    for (const Cell& c : cells) alive.insert(pack(c.x, c.y, c.z));
    for (const Cell& c : cells)
        for (int dz = -1; dz <= 1; ++dz)
            for (int dy = -1; dy <= 1; ++dy)
                for (int dx = -1; dx <= 1; ++dx)
                    if (dx || dy || dz) ++counts[pack(c.x + dx, c.y + dy, c.z + dz)];
    Cells next;
    for (const auto& [key, n] : counts) {
        uint32_t mask = alive.count(key) ? surviveMask : birthMask;
        if ((mask >> n) & 1u) next.push_back(unpack(key));
    }
    std::sort(next.begin(), next.end());
    return next;
}

Cell minCorner(const Cells& cells) {
    Cell m{1 << 30, 1 << 30, 1 << 30};
    for (const Cell& c : cells) m = {std::min(m.x, c.x), std::min(m.y, c.y), std::min(m.z, c.z)};
    return m;
}

Cells normalized(const Cells& cells) {
    Cell m = minCorner(cells);
    Cells out;
    for (const Cell& c : cells) out.push_back({c.x - m.x, c.y - m.y, c.z - m.z});
    std::sort(out.begin(), out.end());
    return out;
}

// The same shape under any of the 48 rotations and reflections of the cube maps
// to one key, so each object is reported once.
Cells canonical(const Cells& cells) {
    static const int perms[6][3] = {{0, 1, 2}, {0, 2, 1}, {1, 0, 2}, {1, 2, 0}, {2, 0, 1}, {2, 1, 0}};
    Cells best;
    for (const auto& perm : perms)
        for (int signs = 0; signs < 8; ++signs) {
            Cells t;
            for (const Cell& c : cells) {
                int v[3] = {c.x, c.y, c.z}, w[3];
                for (int i = 0; i < 3; ++i) w[i] = v[perm[i]] * ((signs >> i) & 1 ? -1 : 1);
                t.push_back({w[0], w[1], w[2]});
            }
            t = normalized(t);
            if (best.empty() || t < best) best = t;
        }
    return best;
}

// Groups of cells at Chebyshev distance 2 or less. Groups farther apart share no
// neighbor cell, so each evolves on its own for at least one generation.
std::vector<Cells> objects(const Cells& cells) {
    std::unordered_map<uint64_t, size_t> index;
    for (size_t i = 0; i < cells.size(); ++i) index[pack(cells[i].x, cells[i].y, cells[i].z)] = i;
    std::vector<bool> seen(cells.size(), false);
    std::vector<Cells> out;
    for (size_t i = 0; i < cells.size(); ++i) {
        if (seen[i]) continue;
        Cells group;
        std::vector<size_t> stack{i};
        seen[i] = true;
        while (!stack.empty()) {
            const Cell c = cells[stack.back()];
            stack.pop_back();
            group.push_back(c);
            for (int dz = -2; dz <= 2; ++dz)
                for (int dy = -2; dy <= 2; ++dy)
                    for (int dx = -2; dx <= 2; ++dx) {
                        auto it = index.find(pack(c.x + dx, c.y + dy, c.z + dz));
                        if (it != index.end() && !seen[it->second]) {
                            seen[it->second] = true;
                            stack.push_back(it->second);
                        }
                    }
        }
        std::sort(group.begin(), group.end());
        out.push_back(group);
    }
    return out;
}

std::mutex mutex;
std::set<Cells> tested;
std::set<Cells> reported;

void classify(const Cells& object) {
    Cells shape = normalized(object);
    {
        std::lock_guard lock(mutex);
        if (tested.size() > 2'000'000) tested.clear(); // bound memory; repeats are only re-tested
        if (!tested.insert(shape).second) return;
    }
    const Cell start = minCorner(object);
    Cells now = object;
    for (int period = 1; period <= 16; ++period) {
        now = step(now);
        if (now.empty() || now.size() > 200) return;
        if (normalized(now) != shape) continue;
        const Cell at = minCorner(now);
        const bool moved = at != start;
        if (!moved && period == 1) return; // still life
        std::lock_guard lock(mutex);
        if (!reported.insert(canonical(shape)).second) return;
        std::printf("%s period %d, shift (%d, %d, %d), %zu cells:\n", moved ? "GLIDER" : "OSCILLATOR", period,
                    at.x - start.x, at.y - start.y, at.z - start.z, shape.size());
        for (const Cell& c : shape) std::printf(" {%d, %d, %d}", c.x, c.y, c.z);
        std::printf("\n");
        std::fflush(stdout);
        return;
    }
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 3) {
        std::fprintf(stderr, "usage: %s \"RULE NAME\" SOUPS\n", argv[0]);
        return 2;
    }
    const std::string name = argv[1];
    const long soups = std::atol(argv[2]);
    for (const VulkanHIP::LifeRule& rule : VulkanHIP::lifeRules()) {
        if (name == rule.name) surviveMask = rule.surviveMask, birthMask = rule.birthMask;
    }
    if (!birthMask) {
        std::fprintf(stderr, "unknown rule %s\n", name.c_str());
        return 2;
    }
    std::atomic<long> next{0};
    std::vector<std::thread> threads;
    for (unsigned t = 0; t < std::max(1u, std::thread::hardware_concurrency()); ++t) {
        threads.emplace_back([&] {
            for (long seed; (seed = next++) < soups;) {
                // Soups of 3^3 to 6^3 cells at 30% to 60% density.
                std::mt19937 rng(static_cast<uint32_t>(seed) * 2654435761u + 1);
                const int size = 3 + int(seed % 4);
                const uint32_t permille = 300 + 50 * uint32_t((seed / 4) % 7);
                Cells cells;
                for (int z = 0; z < size; ++z)
                    for (int y = 0; y < size; ++y)
                        for (int x = 0; x < size; ++x)
                            if (rng() % 1000 < permille) cells.push_back({x, y, z});
                for (int generation = 0; generation < 60 && !cells.empty() && cells.size() < 3000; ++generation) {
                    cells = step(cells);
                    if (generation < 2) continue;
                    for (const Cells& object : objects(cells))
                        if (object.size() <= 60) classify(object);
                }
            }
        });
    }
    for (std::thread& t : threads) t.join();
    std::printf("%zu distinct gliders and oscillators\n", reported.size());
    return 0;
}

// Brute-force search for gliders and oscillators under one of the game's rules,
// used to find the patterns in src/life/Patterns.h. Random soups evolve in
// unbounded space; every small isolated object that appears is then run on its
// own and reported if it returns to its shape, moved (glider) or in place
// (oscillator).
//
//   cmake --build build --target find_patterns
//   ./build/find_patterns "Life 5766" 20000
//   ./build/find_patterns S4-5/B5 20000     (any rule, in S/B notation)
//
// Stepping is sparse (a hash map of neighbor counts) with the same survive/birth
// masks as stepLifeReference; tests/PatternsTest.cpp verifies what it finds with
// the dense reference.

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <mutex>
#include <optional>
#include <random>
#include <set>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "life/LifeRules.h"

namespace {

// Soups: cubes of SOUP_MIN_SIZE^3 to (SOUP_MIN_SIZE + 3)^3 cells, filled 30% to
// 60%. The seed picks the size and the fill (see Search::runSoup), so
// consecutive seeds cycle through every combination.
constexpr int SOUP_MIN_SIZE = 3;
constexpr int SOUP_SIZE_COUNT = 4;          // sizes 3, 4, 5, 6
constexpr uint32_t SOUP_MIN_PERMILLE = 300; // fill in thousandths: 30%
constexpr uint32_t SOUP_PERMILLE_STEP = 50; // 30%, 35%, ..., 60%
constexpr uint32_t SOUP_FILL_COUNT = 7;
constexpr int SOUP_GENERATIONS = 60;    // how long each soup runs
constexpr size_t SOUP_MAX_CELLS = 3000; // stop a soup that explodes
constexpr int SETTLE_GENERATIONS = 2;   // let the soup's first chaos pass before looking
constexpr size_t OBJECT_MAX_CELLS = 60; // larger objects are not candidates
// A candidate is run alone for up to MAX_PERIOD generations, as long as it stays
// small.
constexpr int MAX_PERIOD = 16;
constexpr size_t RUN_MAX_CELLS = 200;
// Shapes already tested are remembered, up to this many.
constexpr size_t TESTED_MEMORY = 2'000'000;

struct Cell {
    int x;
    int y;
    int z;
    auto operator<=>(const Cell&) const = default;
};
using Cells = std::vector<Cell>; // kept sorted

// A rule as two masks: bit n is set when a live cell with n live neighbors
// survives, or an empty one with n live neighbors is born.
struct Masks {
    uint32_t survive = 0;
    uint32_t birth = 0;
};

// A cell as one 64-bit key, 21 bits per coordinate after adding a bias so
// negative coordinates become positive.
constexpr int KEY_BITS = 21;
constexpr int KEY_BIAS = 1 << 20;
constexpr uint64_t KEY_FIELD = (uint64_t{1} << KEY_BITS) - 1;

uint64_t pack(int x, int y, int z) {
    auto field = [](int v) { return static_cast<uint64_t>(static_cast<uint32_t>(v + KEY_BIAS)); };
    return (field(x) << (2 * KEY_BITS)) | (field(y) << KEY_BITS) | field(z);
}

Cell unpack(uint64_t key) {
    auto coordinate = [](uint64_t field) { return static_cast<int>(field & KEY_FIELD) - KEY_BIAS; };
    return {coordinate(key >> (2 * KEY_BITS)), coordinate(key >> KEY_BITS), coordinate(key)};
}

// One generation: count every live cell's contribution to its 26 neighbors,
// then keep the cells whose count the rule accepts.
Cells step(const Cells& cells, const Masks& rule) {
    constexpr size_t NEIGHBORHOOD_CELLS = 27; // each cell touches at most this many counts
    std::unordered_set<uint64_t> alive;
    std::unordered_map<uint64_t, int> counts;
    counts.reserve(cells.size() * NEIGHBORHOOD_CELLS);
    for (const Cell& cell : cells) {
        alive.insert(pack(cell.x, cell.y, cell.z));
    }
    for (const Cell& cell : cells) {
        for (int dz = -1; dz <= 1; ++dz) {
            for (int dy = -1; dy <= 1; ++dy) {
                for (int dx = -1; dx <= 1; ++dx) {
                    if (dx || dy || dz) ++counts[pack(cell.x + dx, cell.y + dy, cell.z + dz)];
                }
            }
        }
    }
    Cells next;
    for (const auto& [key, count] : counts) {
        uint32_t mask = alive.count(key) ? rule.survive : rule.birth;
        if ((mask >> count) & 1u) next.push_back(unpack(key));
    }
    std::sort(next.begin(), next.end());
    return next;
}

// The smallest x, y and z over a non-empty set of cells.
Cell minCorner(const Cells& cells) {
    constexpr int FAR_AWAY = 1 << 30; // larger than any coordinate a soup reaches
    Cell corner{FAR_AWAY, FAR_AWAY, FAR_AWAY};
    for (const Cell& cell : cells) {
        corner = {std::min(corner.x, cell.x), std::min(corner.y, cell.y),
                  std::min(corner.z, cell.z)};
    }
    return corner;
}

// The cells moved so their minimum corner is (0, 0, 0), sorted.
Cells normalized(const Cells& cells) {
    Cell corner = minCorner(cells);
    Cells out;
    for (const Cell& cell : cells) {
        out.push_back({cell.x - corner.x, cell.y - corner.y, cell.z - corner.z});
    }
    std::sort(out.begin(), out.end());
    return out;
}

// One key for a shape under all 48 rotations and reflections of the cube (6
// axis orders times 8 sign flips), so each object is reported once.
Cells canonical(const Cells& cells) {
    static const int AXIS_ORDERS[6][3] = {{0, 1, 2}, {0, 2, 1}, {1, 0, 2},
                                          {1, 2, 0}, {2, 0, 1}, {2, 1, 0}};
    // Bit `axis` of a flip number negates that axis; 8 numbers cover every subset.
    constexpr int SIGN_FLIP_COUNT = 8;
    Cells best;
    for (const auto& order : AXIS_ORDERS) {
        for (int flips = 0; flips < SIGN_FLIP_COUNT; ++flips) {
            Cells transformed;
            for (const Cell& cell : cells) {
                const int in[3] = {cell.x, cell.y, cell.z};
                int out[3];
                for (int axis = 0; axis < 3; ++axis) {
                    bool flipped = (flips >> axis) & 1;
                    out[axis] = in[order[axis]] * (flipped ? -1 : 1);
                }
                transformed.push_back({out[0], out[1], out[2]});
            }
            transformed = normalized(transformed);
            if (best.empty() || transformed < best) best = transformed;
        }
    }
    return best;
}

// Splits live cells into objects: groups connected by steps of Chebyshev
// distance OBJECT_GAP or less. Cells farther apart than that share no neighbor
// cell, so each object evolves on its own for at least one generation.
std::vector<Cells> isolatedObjects(const Cells& cells) {
    constexpr int OBJECT_GAP = 2;
    std::unordered_map<uint64_t, size_t> indexOf;
    for (size_t i = 0; i < cells.size(); ++i) {
        indexOf[pack(cells[i].x, cells[i].y, cells[i].z)] = i;
    }
    std::vector<bool> seen(cells.size(), false);
    std::vector<Cells> groups;
    for (size_t first = 0; first < cells.size(); ++first) {
        if (seen[first]) continue;
        // Flood fill from `first`.
        Cells group;
        std::vector<size_t> toVisit{first};
        seen[first] = true;
        while (!toVisit.empty()) {
            const Cell cell = cells[toVisit.back()];
            toVisit.pop_back();
            group.push_back(cell);
            for (int dz = -OBJECT_GAP; dz <= OBJECT_GAP; ++dz) {
                for (int dy = -OBJECT_GAP; dy <= OBJECT_GAP; ++dy) {
                    for (int dx = -OBJECT_GAP; dx <= OBJECT_GAP; ++dx) {
                        auto it = indexOf.find(pack(cell.x + dx, cell.y + dy, cell.z + dz));
                        if (it != indexOf.end() && !seen[it->second]) {
                            seen[it->second] = true;
                            toVisit.push_back(it->second);
                        }
                    }
                }
            }
        }
        std::sort(group.begin(), group.end());
        groups.push_back(group);
    }
    return groups;
}

// The shared state of the search threads.
class Search {
public:
    explicit Search(Masks rule) : rule_(rule) {}

    // Runs one random soup and classifies the small objects it leaves.
    void runSoup(long seed) {
        // A multiplicative hash (Knuth's 2^32 / golden ratio) spreads consecutive
        // seeds over the generator's state.
        constexpr uint32_t KNUTH_MULTIPLIER = 2654435761u;
        std::mt19937 rng(static_cast<uint32_t>(seed) * KNUTH_MULTIPLIER + 1);
        // The low digit (base SOUP_SIZE_COUNT) of the seed picks the size, the
        // next one the fill.
        const int size = SOUP_MIN_SIZE + static_cast<int>(seed % SOUP_SIZE_COUNT);
        const uint32_t fillStep = static_cast<uint32_t>((seed / SOUP_SIZE_COUNT) % SOUP_FILL_COUNT);
        const uint32_t permille = SOUP_MIN_PERMILLE + SOUP_PERMILLE_STEP * fillStep;
        Cells cells;
        for (int z = 0; z < size; ++z) {
            for (int y = 0; y < size; ++y) {
                for (int x = 0; x < size; ++x) {
                    if (rng() % 1000 < permille) cells.push_back({x, y, z});
                }
            }
        }
        for (int generation = 0; generation < SOUP_GENERATIONS; ++generation) {
            if (cells.empty() || cells.size() >= SOUP_MAX_CELLS) break;
            cells = step(cells, rule_);
            if (generation < SETTLE_GENERATIONS) continue;
            for (const Cells& object : isolatedObjects(cells)) {
                if (object.size() <= OBJECT_MAX_CELLS) classify(object);
            }
        }
    }

    size_t reportedCount() const { return reported_.size(); }

private:
    // Runs an object alone and prints it if it is a glider or an oscillator.
    void classify(const Cells& object) {
        Cells shape = normalized(object);
        {
            std::lock_guard lock(mutex_);
            if (tested_.size() > TESTED_MEMORY) tested_.clear(); // repeats are only re-tested
            if (!tested_.insert(shape).second) return;
        }
        const Cell start = minCorner(object);
        Cells now = object;
        for (int period = 1; period <= MAX_PERIOD; ++period) {
            now = step(now, rule_);
            if (now.empty() || now.size() > RUN_MAX_CELLS) return;
            if (normalized(now) != shape) continue;
            const Cell at = minCorner(now);
            const bool moved = at != start;
            if (!moved && period == 1) return; // a still life
            report(shape, period, moved, Cell{at.x - start.x, at.y - start.y, at.z - start.z});
            return;
        }
    }

    void report(const Cells& shape, int period, bool moved, Cell shift) {
        std::lock_guard lock(mutex_);
        if (!reported_.insert(canonical(shape)).second) return;
        std::printf("%s period %d, shift (%d, %d, %d), %zu cells:\n",
                    moved ? "GLIDER" : "OSCILLATOR", period, shift.x, shift.y, shift.z,
                    shape.size());
        for (const Cell& cell : shape) {
            std::printf(" {%d, %d, %d}", cell.x, cell.y, cell.z);
        }
        std::printf("\n");
        std::fflush(stdout);
    }

    const Masks rule_;
    std::mutex mutex_;
    std::set<Cells> tested_;
    std::set<Cells> reported_;
};

// A rule from the game by name, or any rule in S/B notation.
std::optional<Masks> ruleFromArgument(const std::string& text) {
    for (const gol3d::LifeRule& known : gol3d::lifeRules()) {
        if (text == known.name) return Masks{known.surviveMask, known.birthMask};
    }
    Masks rule;
    if (gol3d::parseRuleNotation(text, rule.survive, rule.birth)) return rule;
    return std::nullopt;
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 3) {
        std::fprintf(stderr, "usage: %s \"RULE NAME\"|S../B.. SOUPS\n", argv[0]);
        return 2;
    }
    const std::string name = argv[1];
    const long soups = std::atol(argv[2]);

    const std::optional<Masks> rule = ruleFromArgument(name);
    if (!rule) {
        std::fprintf(stderr, "unknown rule %s\n", name.c_str());
        return 2;
    }

    Search search(*rule);
    std::atomic<long> nextSeed{0};
    const unsigned threadCount = std::max(1u, std::thread::hardware_concurrency());
    std::vector<std::thread> threads;
    threads.reserve(threadCount);
    for (unsigned t = 0; t < threadCount; ++t) {
        threads.emplace_back([&] {
            while (true) {
                const long seed = nextSeed++;
                if (seed >= soups) break;
                search.runSoup(seed);
            }
        });
    }
    for (std::thread& thread : threads) {
        thread.join();
    }
    std::printf("%zu distinct gliders and oscillators\n", search.reportedCount());
    return 0;
}

#pragma once

// The life-like rules the game offers, their notation, and a plain CPU
// reference simulation used to check the GPU.
//
// A rule is two sets of neighbor counts: a live cell *survives* if its count of
// live neighbors is in the survive set, and an empty cell is *born* if its count
// is in the birth set. Each set is stored as a bit mask: bit n set means "n
// neighbors". The same masks are pushed to shaders/life3d_step.comp. Most rules
// count the 26-cell 3D Moore neighborhood (the 3x3x3 cube around a cell); 2D
// rules count only the 8 cells around it in its own x-y layer (Neighborhood).

#include <array>
#include <cstdint>
#include <initializer_list>
#include <stdexcept>
#include <string>
#include <vector>

#include "life/CellTypes.h"

namespace gol3d {

// Cells in the 3x3x3 cube around a cell, the cell itself excluded: the most
// neighbors any rule counts.
constexpr int NEIGHBOR_COUNT = 26;
// Cells in the 3x3 square around a cell in its own x-y layer.
constexpr int LAYER_NEIGHBOR_COUNT = 8;
// Cells in the 3x3 squares around a cell in its x-y and y-z layers, which
// share the 2 cells above and below it.
constexpr int CROSSED_NEIGHBOR_COUNT = 14;

// Which cells around a cell a rule counts.
enum class Neighborhood : uint8_t {
    Cube,    // the 26 cells of the 3x3x3 cube around it
    Layer,   // the 8 cells of the 3x3 square around it in its own x-y layer (z
             // fixed): every layer is an independent 2D board
    Crossed, // the 14 cells of the 3x3 squares around it in its x-y layer and
             // its y-z layer (x fixed): two upright 2D boards through every cell
};

struct LifeRule {
    const char* name;
    uint32_t surviveMask; // bit n: a live cell with n live neighbors stays alive
    uint32_t birthMask;   // bit n: an empty cell with n live neighbors comes alive
    float seedDensity;    // fill ratio of the random soup a new world starts from
    int seedSize;         // edge length in cells of that soup (x and y)
    const char* description;
    int seedDepth = 0; // its depth along z; 0 for a cube (seedSize)
    Neighborhood neighborhood = Neighborhood::Cube;
};

// How many neighbors a cell has under the rule: 26, 14 or 8.
constexpr int neighborCount(const LifeRule& rule) {
    switch (rule.neighborhood) {
        case Neighborhood::Layer:
            return LAYER_NEIGHBOR_COUNT;
        case Neighborhood::Crossed:
            return CROSSED_NEIGHBOR_COUNT;
        case Neighborhood::Cube:
            break;
    }
    return NEIGHBOR_COUNT;
}

// Whether neighbor (dx, dy, dz) of a cell, each offset -1..1, counts under the rule.
constexpr bool countsAsNeighbor(const LifeRule& rule, int dx, int dz) {
    switch (rule.neighborhood) {
        case Neighborhood::Layer:
            return dz == 0;
        case Neighborhood::Crossed:
            return dz == 0 || dx == 0;
        case Neighborhood::Cube:
            break;
    }
    return true;
}

// The two masks countNeighbors() in shaders/life3d_bits.glsl takes: all ones
// where the rows in front of and behind a cell count, off its y-z layer
// (sides) and on it (middle).
struct FrontBackMasks {
    uint32_t sides;
    uint32_t middle;
};
constexpr FrontBackMasks frontBackMasks(const LifeRule& rule) {
    return {countsAsNeighbor(rule, 1, 1) ? ~0u : 0u, countsAsNeighbor(rule, 0, 1) ? ~0u : 0u};
}

// Mask with a bit for each listed neighbor count: neighborMask({6}) = 1 << 6.
constexpr uint32_t neighborMask(std::initializer_list<int> counts) {
    uint32_t mask = 0;
    for (int n : counts) {
        mask |= 1u << n;
    }
    return mask;
}

// Mask with a bit for every count from `first` to `last` inclusive.
constexpr uint32_t neighborRange(int first, int last) {
    uint32_t mask = 0;
    for (int n = first; n <= last; ++n) {
        mask |= 1u << n;
    }
    return mask;
}

// Conway's Life is B3/S23 over 8 neighbors. Those numbers do not carry over to
// the 26-neighbor 3D neighborhood (the literal rule grows without limit), so the
// closest 3D analogs are Carter Bays' rules that meet his criteria for a true
// Game of Life ("Candidates for the Game of Life in Three Dimensions", Complex
// Systems 1, 1987): random soups settle into still lifes and oscillators instead
// of dying out or exploding, and gliders exist. Life 5766 is the default. Seed
// settings come from CPU sweeps in unbounded space.
inline const auto& lifeRules() {
    static const auto RULES = std::to_array<LifeRule>({
        {
            .name = "Life 5766",
            .surviveMask = neighborRange(5, 7),
            .birthMask = neighborMask({6}),
            .seedDensity = 0.30f,
            .seedSize = 32,
            .description = "Carter Bays' 3D Game of Life (1987), the closest match to Conway's "
                           "rules. As in Conway's game, random soups settle into still lifes and "
                           "oscillators instead of dying out or exploding, and gliders exist. "
                           "Needing exactly 6 neighbors to be born keeps growth in check.",
        },
        {
            .name = "Life 4555",
            .surviveMask = neighborRange(4, 5),
            .birthMask = neighborMask({5}),
            .seedDensity = 0.20f,
            .seedSize = 32,
            .description = "Carter Bays' other Life-like 3D rule. Births need only 5 neighbors and "
                           "survival 4 or 5, so it is livelier than Life 5766: soups churn longer "
                           "before settling into still lifes and oscillators. Gliders exist here "
                           "too.",
        },
        {
            .name = "Conway B3/S23",
            .surviveMask = neighborRange(2, 3),
            .birthMask = neighborMask({3}),
            .seedDensity = 0.20f,
            .seedSize = 8,
            .description = "Conway's exact numbers: born with 3 neighbors, survives with 2 or 3. "
                           "In 3D a cell has 26 neighbors instead of 8, so 3 live neighbors are "
                           "easy to find and patterns grow without limit. Included to show why 3D "
                           "needs different numbers; Conway 2D is the original game.",
        },
        {
            .name = "Clouds",
            .surviveMask = neighborRange(13, 26),
            .birthMask = neighborMask({13, 14, 17, 18, 19}),
            .seedDensity = 0.50f,
            .seedSize = 40,
            .description = "Survival needs 13 or more neighbors, so only dense regions last. Large "
                           "dense soups erode into smooth, stable blobs; sparse ones vanish. Not "
                           "Life-like: nothing moves.",
        },
        {
            .name = "Crystal",
            .surviveMask = neighborMask({4}),
            .birthMask = neighborMask({4}),
            .seedDensity = 0.20f,
            .seedSize = 8,
            .description =
                "Born and survives only with exactly 4 neighbors, so nothing is ever "
                "stable: patterns expand forever in a flickering lattice. Not Life-like.",
        },
        {
            .name = "Slow Crystal",
            .surviveMask = neighborRange(4, 6),
            .birthMask = neighborMask({5}),
            .seedDensity = 0.30f,
            .seedSize = 12,
            .description = "Born with exactly 5 neighbors, survives with 4 to 6. Grows outward "
                           "slowly and leaves solid structure behind. Not Life-like: it never "
                           "stops growing.",
        },
        {
            .name = "Coral",
            .surviveMask = neighborRange(5, 8),
            .birthMask = neighborMask({6, 7, 9, 12}),
            .seedDensity = 0.30f,
            .seedSize = 8,
            .description = "Survives with 5 to 8 neighbors, born with 6, 7, 9 or 12. Branches out "
                           "quickly into dense coral-like growth. Not Life-like: it never stops "
                           "growing.",
        },
        {
            .name = "Architecture",
            .surviveMask = neighborRange(4, 6),
            .birthMask = neighborMask({3}),
            .seedDensity = 0.10f,
            .seedSize = 8,
            .description = "Born with just 3 neighbors, survives with 4 to 6. Almost any pattern "
                           "explodes into constantly changing structures. Not Life-like.",
        },
        // Last, so the rule numbers in save files and --rule stay as they were.
        {
            .name = "Conway 2D",
            .surviveMask = neighborRange(2, 3),
            .birthMask = neighborMask({3}),
            .seedDensity = 0.35f,
            .seedSize = 64,
            .description = "Conway's original Game of Life, exactly, in every x-y layer: a cell "
                           "counts only the 8 cells around it in its own layer, so each layer is "
                           "an independent 2D board standing upright. Gliders, spaceships, guns "
                           "and every other 2D pattern behave as in the original, including the "
                           "machinery that makes it Turing complete. Build on walls that face "
                           "along z.",
            .seedDepth = 1,
            .neighborhood = Neighborhood::Layer,
        },
        {
            .name = "Conway Crossed",
            .surviveMask = neighborRange(2, 3),
            .birthMask = neighborMask({3}),
            .seedDensity = 0.35f,
            .seedSize = 32,
            .description =
                "Conway's numbers over two upright 2D boards through every cell: a cell "
                "counts the 14 cells of the 3x3 squares around it in its x-y layer and "
                "its y-z layer. Still lifes such as the block and the beehive hold, but "
                "three cells in a vertical line give birth in the crossing board, so the "
                "blinker dies and gliders, spaceships and soups grow without limit into "
                "3D structures. Not Life-like.",
            .seedDepth = 1,
            .neighborhood = Neighborhood::Crossed,
        },
    });
    return RULES;
}

// A run of consecutive counts in a mask, such as 5..7 in neighborRange(5, 7).
struct CountRun {
    int first;
    int last;
};

// The mask's counts grouped into runs, lowest first: {13..14, 17..19} for
// neighborMask({13, 14, 17, 18, 19}).
inline std::vector<CountRun> countRuns(uint32_t mask) {
    std::vector<CountRun> runs;
    for (int n = 0; n <= NEIGHBOR_COUNT; ++n) {
        if (!(mask & (1u << n))) continue;
        int last = n;
        while (last < NEIGHBOR_COUNT && (mask & (1u << (last + 1)))) {
            ++last;
        }
        runs.push_back({n, last});
        n = last; // continue after the run
    }
    return runs;
}

// Compact notation for a mask: "13-14,17-19", or "-" for an empty mask.
inline std::string describeMask(uint32_t mask) {
    std::string out;
    for (const CountRun& run : countRuns(mask)) {
        if (!out.empty()) out += ',';
        out += std::to_string(run.first);
        if (run.last > run.first) out += '-' + std::to_string(run.last);
    }
    return out.empty() ? "-" : out;
}

// Survive/birth notation, e.g. "S5-7/B6" for Life 5766.
inline std::string describeRule(const LifeRule& rule) {
    return "S" + describeMask(rule.surviveMask) + "/B" + describeMask(rule.birthMask);
}

// Parses a comma-separated list of counts and count ranges ("4-5",
// "13-14,17-19") into a mask. An empty list is the empty mask. False for
// anything else, such as a count above 26.
inline bool parseCountList(const std::string& list, uint32_t& mask) {
    mask = 0;
    size_t position = 0;
    while (position < list.size()) {
        size_t end = list.find(',', position);
        if (end == std::string::npos) end = list.size();
        const std::string item = list.substr(position, end - position);
        const size_t dash = item.find('-');
        try {
            const int first = std::stoi(item.substr(0, dash));
            const int last = dash == std::string::npos ? first : std::stoi(item.substr(dash + 1));
            if (first < 0 || last > NEIGHBOR_COUNT || first > last) return false;
            mask |= neighborRange(first, last);
        } catch (const std::exception&) { // std::stoi found no number, or one too big
            return false;
        }
        position = end + 1;
    }
    return true;
}

// Parses survive/birth notation ("S4-5/B5", "S13-26/B13-14,17-19") into
// masks; the inverse of describeRule. False when the text is not valid notation
// or the birth list is empty (no rule worth exploring has no births).
inline bool parseRuleNotation(const std::string& text, uint32_t& surviveMask, uint32_t& birthMask) {
    // The shape "S<list>/B<list>"; the shortest is "S/B" plus one count.
    const size_t slash = text.find('/');
    bool shaped = text.size() >= 4 && text[0] == 'S' && slash != std::string::npos &&
                  text.compare(slash + 1, 1, "B") == 0;
    if (!shaped) return false;
    const std::string surviveList = text.substr(1, slash - 1); // between "S" and "/"
    const std::string birthList = text.substr(slash + 2);      // after "/B"
    return parseCountList(surviveList, surviveMask) && parseCountList(birthList, birthMask) &&
           birthMask != 0;
}

// Neighbor counts in words: "exactly 6", "2 or 3", "5 to 7", "6, 7, 9 or 12".
// An empty mask reads "no number of", to fit the sentences in explainRuleCounts.
inline std::string describeCountsInWords(uint32_t mask) {
    std::vector<std::string> parts;
    int singleCounts = 0;
    for (const CountRun& run : countRuns(mask)) {
        if (run.last > run.first + 1) {
            parts.push_back(std::to_string(run.first) + " to " + std::to_string(run.last));
            continue;
        }
        // One or two counts read better listed: "2 or 3", not "2 to 3".
        for (int n = run.first; n <= run.last; ++n) {
            parts.push_back(std::to_string(n));
            ++singleCounts;
        }
    }
    if (parts.empty()) return "no number of";
    if (parts.size() == 1) return singleCounts == 1 ? "exactly " + parts[0] : parts[0];

    std::string out;
    for (size_t i = 0; i < parts.size(); ++i) {
        if (i > 0) out += (i + 1 == parts.size()) ? " or " : ", ";
        out += parts[i];
    }
    return out;
}

// The rule's survive and birth counts in plain words, e.g. for Life 5766: "A
// live cell stays alive with 5 to 7 live neighbors. An empty cell comes alive
// with exactly 6."
inline std::string explainRuleCounts(const LifeRule& rule) {
    return "A live cell stays alive with " + describeCountsInWords(rule.surviveMask) +
           " live neighbors. An empty cell comes alive with " +
           describeCountsInWords(rule.birthMask) + ".";
}

// explainRuleCounts plus a reminder of what a neighbor is.
inline std::string explainRule(const LifeRule& rule) {
    std::string around = " (the 3x3x3 cube around it).";
    if (rule.neighborhood == Neighborhood::Layer) {
        around = " (the 3x3 square around it in its own x-y layer).";
    } else if (rule.neighborhood == Neighborhood::Crossed) {
        around = " (the 3x3 squares around it in its x-y and y-z layers).";
    }
    return explainRuleCounts(rule) + " Every cell has " + std::to_string(neighborCount(rule)) +
           " neighbors" + around;
}

// CPU reference for one generation inside a width x height x depth box whose
// outside is permanently dead. Cells are stored x fastest, then y, then z. A
// pattern that stays clear of the box edges evolves exactly as it would in
// unbounded space. This is deliberately the most obvious implementation (count
// all the neighbors of every cell) so it can serve as the ground truth for the
// GPU shader and the bit-sliced engine; the game never runs it.
//
// `blocks` (optional, same layout) holds static blocks by CellKind value
// (life/CellTypes.h): 0 = none, 1 = Stone, 2 = Ember. No life exists in a
// block, and Ember blocks count as live neighbors.
inline void stepLifeReference(const std::vector<uint32_t>& current, std::vector<uint32_t>& next,
                              int width, int height, int depth, const LifeRule& rule,
                              const std::vector<uint8_t>* blocks = nullptr) {
    constexpr uint8_t EMBER = static_cast<uint8_t>(CellKind::Ember);
    next.assign(current.size(), 0);
    auto index = [&](int x, int y, int z) {
        return (static_cast<size_t>(z) * height + y) * width + x;
    };
    auto countsAsLive = [&](int x, int y, int z) -> uint32_t {
        bool inside = x >= 0 && y >= 0 && z >= 0 && x < width && y < height && z < depth;
        if (!inside) return 0;
        size_t i = index(x, y, z);
        bool ember = blocks && (*blocks)[i] == EMBER;
        return (current[i] != 0 || ember) ? 1u : 0u;
    };

    for (int z = 0; z < depth; ++z) {
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                size_t i = index(x, y, z);
                if (blocks && (*blocks)[i] != 0) continue; // a block never holds life

                uint32_t neighbors = 0;
                for (int dz = -1; dz <= 1; ++dz) {
                    for (int dy = -1; dy <= 1; ++dy) {
                        for (int dx = -1; dx <= 1; ++dx) {
                            bool isSelf = dx == 0 && dy == 0 && dz == 0;
                            bool counts = !isSelf && countsAsNeighbor(rule, dx, dz);
                            if (counts) neighbors += countsAsLive(x + dx, y + dy, z + dz);
                        }
                    }
                }
                // Survive or be born: bit `neighbors` of the matching mask.
                uint32_t mask = current[i] ? rule.surviveMask : rule.birthMask;
                next[i] = (mask >> neighbors) & 1u;
            }
        }
    }
}

} // namespace gol3d

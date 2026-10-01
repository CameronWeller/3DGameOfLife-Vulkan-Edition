#pragma once

#include <array>
#include <cstdint>
#include <initializer_list>
#include <string>
#include <vector>

namespace VulkanHIP {

// A two-state 3D life-like rule over the 26-cell Moore neighborhood.
// Bit n of surviveMask/birthMask is set when a cell with n live neighbors
// survives / is born. The same masks are pushed to shaders/life3d_chunks.comp.
struct LifeRule {
    const char* name;
    uint32_t surviveMask;
    uint32_t birthMask;
    float seedDensity; // fill ratio of the soup a new world starts from
    int seedSize;      // edge length in cells of that soup
    const char* description;
};

constexpr uint32_t neighborMask(std::initializer_list<int> counts) {
    uint32_t mask = 0;
    for (int n : counts) mask |= 1u << n;
    return mask;
}

constexpr uint32_t neighborRange(int first, int last) {
    uint32_t mask = 0;
    for (int n = first; n <= last; ++n) mask |= 1u << n;
    return mask;
}

// Conway's Life is B3/S23 over 8 neighbors. Those numbers do not carry over to
// the 26-neighbor 3D Moore neighborhood (the literal rule grows without limit),
// so the closest 3D analogs are Carter Bays' rules that meet his criteria for a
// true Game of Life ("Candidates for the Game of Life in Three Dimensions",
// Complex Systems 1, 1987): random soups settle into still lifes and
// oscillators instead of dying out or exploding, and gliders exist. Life 5766
// is the default. Seed settings come from CPU sweeps in unbounded space.
inline const std::array<LifeRule, 8>& lifeRules() {
    static const std::array<LifeRule, 8> rules{{
        {"Life 5766",    neighborRange(5, 7),   neighborMask({6}),                  0.30f, 32,
         "Carter Bays' 3D Game of Life (1987), the closest match to Conway's rules. As in Conway's game, random soups settle into still lifes and oscillators instead of dying out or exploding, and gliders exist. Needing exactly 6 neighbors to be born keeps growth in check."},
        {"Life 4555",    neighborRange(4, 5),   neighborMask({5}),                  0.20f, 32,
         "Carter Bays' other Life-like 3D rule. Births need only 5 neighbors and survival 4 or 5, so it is livelier than Life 5766: soups churn longer before settling into still lifes and oscillators. Gliders exist here too."},
        {"Conway B3/S23", neighborRange(2, 3),  neighborMask({3}),                  0.20f, 8,
         "Conway's exact numbers: born with 3 neighbors, survives with 2 or 3. In 3D a cell has 26 neighbors instead of 8, so 3 live neighbors are easy to find and patterns grow without limit. Included to show why 3D needs different numbers."},
        {"Clouds",       neighborRange(13, 26), neighborMask({13, 14, 17, 18, 19}), 0.50f, 40,
         "Survival needs 13 or more neighbors, so only dense regions last. Large dense soups erode into smooth, stable blobs; sparse ones vanish. Not Life-like: nothing moves."},
        {"Crystal",      neighborMask({4}),     neighborMask({4}),                  0.20f, 8,
         "Born and survives only with exactly 4 neighbors, so nothing is ever stable: patterns expand forever in a flickering lattice. Not Life-like."},
        {"Slow Crystal", neighborRange(4, 6),   neighborMask({5}),                  0.30f, 12,
         "Born with exactly 5 neighbors, survives with 4 to 6. Grows outward slowly and leaves solid structure behind. Not Life-like: it never stops growing."},
        {"Coral",        neighborRange(5, 8),   neighborMask({6, 7, 9, 12}),        0.30f, 8,
         "Survives with 5 to 8 neighbors, born with 6, 7, 9 or 12. Branches out quickly into dense coral-like growth. Not Life-like: it never stops growing."},
        {"Architecture", neighborRange(4, 6),   neighborMask({3}),                  0.10f, 8,
         "Born with just 3 neighbors, survives with 4 to 6. Almost any pattern explodes into constantly changing structures. Not Life-like."},
    }};
    return rules;
}

// Human-readable "13-26" form of a neighbor-count mask.
inline std::string describeMask(uint32_t mask) {
    std::string out;
    for (int n = 0; n <= 26; ++n) {
        if (!(mask & (1u << n))) continue;
        int last = n;
        while (last < 26 && (mask & (1u << (last + 1)))) ++last;
        if (!out.empty()) out += ',';
        out += std::to_string(n);
        if (last > n) out += '-' + std::to_string(last);
        n = last;
    }
    return out.empty() ? "-" : out;
}

inline std::string describeRule(const LifeRule& rule) {
    return "S" + describeMask(rule.surviveMask) + "/B" + describeMask(rule.birthMask);
}

// Neighbor counts in words: "exactly 6", "2 or 3", "5 to 7", "6, 7, 9 or 12".
inline std::string describeCountsInWords(uint32_t mask) {
    std::vector<std::string> parts;
    int singles = 0;
    for (int n = 0; n <= 26; ++n) {
        if (!(mask & (1u << n))) continue;
        int last = n;
        while (last < 26 && (mask & (1u << (last + 1)))) ++last;
        if (last > n + 1) {
            parts.push_back(std::to_string(n) + " to " + std::to_string(last));
        } else {
            for (int k = n; k <= last; ++k) parts.push_back(std::to_string(k)); // "2 or 3", not "2 to 3"
            singles += last - n + 1;
        }
        n = last;
    }
    if (parts.empty()) return "no number of";
    if (parts.size() == 1) return singles == 1 ? "exactly " + parts[0] : parts[0];
    std::string out;
    for (size_t i = 0; i < parts.size(); ++i) {
        if (i) out += i + 1 == parts.size() ? " or " : ", ";
        out += parts[i];
    }
    return out;
}

// The rule's specification in plain words, e.g. for Life 5766: "A live cell stays
// alive with 5 to 7 live neighbors. An empty cell comes alive with exactly 6.
// Every cell has 26 neighbors (the 3x3x3 cube around it)."
inline std::string explainRule(const LifeRule& rule) {
    return "A live cell stays alive with " + describeCountsInWords(rule.surviveMask) +
           " live neighbors. An empty cell comes alive with " + describeCountsInWords(rule.birthMask) +
           ". Every cell has 26 neighbors (the 3x3x3 cube around it).";
}

// CPU reference for one generation inside a box whose outside is permanently
// dead (x fastest, then y, then z). A pattern that stays clear of the box edges
// evolves exactly as it would in unbounded space. Used to verify the compute
// shader; not used in the game loop.
inline void stepLifeReference(const std::vector<uint32_t>& current, std::vector<uint32_t>& next,
                              int width, int height, int depth, const LifeRule& rule) {
    next.assign(current.size(), 0);
    auto at = [&](int x, int y, int z) -> uint32_t {
        if (x < 0 || y < 0 || z < 0 || x >= width || y >= height || z >= depth) return 0;
        return current[(z * height + y) * width + x];
    };
    for (int z = 0; z < depth; ++z) {
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                uint32_t neighbors = 0;
                for (int dz = -1; dz <= 1; ++dz)
                    for (int dy = -1; dy <= 1; ++dy)
                        for (int dx = -1; dx <= 1; ++dx)
                            if (dx || dy || dz) neighbors += at(x + dx, y + dy, z + dz);
                uint32_t mask = at(x, y, z) ? rule.surviveMask : rule.birthMask;
                next[(z * height + y) * width + x] = (mask >> neighbors) & 1u;
            }
        }
    }
}

} // namespace VulkanHIP

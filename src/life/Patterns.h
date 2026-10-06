#pragma once

#include <initializer_list>
#include <utility>
#include <vector>

namespace gol3d {

// Small named patterns for the tutorial and the hotbar. Coordinates are cells
// (x, y, z) with y up. Every claim below (still life, period, glider shift) is
// checked against stepLifeReference in tests/PatternsTest.cpp.
//
// Why Life 5766 is Conway's Life in disguise: stack a 2D pattern two layers
// thick. A live cell with n live 2D neighbors then has 2n + 1 live 3D neighbors
// (n in each layer plus its twin), and an empty cell in the slab has 2n. S5-7
// keeps 2n + 1 in {5, 7}, i.e. n in {2, 3}; B6 needs 2n = 6, i.e. n = 3. That is
// exactly Conway's S23/B3. Cells just above or below the slab see one layer's
// 3x3 window, so the copy stays exact while no such window holds exactly 6
// cells, which is true for the small patterns here.
struct PatternCell {
    int x, y, z;
    bool operator==(const PatternCell&) const = default;
};
using Pattern = std::vector<PatternCell>;

// A 2D pattern of (x, z) cells laid flat at y = 0.
inline Pattern flatPattern(std::initializer_list<std::pair<int, int>> xz) {
    Pattern out;
    for (auto [x, z] : xz) {
        out.push_back({x, 0, z});
    }
    return out;
}

// The same 2D pattern two layers thick (y = 0 and y = 1).
inline Pattern doubledPattern(std::initializer_list<std::pair<int, int>> xz) {
    Pattern out;
    for (int y = 0; y < 2; ++y) {
        for (auto [x, z] : xz) {
            out.push_back({x, y, z});
        }
    }
    return out;
}

// The pattern moved by (dx, dy, dz).
inline Pattern translated(Pattern pattern, int dx, int dy, int dz) {
    for (PatternCell& c : pattern) {
        c.x += dx;
        c.y += dy;
        c.z += dz;
    }
    return pattern;
}

// Conway's 2D glider:  . X .
//                      . . X
//                      X X X   (rows are z = 0, 1, 2; columns x = 0, 1, 2)
// In 2D it moves one cell in +x and +z every 4 generations.
inline Pattern conwayGliderFlat() {
    return flatPattern({{1, 0}, {2, 1}, {0, 2}, {1, 2}, {2, 2}});
}

// Life 5766 glider: Conway's glider two layers thick (10 cells). Period 4,
// moves (+1, 0, +1) per period, as the explanation above predicts. (The soup
// search in tools/FindPatterns.cpp finds it too.)
inline Pattern life5766Glider() {
    Pattern doubled = conwayGliderFlat();
    for (const PatternCell& cell : conwayGliderFlat()) {
        doubled.push_back({cell.x, 1, cell.z});
    }
    return doubled;
}

// Life 5766 still life: Conway's 2x2 block two layers thick, i.e. a 2x2x2 cube.
// Every cell has 7 live neighbors; no empty cell has 6.
inline Pattern life5766Block() {
    return doubledPattern({{0, 0}, {1, 0}, {0, 1}, {1, 1}});
}

// Life 5766 period-2 oscillators: Conway's blinker (3 in a row) and toad, each
// two layers thick. The blinker slab turns a quarter turn and back.
inline Pattern life5766Blinker() {
    return doubledPattern({{0, 1}, {1, 1}, {2, 1}});
}
inline Pattern life5766Toad() {
    return doubledPattern({{1, 0}, {2, 0}, {3, 0}, {0, 1}, {1, 1}, {2, 1}});
}

// Life 4555 glider (10 cells): a 2x2 square beside a ring of 6. Period 4, moves
// (+1, 0, +1) per period. It is not a doubled Conway pattern: with B5, an empty
// cell in a two-layer slab (always an even count) can never be born.
// Found by a brute-force search over random soups (tools/FindPatterns.cpp).
inline Pattern life4555Glider() {
    return {{1, 1, 1}, {1, 2, 1}, {1, 1, 0}, {1, 2, 0}, {0, 1, 2},
            {0, 2, 2}, {0, 0, 1}, {0, 3, 1}, {0, 0, 0}, {0, 3, 0}};
}

} // namespace gol3d

#pragma once

// The materials a world cell can hold.
//
// Life cells follow the current rule. Every other kind is a static block the
// player places, which never changes by itself.
//
// The GPU does not store a kind per cell. It stores one bit plane per
// *behavior* (see shaders/life3d_storage.glsl):
//   - life:    the cell is alive and follows the rule;
//   - blocked: a static block sits here, so no life can be born or survive in it;
//   - emits:   the block counts as a live neighbor of the cells around it.
// A kind is a combination of those planes, which keeps the simulation a few
// bitwise operations per row however many kinds exist. A new kind with a new
// behavior (for example a block that decays, or a second species of life) adds
// a plane here and a term in shaders/life3d_bits.glsl; a new kind that only
// looks different from an existing one needs no new plane.

#include <array>
#include <cstddef>
#include <cstdint>

namespace gol3d {

// Values are stored in save files, so append new kinds at the end.
enum class CellKind : uint8_t {
    Life = 0,
    Stone = 1, // inert: blocks life and is invisible to the rule
    Ember = 2, // a permanent live neighbor: feeds births around it, never dies
};

struct CellType {
    CellKind kind;
    const char* name;
    const char* description;
    bool blocksLife;       // no life can exist in this cell
    bool countsAsNeighbor; // counts toward the neighbor total of the cells around it
    uint8_t rgb[3];        // menu swatch (sRGB); the hotbar in life3d_screen.frag matches it
};

inline const std::array<CellType, 3>& cellTypes() {
    static const std::array<CellType, 3> TYPES{{
        {
            .kind = CellKind::Life,
            .name = "Life",
            .description = "Living cells. They are born, survive and die by the current rule.",
            .blocksLife = false,
            .countsAsNeighbor = true,
            .rgb = {94, 230, 168},
        },
        {
            .kind = CellKind::Stone,
            .name = "Stone",
            .description =
                "An inert block. Life can't be born inside it and the rule doesn't see it, "
                "so a wall of stone stops a pattern from spreading. Use it to build "
                "containers and channels.",
            .blocksLife = true,
            .countsAsNeighbor = false,
            .rgb = {150, 156, 170},
        },
        {
            .kind = CellKind::Ember,
            .name = "Ember",
            .description = "A block that never changes but always counts as a live neighbor. It "
                           "feeds births next to it, so embers can anchor and power patterns that "
                           "would otherwise die.",
            .blocksLife = true,
            .countsAsNeighbor = true,
            .rgb = {255, 150, 60},
        },
    }};
    return TYPES;
}

inline const CellType& cellType(CellKind kind) {
    return cellTypes()[static_cast<size_t>(kind)];
}

} // namespace gol3d

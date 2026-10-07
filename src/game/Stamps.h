#pragma once

// Hotbar stamps: the shapes a click places. A stamp is placed against the face
// the crosshair points at and grows away from it, so it never overlaps the
// targeted block. Q/E rotate it around that face's normal and Z/C tilt it
// around the world x axis.

#include <array>
#include <cstdint>
#include <random>
#include <vector>

#include <glm/glm.hpp>

#include "life/LifeRules.h"

namespace gol3d {

// Hotbar order; it matches the icons in shaders/life3d_screen.frag.
enum class Stamp { Cell, Block, Plus, SmallSoup, BigSoup, Wall, Pillar, RuleSeed, Glider };
constexpr int STAMP_COUNT = static_cast<int>(Stamp::Glider) + 1;

// Soups are at least this dense, whatever the rule's seed density: sparser
// soups mostly die before they do anything interesting.
constexpr float MIN_SOUP_DENSITY = 0.25f;

// How a stamp is presented in the hotbar and the inventory.
struct StampInfo {
    const char* name;
    const char* description;
    // 5x5 icon, one bit per pixel, row by row from the top-left pixel at bit 24.
    // Same bitmaps as ICONS in shaders/life3d_screen.frag.
    uint32_t icon;
};

inline const std::array<StampInfo, STAMP_COUNT>& stampInfos() {
    static const std::array<StampInfo, STAMP_COUNT> INFOS{{
        {"Cell", "One live cell.", 0x0001000u},
        {"Block 2x2x2", "A solid 2x2x2 cube.", 0x00739C0u},
        {"Plus", "A 3D cross of seven cells.", 0x0023880u},
        {"Soup 8^3", "An 8x8x8 random soup at the rule's density.", 0x0051120u},
        {"Soup 16^3", "A 16x16x16 random soup at the rule's density.", 0x165E9B6u},
        {"Wall 5x5", "A solid 5x5 wall. On the ground it stands upright.", 0x1FFFFFFu},
        {"Pillar 8", "A line of 8 cells growing away from the surface.", 0x0421084u},
        {"Rule seed", "The current rule's own starting soup.", 0x1555555u},
        {"Glider",
         "Bays' glider (Life 4555's under that rule, Life 5766's otherwise). It lies flat on the "
         "ground and stands up on a wall; Q/E pick its heading, Z/C tilt it to climb or dive.",
         0x00209C0u},
    }};
    return INFOS;
}

inline const StampInfo& stampInfo(Stamp stamp) {
    return stampInfos()[static_cast<size_t>(stamp)];
}

// Where and how a stamp goes.
struct StampPlacement {
    Stamp stamp = Stamp::Cell;
    glm::ivec3 anchor{0};             // the cell next to the targeted face
    glm::ivec3 normal{0, 1, 0};       // unit axis direction away from that face
    int rotation = 0;                 // quarter turns around `normal` (Q/E)
    int tilt = 0;                     // quarter turns around world x, applied after rotation (Z/C)
    glm::vec3 viewDirection{1, 0, 0}; // a wall placed on a floor stands across the view
    bool solid = false;               // fill soups completely, for the placement outline
};

// The cells of a placed stamp. Soups draw from `rng` (unless solid), so the
// same seed places the same soup.
std::vector<glm::ivec3> stampCells(const StampPlacement& placement, const LifeRule& rule,
                                   std::mt19937& rng);

// The smallest box holding all cells (inclusive corners).
struct CellBounds {
    glm::ivec3 min;
    glm::ivec3 max;
};
CellBounds boundsOf(const std::vector<glm::ivec3>& cells);

} // namespace gol3d

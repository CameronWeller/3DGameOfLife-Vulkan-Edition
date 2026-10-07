// Turns a hotbar stamp into cells. Every stamp is first built in a frame tied
// to the targeted face (u and v across it, the normal away from it), then
// turned by the player's rotation and tilt around the anchor.

#include "game/Stamps.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <string>

#include "life/Patterns.h"

namespace gol3d {
namespace {

// Sizes of the fixed shapes, in cells. The hotbar descriptions in Stamps.h
// quote them.
constexpr int BLOCK_SIZE = 2;
constexpr int SMALL_SOUP_SIZE = 8;
constexpr int BIG_SOUP_SIZE = 16;
constexpr int WALL_SIZE = 5;
constexpr int PILLAR_LENGTH = 8;

// Pattern cell (1, y, 1) of a glider goes over the anchor, so the 3-cell-wide
// glider is centered on it.
constexpr int GLIDER_CENTER = 1;

// The index of the axis `steps` places after `axis`, cycling x -> y -> z -> x.
int axisAfter(int axis, int steps) {
    return (axis + steps) % 3;
}

// The two axes across a surface whose normal lies along `axis`.
struct SurfaceAxes {
    int axis;     // 0, 1 or 2: the axis of the normal
    glm::ivec3 u; // the next axis after it (x -> y -> z -> x)
    glm::ivec3 v; // the one after that
};

SurfaceAxes surfaceAxes(const glm::ivec3& normal) {
    SurfaceAxes axes;
    if (normal.x != 0) {
        axes.axis = 0;
    } else if (normal.y != 0) {
        axes.axis = 1;
    } else {
        axes.axis = 2;
    }
    axes.u = glm::ivec3(0);
    axes.v = glm::ivec3(0);
    axes.u[axisAfter(axes.axis, 1)] = 1;
    axes.v[axisAfter(axes.axis, 2)] = 1;
    return axes;
}

// Turns an offset a quarter turn around the surface normal (u -> v -> -u).
glm::ivec3 rotateAroundNormal(glm::ivec3 offset, int axis) {
    const int uAxis = axisAfter(axis, 1);
    const int vAxis = axisAfter(axis, 2);
    const int alongU = offset[uAxis];
    const int alongV = offset[vAxis];
    offset[uAxis] = -alongV;
    offset[vAxis] = alongU;
    return offset;
}

// Turns an offset a quarter turn around the world x axis (y -> z -> -y).
glm::ivec3 tiltAroundX(const glm::ivec3& offset) {
    return glm::ivec3(offset.x, -offset.z, offset.y);
}

// A 5x5 wall standing up from a floor or ceiling, across the view direction.
void addStandingWall(std::vector<glm::ivec3>& cells, const StampPlacement& placement) {
    const glm::vec3& view = placement.viewDirection;
    const bool lookingAlongX = std::abs(view.x) > std::abs(view.z);
    const glm::ivec3 across = lookingAlongX ? glm::ivec3(0, 0, 1) : glm::ivec3(1, 0, 0);
    for (int height = 0; height < WALL_SIZE; ++height) {
        for (int u = 0; u < WALL_SIZE; ++u) {
            const int fromCenter = u - WALL_SIZE / 2;
            cells.push_back(placement.anchor + across * fromCenter + placement.normal * height);
        }
    }
}

// The rule's glider. Pattern x and z lie across the surface and pattern y
// grows away from it, so the glider slides along the surface it is placed on.
void addGlider(std::vector<glm::ivec3>& cells, const StampPlacement& placement,
               const SurfaceAxes& axes, const LifeRule& rule) {
    const bool life4555 = std::string(rule.name) == "Life 4555";
    const Pattern glider = life4555 ? life4555Glider() : life5766Glider();
    for (const PatternCell& cell : glider) {
        cells.push_back(placement.anchor + axes.u * (cell.x - GLIDER_CENTER) +
                        axes.v * (cell.z - GLIDER_CENTER) + placement.normal * cell.y);
    }
}

} // namespace

std::vector<glm::ivec3> stampCells(const StampPlacement& placement, const LifeRule& rule,
                                   std::mt19937& rng) {
    const glm::ivec3& anchor = placement.anchor;
    const glm::ivec3& normal = placement.normal;
    const SurfaceAxes axes = surfaceAxes(normal);
    std::vector<glm::ivec3> cells;

    // A box `across` cells wide on the surface and `along` cells deep, centered
    // on the anchor, each cell kept with probability `density`. Cells are
    // visited in a fixed order and the generator is only drawn from when the
    // outcome is in doubt, so a seed always gives the same soup.
    auto addBox = [&](int across, int along, float density) {
        std::uniform_real_distribution<float> chance(0.0f, 1.0f);
        for (int depth = 0; depth < along; ++depth) {
            for (int v = 0; v < across; ++v) {
                for (int u = 0; u < across; ++u) {
                    const bool keep = placement.solid || density >= 1.0f || chance(rng) < density;
                    if (!keep) continue;
                    const int uFromCenter = u - across / 2;
                    const int vFromCenter = v - across / 2;
                    cells.push_back(anchor + axes.u * uFromCenter + axes.v * vFromCenter +
                                    normal * depth);
                }
            }
        }
    };
    const float soupDensity = std::max(rule.seedDensity, MIN_SOUP_DENSITY);

    switch (placement.stamp) {
        case Stamp::Cell:
            cells.push_back(anchor);
            break;
        case Stamp::Block:
            addBox(BLOCK_SIZE, BLOCK_SIZE, 1.0f);
            break;
        case Stamp::Plus: {
            // Centered one cell out, so the arm toward the surface sits on the anchor.
            const glm::ivec3 center = anchor + normal;
            for (const glm::ivec3& arm :
                 {glm::ivec3(0), axes.u, -axes.u, axes.v, -axes.v, normal, -normal}) {
                cells.push_back(center + arm);
            }
            break;
        }
        case Stamp::SmallSoup:
            addBox(SMALL_SOUP_SIZE, SMALL_SOUP_SIZE, soupDensity);
            break;
        case Stamp::BigSoup:
            addBox(BIG_SOUP_SIZE, BIG_SOUP_SIZE, soupDensity);
            break;
        case Stamp::Wall:
            // Flat against a wall; on a floor or ceiling it stands up.
            if (axes.axis == 1) {
                addStandingWall(cells, placement);
            } else {
                addBox(WALL_SIZE, 1, 1.0f);
            }
            break;
        case Stamp::Pillar:
            addBox(1, PILLAR_LENGTH, 1.0f);
            break;
        case Stamp::RuleSeed:
            addBox(rule.seedSize, rule.seedSize, rule.seedDensity);
            break;
        case Stamp::Glider:
            addGlider(cells, placement, axes, rule);
            break;
    }

    // Rotation first, then tilt, both around the anchor.
    for (glm::ivec3& cell : cells) {
        glm::ivec3 offset = cell - anchor;
        for (int turn = 0; turn < placement.rotation; ++turn) {
            offset = rotateAroundNormal(offset, axes.axis);
        }
        for (int turn = 0; turn < placement.tilt; ++turn) {
            offset = tiltAroundX(offset);
        }
        cell = anchor + offset;
    }
    return cells;
}

CellBounds boundsOf(const std::vector<glm::ivec3>& cells) {
    // Start inside out, so the first cell sets both corners.
    CellBounds bounds{glm::ivec3(std::numeric_limits<int>::max()),
                      glm::ivec3(std::numeric_limits<int>::lowest())};
    for (const glm::ivec3& cell : cells) {
        bounds.min = glm::min(bounds.min, cell);
        bounds.max = glm::max(bounds.max, cell);
    }
    return bounds;
}

} // namespace gol3d

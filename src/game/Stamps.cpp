#include "game/Stamps.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <string>

#include "life/Patterns.h"

namespace gol3d {
namespace {

// The two axes across a surface whose normal lies along `axis`.
struct SurfaceAxes {
    int axis;     // 0, 1 or 2: the axis of the normal
    glm::ivec3 u; // the next axis after it (x -> y -> z -> x)
    glm::ivec3 v; // the one after that
};

SurfaceAxes surfaceAxes(const glm::ivec3& normal) {
    SurfaceAxes axes;
    axes.axis = normal.x != 0 ? 0 : (normal.y != 0 ? 1 : 2);
    axes.u = glm::ivec3(0);
    axes.v = glm::ivec3(0);
    axes.u[(axes.axis + 1) % 3] = 1;
    axes.v[(axes.axis + 2) % 3] = 1;
    return axes;
}

// Turns an offset a quarter turn around the surface normal.
glm::ivec3 rotateAroundNormal(glm::ivec3 offset, int axis) {
    int alongU = offset[(axis + 1) % 3];
    int alongV = offset[(axis + 2) % 3];
    offset[(axis + 1) % 3] = -alongV;
    offset[(axis + 2) % 3] = alongU;
    return offset;
}

// Turns an offset a quarter turn around the world x axis (y -> z -> -y).
glm::ivec3 tiltAroundX(const glm::ivec3& offset) {
    return glm::ivec3(offset.x, -offset.z, offset.y);
}

} // namespace

std::vector<glm::ivec3> stampCells(const StampPlacement& placement, const LifeRule& rule,
                                   std::mt19937& rng) {
    const glm::ivec3& anchor = placement.anchor;
    const glm::ivec3& normal = placement.normal;
    const SurfaceAxes axes = surfaceAxes(normal);
    std::vector<glm::ivec3> cells;

    // A box `across` cells wide on the surface and `along` cells deep, centered
    // on the anchor, each cell kept with probability `density`.
    auto addBox = [&](int across, int along, float density) {
        std::uniform_real_distribution<float> chance(0.0f, 1.0f);
        for (int depth = 0; depth < along; ++depth) {
            for (int v = 0; v < across; ++v) {
                for (int u = 0; u < across; ++u) {
                    bool keep = placement.solid || density >= 1.0f || chance(rng) < density;
                    if (!keep) continue;
                    cells.push_back(anchor + axes.u * (u - across / 2) + axes.v * (v - across / 2) +
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
            addBox(2, 2, 1.0f);
            break;
        case Stamp::Plus: {
            const glm::ivec3 center = anchor + normal;
            for (const glm::ivec3& d :
                 {glm::ivec3(0), axes.u, -axes.u, axes.v, -axes.v, normal, -normal}) {
                cells.push_back(center + d);
            }
            break;
        }
        case Stamp::SmallSoup:
            addBox(8, 8, soupDensity);
            break;
        case Stamp::BigSoup:
            addBox(16, 16, soupDensity);
            break;
        case Stamp::Wall:
            if (axes.axis == 1) {
                // On a floor or ceiling, stand the wall up across the view direction.
                const glm::vec3& view = placement.viewDirection;
                glm::ivec3 across =
                    std::abs(view.x) > std::abs(view.z) ? glm::ivec3(0, 0, 1) : glm::ivec3(1, 0, 0);
                for (int height = 0; height < 5; ++height) {
                    for (int u = 0; u < 5; ++u) {
                        cells.push_back(anchor + across * (u - 2) + normal * height);
                    }
                }
            } else {
                addBox(5, 1, 1.0f);
            }
            break;
        case Stamp::Pillar:
            addBox(1, 8, 1.0f);
            break;
        case Stamp::RuleSeed:
            addBox(rule.seedSize, rule.seedSize, rule.seedDensity);
            break;
        case Stamp::Glider: {
            // Pattern x and z lie across the surface and pattern y grows away from
            // it, so the glider slides along the surface it is placed on.
            const bool life4555 = std::string(rule.name) == "Life 4555";
            const Pattern glider = life4555 ? life4555Glider() : life5766Glider();
            for (const PatternCell& cell : glider) {
                cells.push_back(anchor + axes.u * (cell.x - 1) + axes.v * (cell.z - 1) +
                                normal * cell.y);
            }
            break;
        }
    }

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
    CellBounds bounds{glm::ivec3(std::numeric_limits<int>::max()),
                      glm::ivec3(std::numeric_limits<int>::lowest())};
    for (const glm::ivec3& cell : cells) {
        bounds.min = glm::min(bounds.min, cell);
        bounds.max = glm::max(bounds.max, cell);
    }
    return bounds;
}

} // namespace gol3d

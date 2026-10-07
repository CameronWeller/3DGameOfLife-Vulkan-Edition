// Checks for src/game/Stamps.h: the shape of each stamp, and how rotation and
// tilt turn it.

#include <algorithm>
#include <set>
#include <string>
#include <tuple>

#include "TestSupport.h"
#include "game/Stamps.h"

using namespace gol3d;
using testing::expect;

namespace {

const LifeRule& ruleNamed(const std::string& name) {
    for (const LifeRule& rule : lifeRules()) {
        if (name == rule.name) return rule;
    }
    return lifeRules()[0];
}

std::set<std::tuple<int, int, int>> asSet(const std::vector<glm::ivec3>& cells) {
    std::set<std::tuple<int, int, int>> out;
    for (const glm::ivec3& cell : cells) {
        out.insert({cell.x, cell.y, cell.z});
    }
    return out;
}

// How onFloor() places a stamp; the defaults are an unturned stamp under the
// default rule (Life 5766).
struct FloorPlacement {
    int rotation = 0;
    int tilt = 0;
    bool solid = false;
    uint32_t seed = 1;
    const LifeRule* rule = &lifeRules()[0];
};

// A stamp on the floor (normal +y) at the origin.
std::vector<glm::ivec3> onFloor(Stamp stamp, const FloorPlacement& how = {}) {
    StampPlacement placement;
    placement.stamp = stamp;
    placement.rotation = how.rotation;
    placement.tilt = how.tilt;
    placement.solid = how.solid;
    std::mt19937 rng(how.seed);
    return stampCells(placement, *how.rule, rng);
}

void checkShapes() {
    expect(onFloor(Stamp::Cell) == std::vector<glm::ivec3>{glm::ivec3(0)},
           "a cell stamp is its anchor");
    expect(asSet(onFloor(Stamp::Block)).size() == 2 * 2 * 2, "a block is 2x2x2");

    const std::vector<glm::ivec3> plus = onFloor(Stamp::Plus);
    expect(plus.size() == 7 && asSet(plus).count({0, 1, 0}),
           "a plus is 7 cells around the cell above the anchor");

    const CellBounds pillar = boundsOf(onFloor(Stamp::Pillar));
    expect(pillar.min == glm::ivec3(0) && pillar.max == glm::ivec3(0, 7, 0),
           "a pillar grows 8 cells up");

    const FloorPlacement under4555{.rule = &ruleNamed("Life 4555")};
    expect(onFloor(Stamp::Glider).size() == 10, "Life 5766's glider has 10 cells");
    expect(onFloor(Stamp::Glider, under4555).size() == 10, "Life 4555's glider has 10 cells");
    expect(onFloor(Stamp::Glider) != onFloor(Stamp::Glider, under4555),
           "the glider follows the rule");
}

void checkSoups() {
    constexpr int SMALL_SOUP_SIZE = 8;
    expect(onFloor(Stamp::SmallSoup, {.solid = true}).size() ==
               SMALL_SOUP_SIZE * SMALL_SOUP_SIZE * SMALL_SOUP_SIZE,
           "a solid soup fills its whole box");
    expect(onFloor(Stamp::SmallSoup, {.seed = 7}) == onFloor(Stamp::SmallSoup, {.seed = 7}),
           "the same seed places the same soup");
    expect(onFloor(Stamp::SmallSoup, {.seed = 7}) != onFloor(Stamp::SmallSoup, {.seed = 8}),
           "another seed places another soup");

    // 16 cells across, centered: x and z run from -8 to 7. 16 cells up from the floor.
    const CellBounds soup = boundsOf(onFloor(Stamp::BigSoup, {.solid = true}));
    expect(soup.min == glm::ivec3(-8, 0, -8) && soup.max == glm::ivec3(7, 15, 7),
           "a soup is centered across the surface and grows away from it");
}

void checkWall() {
    StampPlacement placement;
    placement.stamp = Stamp::Wall;
    placement.viewDirection = glm::vec3(1, 0, 0); // looking along +x
    std::mt19937 rng(1);
    CellBounds bounds = boundsOf(stampCells(placement, lifeRules()[0], rng));
    // 5x5: one cell thick along the view, 5 wide across it, 5 tall.
    expect(bounds.min.x == 0 && bounds.max.x == 0 && bounds.max.y == 4 &&
               bounds.max.z - bounds.min.z == 4,
           "on a floor, a wall stands up across the view");

    placement.normal = glm::ivec3(1, 0, 0); // against a wall facing +x
    bounds = boundsOf(stampCells(placement, lifeRules()[0], rng));
    expect(bounds.min.x == 0 && bounds.max.x == 0, "on a wall, a wall lies flat against it");
}

void checkRotationAndTilt() {
    expect(asSet(onFloor(Stamp::Glider, {.rotation = 4})) == asSet(onFloor(Stamp::Glider)),
           "four quarter turns are no turn");
    expect(asSet(onFloor(Stamp::Glider, {.rotation = 1})) != asSet(onFloor(Stamp::Glider)),
           "a quarter turn changes the heading");

    // Tilting turns y toward z: a pillar on the floor becomes a row along +z.
    const CellBounds tilted = boundsOf(onFloor(Stamp::Pillar, {.tilt = 1}));
    expect(tilted.min == glm::ivec3(0) && tilted.max == glm::ivec3(0, 0, 7),
           "a tilt turns up into +z");
    const CellBounds twice = boundsOf(onFloor(Stamp::Pillar, {.tilt = 2}));
    expect(twice.min == glm::ivec3(0, -7, 0) && twice.max == glm::ivec3(0),
           "two tilts turn it upside down");
}

void checkTable() {
    // A 5x5 icon has 25 pixels, so only the low 25 bits may be set.
    constexpr uint32_t ICON_PIXELS = 5 * 5;
    expect(static_cast<int>(stampInfos().size()) == STAMP_COUNT, "one entry per stamp");
    for (const StampInfo& info : stampInfos()) {
        expect(info.icon != 0 && info.icon < (1u << ICON_PIXELS),
               std::string(info.name) + " has a 5x5 icon");
    }
}

} // namespace

int main() {
    checkShapes();
    checkSoups();
    checkWall();
    checkRotationAndTilt();
    checkTable();
    return testing::finish("stamps");
}

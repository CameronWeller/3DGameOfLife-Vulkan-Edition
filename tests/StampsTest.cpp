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
    for (const glm::ivec3& c : cells) {
        out.insert({c.x, c.y, c.z});
    }
    return out;
}

// A stamp on the floor (normal +y) at the origin.
std::vector<glm::ivec3> onFloor(Stamp stamp, int rotation = 0, int tilt = 0, bool solid = false,
                                uint32_t seed = 1, const LifeRule& rule = lifeRules()[0]) {
    StampPlacement placement;
    placement.stamp = stamp;
    placement.rotation = rotation;
    placement.tilt = tilt;
    placement.solid = solid;
    std::mt19937 rng(seed);
    return stampCells(placement, rule, rng);
}

void checkShapes() {
    expect(onFloor(Stamp::Cell) == std::vector<glm::ivec3>{glm::ivec3(0)},
           "a cell stamp is its anchor");
    expect(asSet(onFloor(Stamp::Block)).size() == 8, "a block is 2x2x2");
    std::vector<glm::ivec3> plus = onFloor(Stamp::Plus);
    expect(plus.size() == 7 && asSet(plus).count({0, 1, 0}),
           "a plus is 7 cells around the cell above the anchor");
    CellBounds pillar = boundsOf(onFloor(Stamp::Pillar));
    expect(pillar.min == glm::ivec3(0) && pillar.max == glm::ivec3(0, 7, 0),
           "a pillar grows 8 cells up");
    expect(onFloor(Stamp::Glider).size() == 10, "Life 5766's glider has 10 cells");
    expect(onFloor(Stamp::Glider, 0, 0, false, 1, ruleNamed("Life 4555")).size() == 10,
           "Life 4555's glider has 10 cells");
    expect(onFloor(Stamp::Glider) != onFloor(Stamp::Glider, 0, 0, false, 1, ruleNamed("Life 4555")),
           "the glider follows the rule");
}

void checkSoups() {
    expect(onFloor(Stamp::SmallSoup, 0, 0, true).size() == 8 * 8 * 8,
           "a solid soup fills its whole box");
    expect(onFloor(Stamp::SmallSoup, 0, 0, false, 7) == onFloor(Stamp::SmallSoup, 0, 0, false, 7),
           "the same seed places the same soup");
    expect(onFloor(Stamp::SmallSoup, 0, 0, false, 7) != onFloor(Stamp::SmallSoup, 0, 0, false, 8),
           "another seed places another soup");
    CellBounds soup = boundsOf(onFloor(Stamp::BigSoup, 0, 0, true));
    expect(soup.min == glm::ivec3(-8, 0, -8) && soup.max == glm::ivec3(7, 15, 7),
           "a soup is centered across the surface and grows away from it");
}

void checkWall() {
    StampPlacement placement;
    placement.stamp = Stamp::Wall;
    placement.viewDirection = glm::vec3(1, 0, 0); // looking along +x
    std::mt19937 rng(1);
    CellBounds bounds = boundsOf(stampCells(placement, lifeRules()[0], rng));
    expect(bounds.min.x == 0 && bounds.max.x == 0 && bounds.max.y == 4 &&
               bounds.max.z - bounds.min.z == 4,
           "on a floor, a wall stands up across the view");

    placement.normal = glm::ivec3(1, 0, 0); // against a wall facing +x
    bounds = boundsOf(stampCells(placement, lifeRules()[0], rng));
    expect(bounds.min.x == 0 && bounds.max.x == 0, "on a wall, a wall lies flat against it");
}

void checkRotationAndTilt() {
    expect(asSet(onFloor(Stamp::Glider, 4)) == asSet(onFloor(Stamp::Glider, 0)),
           "four quarter turns are no turn");
    expect(asSet(onFloor(Stamp::Glider, 1)) != asSet(onFloor(Stamp::Glider, 0)),
           "a quarter turn changes the heading");
    // Tilting turns y toward z: a pillar on the floor becomes a row along +z.
    CellBounds tilted = boundsOf(onFloor(Stamp::Pillar, 0, 1));
    expect(tilted.min == glm::ivec3(0) && tilted.max == glm::ivec3(0, 0, 7),
           "a tilt turns up into +z");
    CellBounds twice = boundsOf(onFloor(Stamp::Pillar, 0, 2));
    expect(twice.min == glm::ivec3(0, -7, 0) && twice.max == glm::ivec3(0),
           "two tilts turn it upside down");
}

void checkTable() {
    expect(static_cast<int>(stampInfos().size()) == STAMP_COUNT, "one entry per stamp");
    for (const StampInfo& info : stampInfos()) {
        expect(info.icon != 0 && info.icon < (1u << 25),
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

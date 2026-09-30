#include "GameRules.h"
#include "SparseGrid3D.h"

#include <iostream>
#include <stdexcept>

static void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

int main() {
    const GameRules::RuleSet birthOnOne("B1", 1, 1, 27, 27);

    SparseGrid3D fixed(3, 3, 3);
    fixed.setBoundaryType(GameRules::BoundaryType::FIXED);
    fixed.setRuleSet(birthOnOne);
    fixed.setCell(0, 0, 0, true);
    fixed.update();
    require(fixed.getPopulation() == 7, "fixed edge population");
    require(fixed.getCell(1, 1, 1), "fixed edge birth");
    require(!fixed.getCell(3, 0, 0), "fixed edge outside grid");

    SparseGrid3D infinite(3, 3, 3);
    infinite.setBoundaryType(GameRules::BoundaryType::INFINITE);
    infinite.setRuleSet(birthOnOne);
    infinite.setCell(0, 0, 0, true);
    infinite.update();
    require(infinite.getPopulation() == 7, "infinite edge population");

    SparseGrid3D toroidal(3, 3, 3);
    toroidal.setRuleSet(birthOnOne);
    toroidal.setCell(0, 0, 0, true);
    toroidal.update();
    require(toroidal.getPopulation() == 26, "toroidal population");
    require(toroidal.getCell(2, 2, 2), "toroidal wrap");

    SparseGrid3D mirror(3, 3, 3);
    mirror.setBoundaryType(GameRules::BoundaryType::MIRROR);
    mirror.setRuleSet(birthOnOne);
    mirror.setCell(0, 0, 0, true);
    mirror.update();
    require(mirror.getPopulation() == 7, "mirror edge population");

    SparseGrid3D single(1, 1, 1);
    single.setBoundaryType(GameRules::BoundaryType::MIRROR);
    single.setRuleSet(birthOnOne);
    single.setCell(0, 0, 0, true);
    single.update();
    require(single.getPopulation() == 0, "single-cell mirror update");

    SparseGrid3D empty(0, 3, 3);
    empty.update();
    require(empty.getPopulation() == 0, "empty grid update");

    const GameRules::RuleSet* rule = GameRules::getRuleSetByName("5766");
    require(rule == &GameRules::RULE_5766, "rule lookup lifetime");
    require(GameRules::getRuleSetByName("missing") == nullptr, "unknown rule lookup");

    std::cout << "SparseGrid3D standalone tests passed\n";
}

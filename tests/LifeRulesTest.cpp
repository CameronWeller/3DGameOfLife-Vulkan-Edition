// Checks for src/life/LifeRules.h: rule notation and the CPU reference.
#include "life/LifeRules.h"

#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

#include "TestSupport.h"

using namespace gol3d;
using testing::expect;

namespace {

const LifeRule& ruleNamed(const std::string& name) {
    for (const LifeRule& rule : lifeRules()) {
        if (name == rule.name) return rule;
    }
    std::cerr << "missing rule " << name << "\n";
    std::exit(1);
}

// A cube of `size`^3 cells for the dense reference, 1 = alive.
struct Grid {
    int size;
    std::vector<uint32_t> cells;
    explicit Grid(int edge) : size(edge), cells(static_cast<size_t>(edge) * edge * edge, 0) {}
    uint32_t& at(int x, int y, int z) {
        return cells[(static_cast<size_t>(z) * size + y) * size + x];
    }
    size_t population() const {
        size_t count = 0;
        for (uint32_t cell : cells) {
            count += cell;
        }
        return count;
    }
    void step(const LifeRule& rule) {
        std::vector<uint32_t> next;
        stepLifeReference(cells, next, size, size, size, rule);
        cells.swap(next);
    }
};

// A 2x2x2 block (cells 3..4 on each axis) in the middle of an 8^3 grid.
Grid blockGrid() {
    Grid grid(8);
    for (int z = 3; z < 5; ++z) {
        for (int y = 3; y < 5; ++y) {
            for (int x = 3; x < 5; ++x) {
                grid.at(x, y, z) = 1;
            }
        }
    }
    return grid;
}

} // namespace

int main() {
    expect(describeRule(ruleNamed("Clouds")) == "S13-26/B13-14,17-19", "Clouds notation");
    expect(describeRule(ruleNamed("Crystal")) == "S4/B4", "Crystal notation");
    expect(describeMask(0) == "-", "empty mask notation");
    expect(explainRule(ruleNamed("Life 5766")) ==
               "A live cell stays alive with 5 to 7 live neighbors. An empty cell comes alive with "
               "exactly 6. "
               "Every cell has 26 neighbors (the 3x3x3 cube around it).",
           "Life 5766 explanation");
    expect(describeCountsInWords(ruleNamed("Coral").birthMask) == "6, 7, 9 or 12",
           "list of counts in words");
    expect(describeCountsInWords(ruleNamed("Conway B3/S23").surviveMask) == "2 or 3",
           "two counts in words");
    expect(describeCountsInWords(ruleNamed("Clouds").surviveMask) == "13 to 26",
           "range of counts in words");

    expect(describeRule(lifeRules()[0]) == "S5-7/B6", "Life 5766 (Bays) is the default rule");

    // Notation parses back into the same masks, for every rule.
    for (const LifeRule& rule : lifeRules()) {
        uint32_t survive = 0;
        uint32_t birth = 0;
        bool parsed = parseRuleNotation(describeRule(rule), survive, birth);
        expect(parsed && survive == rule.surviveMask && birth == rule.birthMask,
               std::string(rule.name) + " notation round-trips");
    }
    uint32_t survive = 0;
    uint32_t birth = 0;
    expect(!parseRuleNotation("B3/S23", survive, birth), "Conway's B/S order is not accepted");
    expect(!parseRuleNotation("S4/B27", survive, birth), "counts above 26 are rejected");
    expect(!parseRuleNotation("S4/B", survive, birth), "a rule without births is rejected");
    expect(describeRule(ruleNamed("Conway B3/S23")) == "S2-3/B3", "literal Conway notation");

    // A 2x2x2 block: every cell has 7 live neighbors; outside cells see at most 4.
    Grid bays = blockGrid();
    bays.step(ruleNamed("Life 5766"));
    expect(bays.cells == blockGrid().cells, "2x2x2 block is a still life under S5-7/B6");

    // Under S4/B4 the block dies (7 is not 4) and each of the 24 face-adjacent
    // cells (4 live neighbors) is born.
    Grid crystal = blockGrid();
    crystal.step(ruleNamed("Crystal"));
    expect(crystal.population() == 24, "S4/B4 turns a 2x2x2 block into its 24 face neighbors");
    expect(crystal.at(3, 3, 3) == 0 && crystal.at(2, 3, 3) == 1,
           "S4/B4 block cells die, face neighbors are born");

    // Space outside the box is dead: nothing wraps around to the far side.
    Grid edge(6);
    for (int y = 0; y < 2; ++y) {
        for (int z = 0; z < 2; ++z) {
            for (int x = 0; x < 2; ++x) {
                edge.at(x, y, z) = 1;
            }
        }
    }
    edge.step(ruleNamed("Crystal"));
    expect(edge.at(5, 0, 0) == 0 && edge.at(0, 5, 0) == 0 && edge.at(0, 0, 5) == 0,
           "no wraparound at the box edge");
    expect(edge.at(2, 0, 0) == 1, "births still happen inside the box next to the edge");

    // Conway 2D counts only the cell's own x-y layer: a blinker turns within its
    // layer, and nothing is born in the layers beside it.
    const LifeRule conway2D = ruleNamed("Conway 2D"); // a copy: GCC flags a reference here
    expect(explainRule(conway2D).find("8 neighbors") != std::string::npos,
           "Conway 2D explains its 8 neighbors");
    Grid blinker(5);
    for (int x = 1; x < 4; ++x) {
        blinker.at(x, 2, 2) = 1;
    }
    blinker.step(conway2D);
    expect(blinker.population() == 3 && blinker.at(2, 1, 2) == 1 && blinker.at(2, 3, 2) == 1 &&
               blinker.at(2, 2, 2) == 1,
           "a Conway 2D blinker turns upright within its layer");

    // Conway Crossed counts the x-y and y-z layers through a cell: 14 cells.
    const LifeRule crossed = ruleNamed("Conway Crossed");
    expect(explainRule(crossed).find("14 neighbors") != std::string::npos,
           "Conway Crossed explains its 14 neighbors");
    int counted = 0;
    for (int dz = -1; dz <= 1; ++dz) {
        for (int dy = -1; dy <= 1; ++dy) {
            for (int dx = -1; dx <= 1; ++dx) {
                const bool self = dx == 0 && dy == 0 && dz == 0;
                if (!self && countsAsNeighbor(crossed, dx, dz)) ++counted;
            }
        }
    }
    expect(counted == CROSSED_NEIGHBOR_COUNT, "the crossed neighborhood has 14 cells");
    // Three live cells in front of an empty cell along its y-z layer give birth;
    // the same three off to the side (dx and dz both nonzero) do not.
    Grid inPlane(5);
    for (int y = 1; y < 4; ++y) {
        inPlane.at(2, y, 1) = 1;
    }
    inPlane.step(crossed);
    expect(inPlane.at(2, 2, 2) == 1, "a vertical line of 3 gives birth across the y-z layer");
    Grid offPlane(5);
    for (int y = 1; y < 4; ++y) {
        offPlane.at(1, y, 1) = 1;
    }
    offPlane.step(crossed);
    expect(offPlane.at(2, 2, 2) == 0, "cells off both layers are not neighbors");

    return testing::finish("life rules");
}

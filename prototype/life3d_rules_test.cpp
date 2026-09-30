// CPU-only checks for include/Life3DRules.h (no GPU or window needed).
#include "Life3DRules.h"

#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

using namespace VulkanHIP;

namespace {

int failures = 0;

void expect(bool condition, const std::string& what) {
    if (!condition) {
        std::cerr << "FAIL: " << what << "\n";
        ++failures;
    }
}

const LifeRule& ruleNamed(const std::string& name) {
    for (const LifeRule& rule : lifeRules()) {
        if (name == rule.name) return rule;
    }
    std::cerr << "missing rule " << name << "\n";
    std::exit(1);
}

struct Grid {
    int size;
    std::vector<uint32_t> cells;
    explicit Grid(int n) : size(n), cells(size_t(n) * n * n, 0) {}
    uint32_t& at(int x, int y, int z) { return cells[(size_t(z) * size + y) * size + x]; }
    size_t population() const {
        size_t count = 0;
        for (uint32_t c : cells) count += c;
        return count;
    }
    void step(const LifeRule& rule) {
        std::vector<uint32_t> next;
        stepLifeReference(cells, next, size, size, size, rule);
        cells.swap(next);
    }
};

Grid blockGrid() {
    Grid grid(8);
    for (int z = 3; z < 5; ++z)
        for (int y = 3; y < 5; ++y)
            for (int x = 3; x < 5; ++x) grid.at(x, y, z) = 1;
    return grid;
}

} // namespace

int main() {
    expect(describeRule(ruleNamed("Clouds")) == "S13-26/B13-14,17-19", "Clouds notation");
    expect(describeRule(ruleNamed("Crystal")) == "S4/B4", "Crystal notation");
    expect(describeMask(0) == "-", "empty mask notation");
    expect(explainRule(ruleNamed("Life 5766")) ==
               "A live cell stays alive with 5 to 7 live neighbors. An empty cell comes alive with exactly 6. "
               "Every cell has 26 neighbors (the 3x3x3 cube around it).",
           "Life 5766 explanation");
    expect(describeCountsInWords(ruleNamed("Coral").birthMask) == "6, 7, 9 or 12", "list of counts in words");
    expect(describeCountsInWords(ruleNamed("Conway B3/S23").surviveMask) == "2 or 3", "two counts in words");
    expect(describeCountsInWords(ruleNamed("Clouds").surviveMask) == "13 to 26", "range of counts in words");

    expect(describeRule(lifeRules()[0]) == "S5-7/B6", "Life 5766 (Bays) is the default rule");
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
    expect(crystal.at(3, 3, 3) == 0 && crystal.at(2, 3, 3) == 1, "S4/B4 block cells die, face neighbors are born");

    // Space outside the box is dead: nothing wraps around to the far side.
    Grid edge(6);
    for (int y = 0; y < 2; ++y)
        for (int z = 0; z < 2; ++z)
            for (int x = 0; x < 2; ++x) edge.at(x, y, z) = 1;
    edge.step(ruleNamed("Crystal"));
    expect(edge.at(5, 0, 0) == 0 && edge.at(0, 5, 0) == 0 && edge.at(0, 0, 5) == 0, "no wraparound at the box edge");
    expect(edge.at(2, 0, 0) == 1, "births still happen inside the box next to the edge");

    if (failures) return 1;
    std::cout << "life3d rules: all checks passed\n";
    return 0;
}

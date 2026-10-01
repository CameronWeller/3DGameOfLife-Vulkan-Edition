// CPU checks for include/Life3DPatterns.h and the tutorial scenes in
// src/tutorial/TutorialLessons.cpp: every still life, oscillator period and glider
// shift the tutorial claims is verified with stepLifeReference (no GPU needed).
#include "Life3DPatterns.h"
#include "Life3DRules.h"
#include "tutorial/TutorialLessons.h"

#include <algorithm>
#include <iostream>
#include <set>
#include <string>
#include <tuple>
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

const LifeRule& ruleNamed(const char* name) { return lifeRules()[tutorial::ruleIndexNamed(name)]; }

using CellSet = std::set<std::tuple<int, int, int>>;

CellSet toSet(const Pattern& pattern) {
    CellSet out;
    for (const PatternCell& c : pattern) out.insert({c.x, c.y, c.z});
    return out;
}

CellSet shifted(const CellSet& cells, int dx, int dy, int dz) {
    CellSet out;
    for (auto [x, y, z] : cells) out.insert({x + dx, y + dy, z + dz});
    return out;
}

// Runs `generations` steps of the dense CPU reference in a box around the
// pattern. The box grows by one cell per generation on every side, as fast as any
// pattern can, so the dead outside never changes the result: this is unbounded space.
std::vector<CellSet> evolve(const CellSet& start, const LifeRule& rule, int generations) {
    std::vector<CellSet> history{start};
    if (start.empty()) return history;
    int lo[3] = {1 << 30, 1 << 30, 1 << 30}, hi[3] = {-(1 << 30), -(1 << 30), -(1 << 30)};
    for (auto [x, y, z] : start) {
        int v[3] = {x, y, z};
        for (int i = 0; i < 3; ++i) lo[i] = std::min(lo[i], v[i]), hi[i] = std::max(hi[i], v[i]);
    }
    const int margin = generations + 2;
    int origin[3], size[3];
    for (int i = 0; i < 3; ++i) origin[i] = lo[i] - margin, size[i] = hi[i] - lo[i] + 1 + 2 * margin;
    std::vector<uint32_t> cells(size_t(size[0]) * size[1] * size[2], 0), next;
    auto index = [&](int x, int y, int z) {
        return (size_t(z - origin[2]) * size[1] + (y - origin[1])) * size[0] + (x - origin[0]);
    };
    for (auto [x, y, z] : start) cells[index(x, y, z)] = 1;
    for (int g = 0; g < generations; ++g) {
        stepLifeReference(cells, next, size[0], size[1], size[2], rule);
        cells.swap(next);
        CellSet live;
        for (int z = 0; z < size[2]; ++z)
            for (int y = 0; y < size[1]; ++y)
                for (int x = 0; x < size[0]; ++x)
                    if (cells[(size_t(z) * size[1] + y) * size[0] + x]) live.insert({x + origin[0], y + origin[1], z + origin[2]});
        history.push_back(live);
    }
    return history;
}

int liveNeighbors(const CellSet& cells, int x, int y, int z) {
    int count = 0;
    for (int dz = -1; dz <= 1; ++dz)
        for (int dy = -1; dy <= 1; ++dy)
            for (int dx = -1; dx <= 1; ++dx)
                if ((dx || dy || dz) && cells.count({x + dx, y + dy, z + dz})) ++count;
    return count;
}

// Conway's 2D Life (B3/S23, 8 neighbors) on (x, z) cells.
std::set<std::pair<int, int>> conwayStep(const std::set<std::pair<int, int>>& cells) {
    std::set<std::pair<int, int>> next, candidates;
    for (auto [x, z] : cells)
        for (int dz = -1; dz <= 1; ++dz)
            for (int dx = -1; dx <= 1; ++dx) candidates.insert({x + dx, z + dz});
    for (auto [x, z] : candidates) {
        int n = 0;
        for (int dz = -1; dz <= 1; ++dz)
            for (int dx = -1; dx <= 1; ++dx)
                if ((dx || dz) && cells.count({x + dx, z + dz})) ++n;
        bool alive = cells.count({x, z});
        if (n == 3 || (alive && n == 2)) next.insert({x, z});
    }
    return next;
}

// Period and shift: the first generation g >= 1 at which the pattern equals its
// start moved by (dx, dy, dz). Returns g = 0 if it never repeats within `limit`.
struct Repeat {
    int period = 0;
    int dx = 0, dy = 0, dz = 0;
};

Repeat findRepeat(const std::vector<CellSet>& history) {
    const CellSet& start = history[0];
    auto [x0, y0, z0] = *start.begin();
    for (size_t g = 1; g < history.size(); ++g) {
        const CellSet& now = history[g];
        if (now.size() != start.size() || now.empty()) continue;
        // Compare the lexicographically smallest cells, which a translation preserves.
        auto [x1, y1, z1] = *now.begin();
        if (shifted(start, x1 - x0, y1 - y0, z1 - z0) == now) return {int(g), x1 - x0, y1 - y0, z1 - z0};
    }
    return {};
}

void checkGlider(const char* name, const Pattern& pattern, const LifeRule& rule) {
    std::vector<CellSet> history = evolve(toSet(pattern), rule, 20);
    Repeat r = findRepeat(history);
    expect(r.period == 4 && r.dx == 1 && r.dy == 0 && r.dz == 1,
           std::string(name) + ": period 4, moves (+1, 0, +1) per period (got period " + std::to_string(r.period) +
               " shift " + std::to_string(r.dx) + "," + std::to_string(r.dy) + "," + std::to_string(r.dz) + ")");
    expect(history[20] == shifted(history[0], 5, 0, 5), std::string(name) + ": still gliding after 5 periods");
    for (const CellSet& phase : history) expect(phase.size() == 10, std::string(name) + ": 10 cells in every phase");
}

void checkOscillator(const char* name, const Pattern& pattern, const LifeRule& rule, int period) {
    std::vector<CellSet> history = evolve(toSet(pattern), rule, 2 * period);
    Repeat r = findRepeat(history);
    expect(r.period == period && r.dx == 0 && r.dy == 0 && r.dz == 0,
           std::string(name) + ": period " + std::to_string(period) + " in place (got " + std::to_string(r.period) + ")");
}

// A doubled 2D pattern under Life 5766 follows Conway's 2D game exactly.
void checkDoubledMatchesConway(const char* name, const Pattern& flat, int generations) {
    std::set<std::pair<int, int>> conway;
    for (const PatternCell& c : flat) conway.insert({c.x, c.z});
    auto doubled = [](const std::set<std::pair<int, int>>& cells) {
        CellSet out;
        for (auto [x, z] : cells) out.insert({x, 0, z}), out.insert({x, 1, z});
        return out;
    };
    std::vector<CellSet> history = evolve(doubled(conway), ruleNamed("Life 5766"), generations);
    for (int g = 1; g <= generations; ++g) {
        conway = conwayStep(conway);
        expect(history[g] == doubled(conway),
               std::string(name) + " doubled under Life 5766 matches Conway's 2D game at generation " + std::to_string(g));
    }
}

void checkLessons() {
    const auto& list = tutorial::lessons();
    expect(list.size() == 8, "eight lessons");
    for (const tutorial::Lesson& lesson : list) {
        expect(lesson.rule < lifeRules().size(), lesson.title + ": valid rule");
        expect(!lesson.cells.empty() && !lesson.paragraphs.empty() && !lesson.tryThis.empty(), lesson.title + ": content");
        float yaw = 0, pitch = 0;
        tutorial::lookAngles(lesson, yaw, pitch);
        expect(pitch < 0.0f && pitch > -60.0f, lesson.title + ": camera looks down at the scene");
    }
    const LifeRule& rule0 = lifeRules()[list[0].rule];
    auto scene = [](const tutorial::Lesson& lesson) { return toSet(lesson.cells); };

    // 1. The 26 outlined neighbors surround the lone cell, which then dies.
    {
        const tutorial::Lesson& l = list[0];
        CellSet marks;
        for (const auto& m : l.marks) marks.insert({m.cell.x, m.cell.y, m.cell.z});
        const PatternCell& c = l.cells[0];
        bool allAdjacent = true;
        for (auto [x, y, z] : marks)
            allAdjacent &= std::max({std::abs(x - c.x), std::abs(y - c.y), std::abs(z - c.z)}) == 1;
        expect(l.cells.size() == 1 && marks.size() == 26 && allAdjacent, "lesson 1 outlines the 26 neighbors");
        expect(evolve(scene(l), rule0, 1)[1].empty(), "lesson 1: a lone cell dies");
    }
    // 2. Each outline's color matches its cell's count and fate.
    {
        const tutorial::Lesson& l = list[1];
        expect(l.rule == tutorial::ruleIndexNamed("Life 5766"), "lesson 2 uses Life 5766");
        CellSet now = scene(l), next = evolve(now, lifeRules()[l.rule], 1)[1];
        for (const auto& m : l.marks) {
            auto [x, y, z] = std::tuple(m.cell.x, m.cell.y, m.cell.z);
            int n = liveNeighbors(now, x, y, z);
            bool alive = now.count({x, y, z}), after = next.count({x, y, z});
            if (m.mark == tutorial::Mark::Born) expect(!alive && n == 6 && after, "lesson 2: green cells are empty with 6 and born");
            if (m.mark == tutorial::Mark::Dies) expect(alive && n == 3 && !after, "lesson 2: red cells are alive with 3 and die");
            if (m.mark == tutorial::Mark::Survives) expect(alive && n == 5 && after, "lesson 2: blue cells are alive with 5 and survive");
        }
        expect(l.marks.size() == 10, "lesson 2 outlines every cell that changes or survives");
    }
    // 3. Conway's numbers explode: about one block per generation in every direction.
    {
        const tutorial::Lesson& l = list[2];
        expect(l.rule == tutorial::ruleIndexNamed("Conway B3/S23"), "lesson 3 uses Conway's literal numbers");
        std::vector<CellSet> history = evolve(scene(l), lifeRules()[l.rule], 20);
        int lo[3] = {1 << 30, 1 << 30, 1 << 30}, hi[3] = {-(1 << 30), -(1 << 30), -(1 << 30)};
        for (auto [x, y, z] : history[20]) {
            int v[3] = {x, y, z};
            for (int i = 0; i < 3; ++i) lo[i] = std::min(lo[i], v[i]), hi[i] = std::max(hi[i], v[i]);
        }
        bool wide = true;
        for (int i = 0; i < 3; ++i) wide &= hi[i] - lo[i] + 1 >= 30;
        expect(history[20].size() > 2000 && wide, "lesson 3: Conway's glider under B3/S23 grows past 2000 cells, 30+ wide on every axis, in 20 generations");
expect(history[10].size() > 50 * history[0].size() && history[20].size() > 4 * history[10].size(),
               "lesson 3: population keeps multiplying");
    }
    // 4. The cube is a still life.
    {
        const tutorial::Lesson& l = list[3];
        expect(evolve(scene(l), lifeRules()[l.rule], 1)[1] == scene(l), "lesson 4: the cube is a still life");
        for (const auto& c : l.cells) expect(liveNeighbors(scene(l), c.x, c.y, c.z) == 7, "lesson 4: every cube cell has 7 neighbors");
    }
    // 5. The whole oscillator scene has period 2.
    {
        const tutorial::Lesson& l = list[4];
        std::vector<CellSet> h = evolve(scene(l), lifeRules()[l.rule], 4);
        expect(h[1] != h[0] && h[2] == h[0] && h[4] == h[0], "lesson 5: blinker and toad scene repeats every 2 generations");
    }
    // 6. The glider scene glides.
    {
        const tutorial::Lesson& l = list[5];
        std::vector<CellSet> h = evolve(scene(l), lifeRules()[l.rule], 8);
        expect(h[4] == shifted(h[0], 1, 0, 1) && h[8] == shifted(h[0], 2, 0, 2), "lesson 6: Life 5766 glider moves (+1, 0, +1) every 4 generations");
    }
    // 7. Under 4555 the cube vanishes in one generation and the glider flies on undisturbed.
    {
        const tutorial::Lesson& l = list[6];
        expect(l.rule == tutorial::ruleIndexNamed("Life 4555"), "lesson 7 uses Life 4555");
        CellSet glider = toSet(translated(life4555Glider(), 1, 2, -2));
        std::vector<CellSet> h = evolve(scene(l), lifeRules()[l.rule], 12), g = evolve(glider, lifeRules()[l.rule], 12);
        expect(h[1] == g[1], "lesson 7: the cube is gone after one generation");
        expect(h[12] == shifted(glider, 3, 0, 3), "lesson 7: the 4555 glider keeps moving");
    }
    // 8. Under Crystal the cube dies and its 24 face neighbors are born.
    {
        const tutorial::Lesson& l = list[7];
        expect(l.rule == tutorial::ruleIndexNamed("Crystal"), "lesson 8 uses Crystal");
        CellSet now = scene(l), next = evolve(now, lifeRules()[l.rule], 1)[1];
        bool noneLeft = true;
        for (const auto& c : now) noneLeft &= !next.count(c);
        expect(next.size() == 24 && noneLeft, "lesson 8: Crystal replaces the cube with its 24 face neighbors");
    }
}

} // namespace

int main() {
    const LifeRule& life5766 = lifeRules()[tutorial::ruleIndexNamed("Life 5766")];
    const LifeRule& life4555 = lifeRules()[tutorial::ruleIndexNamed("Life 4555")];

    checkGlider("Life 5766 glider", life5766Glider(), life5766);
    checkGlider("Life 4555 glider", life4555Glider(), life4555);
    checkOscillator("Life 5766 blinker", life5766Blinker(), life5766, 2);
    checkOscillator("Life 5766 toad", life5766Toad(), life5766, 2);
    expect(evolve(toSet(life5766Block()), life5766, 1)[1] == toSet(life5766Block()), "2x2x2 block is a Life 5766 still life");
    expect(evolve(toSet(life5766Block()), life4555, 1)[1].empty(), "2x2x2 block dies at once under Life 4555");

    // Life 5766 contains Conway's Life as two-layer slabs.
    expect(life5766Glider() == doubledPattern({{1, 0}, {2, 1}, {0, 2}, {1, 2}, {2, 2}}), "Life 5766 glider is Conway's glider doubled");
    checkDoubledMatchesConway("Conway's glider", conwayGliderFlat(), 12);
    checkDoubledMatchesConway("blinker", flatPattern({{0, 1}, {1, 1}, {2, 1}}), 4);
    checkDoubledMatchesConway("toad", flatPattern({{1, 0}, {2, 0}, {3, 0}, {0, 1}, {1, 1}, {2, 1}}), 4);
    // A single-layer Conway glider under Conway's literal numbers does not glide.
    expect(findRepeat(evolve(toSet(conwayGliderFlat()), ruleNamed("Conway B3/S23"), 8)).period == 0,
           "Conway's glider does not glide under B3/S23 in 3D");

    checkLessons();

    if (failures) return 1;
    std::cout << "life3d patterns and tutorial scenes: all checks passed\n";
    return 0;
}

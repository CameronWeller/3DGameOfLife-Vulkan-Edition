// Checks for src/life/Patterns.h and the tutorial scenes in
// src/tutorial/TutorialLessons.cpp: every still life, oscillator period and
// glider shift the tutorial claims is verified with stepLifeReference.
#include <algorithm>
#include <iostream>
#include <set>
#include <string>
#include <tuple>
#include <vector>

#include "TestSupport.h"
#include "life/LifeRules.h"
#include "life/Patterns.h"
#include "tutorial/TutorialLessons.h"

using namespace gol3d;
using testing::expect;

namespace {

const LifeRule& ruleNamed(const char* name) {
    return lifeRules()[tutorial::ruleIndexNamed(name)];
}

using CellSet = std::set<std::tuple<int, int, int>>;

CellSet toSet(const Pattern& pattern) {
    CellSet out;
    for (const PatternCell& cell : pattern) {
        out.insert({cell.x, cell.y, cell.z});
    }
    return out;
}

CellSet shifted(const CellSet& cells, int dx, int dy, int dz) {
    CellSet out;
    for (auto [x, y, z] : cells) {
        out.insert({x + dx, y + dy, z + dz});
    }
    return out;
}

// The smallest box around a non-empty set of cells (inclusive corners). It
// starts inside out, so the first cell sets both corners.
constexpr int FAR_AWAY = 1 << 30; // beyond any coordinate a test reaches
struct Bounds {
    int low[3] = {FAR_AWAY, FAR_AWAY, FAR_AWAY};
    int high[3] = {-FAR_AWAY, -FAR_AWAY, -FAR_AWAY};
    int width(int axis) const { return high[axis] - low[axis] + 1; }
};

Bounds boundsOf(const CellSet& cells) {
    Bounds bounds;
    for (auto [x, y, z] : cells) {
        const int cell[3] = {x, y, z};
        for (int axis = 0; axis < 3; ++axis) {
            bounds.low[axis] = std::min(bounds.low[axis], cell[axis]);
            bounds.high[axis] = std::max(bounds.high[axis], cell[axis]);
        }
    }
    return bounds;
}

// Runs `generations` steps of the dense CPU reference in a box around the
// pattern. The box has room for the pattern to grow one cell per generation on
// every side, as fast as any pattern can, so the dead outside never changes the
// result: this is unbounded space.
std::vector<CellSet> evolve(const CellSet& start, const LifeRule& rule, int generations) {
    std::vector<CellSet> history{start};
    if (start.empty()) return history;
    const Bounds bounds = boundsOf(start);
    const int margin = generations + 2;
    int origin[3];
    int size[3];
    for (int axis = 0; axis < 3; ++axis) {
        origin[axis] = bounds.low[axis] - margin;
        size[axis] = bounds.width(axis) + 2 * margin;
    }
    auto index = [&](int x, int y, int z) {
        return (static_cast<size_t>(z) * size[1] + y) * size[0] + x;
    };

    std::vector<uint32_t> cells(static_cast<size_t>(size[0]) * size[1] * size[2], 0);
    std::vector<uint32_t> next;
    for (auto [x, y, z] : start) {
        cells[index(x - origin[0], y - origin[1], z - origin[2])] = 1;
    }
    for (int generation = 0; generation < generations; ++generation) {
        stepLifeReference(cells, next, size[0], size[1], size[2], rule);
        cells.swap(next);
        CellSet live;
        for (int z = 0; z < size[2]; ++z) {
            for (int y = 0; y < size[1]; ++y) {
                for (int x = 0; x < size[0]; ++x) {
                    if (cells[index(x, y, z)]) {
                        live.insert({x + origin[0], y + origin[1], z + origin[2]});
                    }
                }
            }
        }
        history.push_back(live);
    }
    return history;
}

// How many of the 26 cells around (x, y, z) are alive.
int liveNeighbors(const CellSet& cells, int x, int y, int z) {
    int count = 0;
    for (int dz = -1; dz <= 1; ++dz) {
        for (int dy = -1; dy <= 1; ++dy) {
            for (int dx = -1; dx <= 1; ++dx) {
                if ((dx || dy || dz) && cells.count({x + dx, y + dy, z + dz})) ++count;
            }
        }
    }
    return count;
}

// Conway's 2D Life (B3/S23, 8 neighbors) on (x, z) cells.
std::set<std::pair<int, int>> conwayStep(const std::set<std::pair<int, int>>& cells) {
    std::set<std::pair<int, int>> next;
    std::set<std::pair<int, int>> candidates;
    for (auto [x, z] : cells) {
        for (int dz = -1; dz <= 1; ++dz) {
            for (int dx = -1; dx <= 1; ++dx) {
                candidates.insert({x + dx, z + dz});
            }
        }
    }
    for (auto [x, z] : candidates) {
        int neighbors = 0;
        for (int dz = -1; dz <= 1; ++dz) {
            for (int dx = -1; dx <= 1; ++dx) {
                if ((dx || dz) && cells.count({x + dx, z + dz})) ++neighbors;
            }
        }
        const bool alive = cells.count({x, z});
        if (neighbors == 3 || (alive && neighbors == 2)) next.insert({x, z});
    }
    return next;
}

// When a pattern first comes back: the period is the first generation >= 1 at
// which it equals its start moved by (dx, dy, dz). A period of 0 means it never
// repeated within the history.
struct Repeat {
    int period = 0;
    int dx = 0;
    int dy = 0;
    int dz = 0;
};

Repeat findRepeat(const std::vector<CellSet>& history) {
    const CellSet& start = history[0];
    auto [x0, y0, z0] = *start.begin();
    for (size_t generation = 1; generation < history.size(); ++generation) {
        const CellSet& now = history[generation];
        if (now.size() != start.size() || now.empty()) continue;
        // A translation keeps the lexicographically smallest cell smallest, so
        // the shift, if any, is the difference between the two smallest cells.
        auto [x1, y1, z1] = *now.begin();
        if (shifted(start, x1 - x0, y1 - y0, z1 - z0) == now) {
            return {static_cast<int>(generation), x1 - x0, y1 - y0, z1 - z0};
        }
    }
    return {};
}

// Both of the game's gliders: 10 cells, period 4, one cell along +x and +z per
// period.
void checkGlider(const char* name, const Pattern& pattern, const LifeRule& rule) {
    constexpr int PERIOD = 4;
    constexpr int PERIODS_RUN = 5;
    constexpr size_t GLIDER_CELLS = 10;
    std::vector<CellSet> history = evolve(toSet(pattern), rule, PERIODS_RUN * PERIOD);
    const Repeat repeat = findRepeat(history);
    expect(repeat.period == PERIOD && repeat.dx == 1 && repeat.dy == 0 && repeat.dz == 1,
           std::string(name) + ": period 4, moves (+1, 0, +1) per period (got period " +
               std::to_string(repeat.period) + " shift " + std::to_string(repeat.dx) + "," +
               std::to_string(repeat.dy) + "," + std::to_string(repeat.dz) + ")");
    expect(history[PERIODS_RUN * PERIOD] == shifted(history[0], PERIODS_RUN, 0, PERIODS_RUN),
           std::string(name) + ": still gliding after 5 periods");
    for (const CellSet& phase : history) {
        expect(phase.size() == GLIDER_CELLS, std::string(name) + ": 10 cells in every phase");
    }
}

// Runs two periods, so findRepeat sees the first return to the start shape.
void checkOscillator(const char* name, const Pattern& pattern, const LifeRule& rule, int period) {
    std::vector<CellSet> history = evolve(toSet(pattern), rule, 2 * period);
    const Repeat repeat = findRepeat(history);
    expect(repeat.period == period && repeat.dx == 0 && repeat.dy == 0 && repeat.dz == 0,
           std::string(name) + ": period " + std::to_string(period) + " in place (got " +
               std::to_string(repeat.period) + ")");
}

// A doubled 2D pattern under Life 5766 follows Conway's 2D game exactly.
void checkDoubledMatchesConway(const char* name, const Pattern& flat, int generations) {
    std::set<std::pair<int, int>> conway;
    for (const PatternCell& cell : flat) {
        conway.insert({cell.x, cell.z});
    }
    auto doubled = [](const std::set<std::pair<int, int>>& cells) {
        CellSet out;
        for (auto [x, z] : cells) {
            out.insert({x, 0, z});
            out.insert({x, 1, z});
        }
        return out;
    };
    std::vector<CellSet> history = evolve(doubled(conway), ruleNamed("Life 5766"), generations);
    for (int generation = 1; generation <= generations; ++generation) {
        conway = conwayStep(conway);
        expect(history[generation] == doubled(conway),
               std::string(name) +
                   " doubled under Life 5766 matches Conway's 2D game at generation " +
                   std::to_string(generation));
    }
}

// The live cells a lesson's scene starts with.
CellSet sceneOf(const tutorial::Lesson& lesson) {
    return toSet(lesson.cells);
}

const LifeRule& ruleOf(const tutorial::Lesson& lesson) {
    return lifeRules()[lesson.rule];
}

// What every lesson needs: a valid rule, some content, and a camera that looks
// down at the scene, less steeply than 60 degrees.
void checkLessonBasics(const tutorial::Lesson& lesson) {
    expect(lesson.rule < lifeRules().size(), lesson.title + ": valid rule");
    expect(!lesson.cells.empty() && !lesson.paragraphs.empty() && !lesson.tryThis.empty(),
           lesson.title + ": content");
    float yaw = 0;
    float pitch = 0;
    tutorial::lookAngles(lesson, yaw, pitch);
    expect(pitch < 0.0f && pitch > -60.0f, lesson.title + ": camera looks down at the scene");
}

// Lesson 1: the 26 outlined neighbors surround the lone cell, which then dies.
void checkNeighborsLesson(const tutorial::Lesson& lesson) {
    CellSet marked;
    for (const tutorial::MarkedCell& mark : lesson.marks) {
        marked.insert({mark.cell.x, mark.cell.y, mark.cell.z});
    }
    const PatternCell& center = lesson.cells[0];
    bool allAdjacent = true;
    for (auto [x, y, z] : marked) {
        const int distance =
            std::max({std::abs(x - center.x), std::abs(y - center.y), std::abs(z - center.z)});
        allAdjacent &= distance == 1;
    }
    expect(lesson.cells.size() == 1 && marked.size() == 26 && allAdjacent,
           "lesson 1 outlines the 26 neighbors");
    expect(evolve(sceneOf(lesson), ruleOf(lesson), 1)[1].empty(), "lesson 1: a lone cell dies");
}

// Lesson 2: each outline's color matches its cell's count and fate.
void checkSurviveAndBirthLesson(const tutorial::Lesson& lesson) {
    expect(lesson.rule == tutorial::ruleIndexNamed("Life 5766"), "lesson 2 uses Life 5766");
    const CellSet now = sceneOf(lesson);
    const CellSet next = evolve(now, ruleOf(lesson), 1)[1];
    for (const tutorial::MarkedCell& mark : lesson.marks) {
        auto [x, y, z] = std::tuple(mark.cell.x, mark.cell.y, mark.cell.z);
        const int neighbors = liveNeighbors(now, x, y, z);
        const bool aliveBefore = now.count({x, y, z});
        const bool aliveAfter = next.count({x, y, z});
        if (mark.mark == tutorial::Mark::Born) {
            expect(!aliveBefore && neighbors == 6 && aliveAfter,
                   "lesson 2: green cells are empty with 6 and born");
        }
        if (mark.mark == tutorial::Mark::Dies) {
            expect(aliveBefore && neighbors == 3 && !aliveAfter,
                   "lesson 2: red cells are alive with 3 and die");
        }
        if (mark.mark == tutorial::Mark::Survives) {
            expect(aliveBefore && neighbors == 5 && aliveAfter,
                   "lesson 2: blue cells are alive with 5 and survive");
        }
    }
    expect(lesson.marks.size() == 10, "lesson 2 outlines every cell that changes or survives");
}

// Lesson 3: Conway's numbers explode, about one block per generation in every
// direction.
void checkConwayNumbersLesson(const tutorial::Lesson& lesson) {
    constexpr int GENERATIONS = 20;
    expect(lesson.rule == tutorial::ruleIndexNamed("Conway B3/S23"),
           "lesson 3 uses Conway's literal numbers");
    std::vector<CellSet> history = evolve(sceneOf(lesson), ruleOf(lesson), GENERATIONS);
    const CellSet& last = history[GENERATIONS];
    const Bounds bounds = boundsOf(last);
    const bool wide = bounds.width(0) >= 30 && bounds.width(1) >= 30 && bounds.width(2) >= 30;
    expect(last.size() > 2000 && wide,
           "lesson 3: Conway's glider under B3/S23 grows past 2000 cells, 30+ wide on every "
           "axis, in 20 generations");
    const CellSet& halfway = history[GENERATIONS / 2];
    expect(halfway.size() > 50 * history[0].size() && last.size() > 4 * halfway.size(),
           "lesson 3: population keeps multiplying");
}

// Lesson 4: the cube is a still life.
void checkStillLifeLesson(const tutorial::Lesson& lesson) {
    const CellSet cube = sceneOf(lesson);
    expect(evolve(cube, ruleOf(lesson), 1)[1] == cube, "lesson 4: the cube is a still life");
    for (const PatternCell& cell : lesson.cells) {
        expect(liveNeighbors(cube, cell.x, cell.y, cell.z) == 7,
               "lesson 4: every cube cell has 7 neighbors");
    }
}

// Lesson 5: the whole oscillator scene has period 2.
void checkOscillatorsLesson(const tutorial::Lesson& lesson) {
    std::vector<CellSet> history = evolve(sceneOf(lesson), ruleOf(lesson), 4);
    expect(history[1] != history[0] && history[2] == history[0] && history[4] == history[0],
           "lesson 5: blinker and toad scene repeats every 2 generations");
}

// Lesson 6: the glider scene glides.
void checkGliderLesson(const tutorial::Lesson& lesson) {
    std::vector<CellSet> history = evolve(sceneOf(lesson), ruleOf(lesson), 8);
    expect(history[4] == shifted(history[0], 1, 0, 1) && history[8] == shifted(history[0], 2, 0, 2),
           "lesson 6: Life 5766 glider moves (+1, 0, +1) every 4 generations");
}

// Lesson 7: under 4555 the cube vanishes in one generation and the glider flies
// on undisturbed, exactly as if it were alone.
void checkLife4555Lesson(const tutorial::Lesson& lesson) {
    constexpr int GENERATIONS = 12; // three periods
    expect(lesson.rule == tutorial::ruleIndexNamed("Life 4555"), "lesson 7 uses Life 4555");
    // The glider as placed in the scene (see TutorialLessons.cpp).
    const CellSet glider = toSet(translated(life4555Glider(), 1, 2, -2));
    std::vector<CellSet> scene = evolve(sceneOf(lesson), ruleOf(lesson), GENERATIONS);
    std::vector<CellSet> alone = evolve(glider, ruleOf(lesson), GENERATIONS);
    expect(scene[1] == alone[1], "lesson 7: the cube is gone after one generation");
    expect(scene[GENERATIONS] == shifted(glider, 3, 0, 3),
           "lesson 7: the 4555 glider keeps moving");
}

// Lesson 8: under Crystal the cube dies and its 24 face neighbors are born.
void checkBeyondLifeLesson(const tutorial::Lesson& lesson) {
    expect(lesson.rule == tutorial::ruleIndexNamed("Crystal"), "lesson 8 uses Crystal");
    const CellSet now = sceneOf(lesson);
    const CellSet next = evolve(now, ruleOf(lesson), 1)[1];
    bool noneLeft = true;
    for (const auto& cell : now) {
        noneLeft &= !next.count(cell);
    }
    expect(next.size() == 24 && noneLeft,
           "lesson 8: Crystal replaces the cube with its 24 face neighbors");
}

void checkLessons() {
    const std::vector<tutorial::Lesson>& list = tutorial::lessons();
    expect(list.size() == 8, "eight lessons");
    for (const tutorial::Lesson& lesson : list) {
        checkLessonBasics(lesson);
    }
    if (list.size() < 8) return;
    checkNeighborsLesson(list[0]);
    checkSurviveAndBirthLesson(list[1]);
    checkConwayNumbersLesson(list[2]);
    checkStillLifeLesson(list[3]);
    checkOscillatorsLesson(list[4]);
    checkGliderLesson(list[5]);
    checkLife4555Lesson(list[6]);
    checkBeyondLifeLesson(list[7]);
}

} // namespace

int main() {
    const LifeRule& life5766 = lifeRules()[tutorial::ruleIndexNamed("Life 5766")];
    const LifeRule& life4555 = lifeRules()[tutorial::ruleIndexNamed("Life 4555")];

    checkGlider("Life 5766 glider", life5766Glider(), life5766);
    checkGlider("Life 4555 glider", life4555Glider(), life4555);
    checkOscillator("Life 5766 blinker", life5766Blinker(), life5766, 2);
    checkOscillator("Life 5766 toad", life5766Toad(), life5766, 2);
    expect(evolve(toSet(life5766Block()), life5766, 1)[1] == toSet(life5766Block()),
           "2x2x2 block is a Life 5766 still life");
    expect(evolve(toSet(life5766Block()), life4555, 1)[1].empty(),
           "2x2x2 block dies at once under Life 4555");

    // Life 5766 contains Conway's Life as two-layer slabs.
    expect(life5766Glider() == doubledPattern({{1, 0}, {2, 1}, {0, 2}, {1, 2}, {2, 2}}),
           "Life 5766 glider is Conway's glider doubled");
    checkDoubledMatchesConway("Conway's glider", conwayGliderFlat(), 12);
    checkDoubledMatchesConway("blinker", flatPattern({{0, 1}, {1, 1}, {2, 1}}), 4);
    checkDoubledMatchesConway("toad", flatPattern({{1, 0}, {2, 0}, {3, 0}, {0, 1}, {1, 1}, {2, 1}}),
                              4);
    // A single-layer Conway glider under Conway's literal numbers does not glide.
    expect(findRepeat(evolve(toSet(conwayGliderFlat()), ruleNamed("Conway B3/S23"), 8)).period == 0,
           "Conway's glider does not glide under B3/S23 in 3D");

    checkLessons();

    return testing::finish("patterns and tutorial scenes");
}

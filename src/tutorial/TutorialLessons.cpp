#include "TutorialLessons.h"

#include <cmath>
#include <stdexcept>

#include "Life3DRules.h"

namespace VulkanHIP::tutorial {

size_t ruleIndexNamed(const std::string& name) {
    for (size_t i = 0; i < lifeRules().size(); ++i) {
        if (name == lifeRules()[i].name) return i;
    }
    throw std::runtime_error("Tutorial: no rule named " + name);
}

void lookAngles(const Lesson& lesson, float& yawDegrees, float& pitchDegrees) {
    float d[3];
    for (int i = 0; i < 3; ++i) d[i] = lesson.lookAt[i] - lesson.eye[i];
    float length = std::sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
    constexpr float DEGREES = 57.2957795f;
    // The lesson panel covers the right part of the screen, so turn a little to the
    // right to put the scene in the open part on the left.
    constexpr float PANEL_YAW_OFFSET = 20.0f;
    yawDegrees = std::atan2(d[2], d[0]) * DEGREES + PANEL_YAW_OFFSET;
    pitchDegrees = length > 0.0f ? std::asin(d[1] / length) * DEGREES : 0.0f;
}

namespace {

// explainRule without its closing reminder that cells have 26 neighbors.
std::string ruleInWords(size_t rule) {
    std::string text = explainRule(lifeRules()[rule]);
    size_t cut = text.find(" Every cell has 26");
    return cut == std::string::npos ? text : text.substr(0, cut);
}

std::string ruleLabel(size_t rule) {
    return std::string(lifeRules()[rule].name) + " (" + describeRule(lifeRules()[rule]) + ")";
}

void setCamera(Lesson& lesson, float ex, float ey, float ez, float tx, float ty, float tz) {
    lesson.eye[0] = ex, lesson.eye[1] = ey, lesson.eye[2] = ez;
    lesson.lookAt[0] = tx, lesson.lookAt[1] = ty, lesson.lookAt[2] = tz;
}

void append(Pattern& to, const Pattern& from) { to.insert(to.end(), from.begin(), from.end()); }

std::vector<Lesson> buildLessons() {
    const size_t life5766 = ruleIndexNamed("Life 5766");
    const size_t life4555 = ruleIndexNamed("Life 4555");
    const size_t conway = ruleIndexNamed("Conway B3/S23");
    const size_t crystal = ruleIndexNamed("Crystal");
    std::vector<Lesson> list;

    {
        Lesson l;
        l.title = "Neighbors";
        l.paragraphs = {
            "Conway's Game of Life is played on a flat grid. Each cell has 8 neighbors: the 3x3 square around it.",
            "This world is 3D. Each block has 26 neighbors: the 3x3x3 cube around it, outlined here.",
            "Every rule in this game counts how many of those 26 are alive."};
        l.legend = {{Mark::Neighbor, "the 26 neighbors of the block"}};
        l.tryThis = "Press N to step one generation. A lone block has 0 live neighbors, so it dies.";
        l.rule = life5766;
        l.cells = {{0, 3, 0}};
        for (int dz = -1; dz <= 1; ++dz)
            for (int dy = -1; dy <= 1; ++dy)
                for (int dx = -1; dx <= 1; ++dx)
                    if (dx || dy || dz) l.marks.push_back({{dx, 3 + dy, dz}, Mark::Neighbor});
        setCamera(l, 4.5f, 6.5f, 6.0f, 0.5f, 3.5f, 0.5f);
        list.push_back(l);
    }
    {
        Lesson l;
        l.title = "Survive and birth";
        l.paragraphs = {
            "Each generation, all blocks update at once. A live block survives if its live-neighbor count is on the "
            "survive list (S); an empty cell is born if its count is on the birth list (B).",
            "Conway's rule is S23/B3. This world uses Carter Bays' " + ruleLabel(life5766) + ". " +
                ruleInWords(life5766)};
        l.legend = {{Mark::Born, "empty, 6 live neighbors: born"},
                    {Mark::Dies, "alive, 3 live neighbors: dies"},
                    {Mark::Survives, "alive, 5 live neighbors: survives"}};
        l.tryThis = "Press N. The outlines stay put so you can check each one.";
        l.rule = life5766;
        l.cells = translated(life5766Blinker(), -1, 3, -1); // x -1..1, y 3..4, z 0
        for (int y = 3; y <= 4; ++y) {
            l.marks.push_back({{0, y, -1}, Mark::Born});
            l.marks.push_back({{0, y, 1}, Mark::Born});
            l.marks.push_back({{-1, y, 0}, Mark::Dies});
            l.marks.push_back({{1, y, 0}, Mark::Dies});
            l.marks.push_back({{0, y, 0}, Mark::Survives});
        }
        setCamera(l, 3.5f, 7.5f, 5.5f, 0.5f, 4.0f, 0.5f);
        list.push_back(l);
    }
    {
        Lesson l;
        l.title = "Conway's numbers in 3D";
        l.paragraphs = {
            "This is Conway's glider, one layer thick, under Conway's own numbers, " + ruleLabel(conway) + ". " +
                ruleInWords(conway),
            "In 2D it glides. In 3D the cells above and below the layer count neighbors too, and 3 live blocks "
            "out of 26 are easy to find. Births outrun deaths, and it never stops growing."};
        l.tryThis = "Press G to run (G again pauses). It spreads about one block per generation in every direction.";
        l.rule = conway;
        l.cells = translated(conwayGliderFlat(), -1, 3, -1);
        setCamera(l, 15.0f, 15.0f, 21.0f, 0.5f, 3.5f, 0.5f);
        list.push_back(l);
    }
    {
        Lesson l;
        l.title = "Life 5766: a still life";
        l.paragraphs = {
            "Carter Bays searched for 3D rules that behave like Conway's (1987). The best is " + ruleLabel(life5766) +
                ".",
            "This cube is Conway's 2x2 block, two layers thick. Each block touches the other 7, and 7 is in 5 to 7, "
            "so all survive. No empty cell touches exactly 6, so nothing is born.",
            "It never changes: a still life, just like the block in Conway's game."};
        l.tryThis = "Press N a few times. Then click the world to look around and right-click a block to remove it.";
        l.rule = life5766;
        l.cells = translated(life5766Block(), 0, 3, 0);
        setCamera(l, 5.0f, 6.5f, 6.0f, 1.0f, 4.0f, 1.0f);
        list.push_back(l);
    }
    {
        Lesson l;
        l.title = "Life 5766: oscillators";
        l.paragraphs = {
            "Why does 5766 act like Conway's rule? Stack a flat pattern two layers thick. A live block with n flat "
            "neighbors then has 2n+1 live neighbors (n per layer, plus its twin); an empty cell has 2n.",
            "Survive with 5 to 7 means n = 2 or 3; born with 6 means n = 3. That is Conway's S23/B3.",
            "So Conway's blinker (left) and toad (right) work here too, two layers thick. Both repeat every 2 "
            "generations."};
        l.tryThis = "Press N to flip them, or G to run.";
        l.rule = life5766;
        l.cells = translated(life5766Blinker(), -5, 3, -1);
        append(l.cells, translated(life5766Toad(), 2, 3, -1));
        setCamera(l, -1.0f, 13.0f, 14.0f, 0.0f, 3.5f, 0.5f);
        list.push_back(l);
    }
    {
        Lesson l;
        l.title = "Life 5766: the glider";
        l.paragraphs = {
            "The same trick gives Bays' glider: Conway's glider, two layers thick (10 blocks).",
            "Every 4 generations it is back in its first shape, one block further along x and along z. Unlike "
            "Conway's numbers in lesson 3, it stays 10 blocks forever."};
        l.tryThis = "Press G to run. Double-tap Space to fly, then follow it with WASD.";
        l.rule = life5766;
        l.cells = translated(life5766Glider(), -4, 3, -4);
        setCamera(l, 3.0f, 12.0f, -9.0f, -0.5f, 3.5f, -0.5f);
        list.push_back(l);
    }
    {
        Lesson l;
        l.title = "Life 4555";
        l.paragraphs = {
            "Bays' other Life-like rule is " + ruleLabel(life4555) + ". " + ruleInWords(life4555),
            "The cube (left) dies here: 7 neighbors is too many. Doubled Conway patterns fail too: an empty cell in "
            "a two-layer slab has an even count, never 5.",
            "4555 has its own 10-block glider (right), also period 4."};
        l.tryThis = "Press N once to see the cube vanish, then G to run the glider.";
        l.rule = life4555;
        l.cells = translated(life5766Block(), -5, 3, -1);
        append(l.cells, translated(life4555Glider(), 1, 2, -2));
        setCamera(l, -1.0f, 9.0f, 10.0f, -1.0f, 3.5f, 0.0f);
        list.push_back(l);
    }
    {
        Lesson l;
        l.title = "Beyond Life";
        l.paragraphs = {
            "The other rules are not Life-like: nothing glides, and most never settle.",
            "Clouds (" + describeRule(lifeRules()[ruleIndexNamed("Clouds")]) +
                ") erodes dense soups into blobs. Crystal, Slow Crystal, Coral and Architecture grow without end.",
            "Here is the cube again under " + ruleLabel(crystal) + ". 7 is not 4, so the cube dies, and its 24 "
            "face neighbors (4 live neighbors each) are born."};
        l.tryThis = "Press G to run. R switches rules; Replay resets the cube. Esc > Tutorial reopens these lessons.";
        l.rule = crystal;
        l.cells = translated(life5766Block(), 0, 3, 0);
        setCamera(l, 9.0f, 11.0f, 12.0f, 1.0f, 4.0f, 1.0f);
        list.push_back(l);
    }
    return list;
}

} // namespace

const std::vector<Lesson>& lessons() {
    static const std::vector<Lesson> list = buildLessons();
    return list;
}

} // namespace VulkanHIP::tutorial

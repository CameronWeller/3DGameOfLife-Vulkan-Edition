// What is drawn over the world while playing: the status panel (top left), the
// name of a newly selected stamp (above the hotbar, which the HUD shader
// draws), message toasts, and the window title.

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <sstream>

#include <GLFW/glfw3.h>

#include "game/Game.h"
#include "ui/Theme.h"
#include "ui/Widgets.h"
#include "util/Units.h"

namespace gol3d {
namespace {

constexpr double TOAST_SECONDS = 4.0;
constexpr double FADE_OUT_SECONDS = 0.5; // toasts and stamp names fade over their last half second
constexpr int QUARTER_TURN_DEGREES = 90;
constexpr float TITLE_REFRESH_SECONDS = 0.25f;

// The hotbar's metrics in pixels at scale 1, as in shaders/life3d_screen.frag
// (HUD_SCALE_HEIGHT, HOTBAR_SLOT). The stamp name sits STAMP_NAME_LIFT above
// the hotbar's slots: the shader's 10-pixel bottom margin plus a 6-pixel gap.
constexpr float HUD_SCALE_HEIGHT = 540.0f;
constexpr float HOTBAR_SLOT = 40.0f;
constexpr float STAMP_NAME_LIFT = 16.0f;

// The run-state icon in the status panel, in fractions of its line height: a
// play triangle, or two pause bars, both ICON_WIDTH wide.
constexpr float ICON_WIDTH = 0.62f;

// The population graph's "peak" label is drawn at this fraction of the font
// size; its measured width is scaled to match.
constexpr float PEAK_LABEL_SCALE = 0.8f;

// Alpha for something that fades out as `secondsLeft` reaches zero.
int fadeOutAlpha(double secondsLeft) {
    return static_cast<int>(255.0 * std::min(1.0, secondsLeft / FADE_OUT_SECONDS));
}

// A play triangle in `accent` while the simulation runs, two pause bars in
// `muted` while it is paused. (x, y) is its top left; `icon` its height, the
// line height.
void drawRunStateIcon(ImDrawList* draw, float x, float y, float icon, bool running, ImU32 accent,
                      ImU32 muted) {
    using ui::px;
    if (running) {
        draw->AddTriangleFilled(ImVec2(x, y + icon * 0.15f), ImVec2(x, y + icon * 0.85f),
                                ImVec2(x + icon * ICON_WIDTH, y + icon * 0.5f), accent);
    } else {
        draw->AddRectFilled(ImVec2(x, y + icon * 0.18f), ImVec2(x + icon * 0.22f, y + icon * 0.82f),
                            muted, px(1));
        draw->AddRectFilled(ImVec2(x + icon * 0.4f, y + icon * 0.18f),
                            ImVec2(x + icon * ICON_WIDTH, y + icon * 0.82f), muted, px(1));
    }
}

} // namespace

void Game::notify(const std::string& message) {
    std::cout << message << std::endl;
    toast_ = message;
    toastUntil_ = glfwGetTime() + TOAST_SECONDS;
}

std::string Game::handName() const {
    return brush_.emptyHand() ? "Empty hand" : stampInfos()[brush_.hotbarSlot].name;
}

std::string Game::handLabel() const {
    std::string label = handName();
    if (brush_.emptyHand()) return label;
    if (brush_.material != CellKind::Life) {
        label += std::string(" of ") + cellType(brush_.material).name;
    }
    if (brush_.rotation != 0) {
        label += " (rotated " + std::to_string(brush_.rotation * QUARTER_TURN_DEGREES) + " deg)";
    }
    if (brush_.tilt != 0) {
        label += " (tilted " + std::to_string(brush_.tilt * QUARTER_TURN_DEGREES) + " deg)";
    }
    return label;
}

// The speed line: what the simulation is doing, colored by state.
Game::HudLine Game::speedLine() const {
    const std::string reached = SimulationSpeed::formatRate(rateMeter_.rate()) + " gen/s";
    if (fastForward_ > 0) {
        return {"fast-forward  " + ui::withCommas(fastForwardTotal_ - fastForward_) + " / " +
                    ui::withCommas(fastForwardTotal_),
                ui::accentColor()};
    }
    if (!running_) {
        return {"paused  (" + speed_.label() + ", G to run)", ui::color(ui::palette::HUD_TEXT)};
    }
    if (speed_.unlimited()) return {"max speed  " + reached, ui::accentColor()};
    if (slowdown_.active()) {
        return {speed_.label() + "  ->  " + reached + " (slowed to keep up)",
                ui::color(ui::palette::SLOWED)};
    }
    return {speed_.label(), ui::accentColor()};
}

// The F3 lines: rule notation, position, chunk, facing, memory, drawing and timing.
std::vector<Game::HudLine> Game::debugLines() const {
    const ImU32 detail = ui::color(ui::palette::HUD_DETAIL);
    const glm::vec3 feet = player_.feet();
    const glm::ivec3 chunk = chunkOf(glm::ivec3(glm::floor(player_.eye)));
    const PassCosts& costs = passes_.costs();
    std::ostringstream position;
    std::ostringstream chunkLine;
    std::ostringstream facing;
    std::ostringstream chunks;
    std::ostringstream blocks;
    std::ostringstream timing;
    position << std::fixed << std::setprecision(3) << "XYZ: " << feet.x << " / " << feet.y << " / "
             << feet.z;
    chunkLine << "Chunk: " << chunk.x << " " << chunk.y << " " << chunk.z << "  |  "
              << (player_.flying ? "flying" : "walking") << (player_.onGround ? ", on ground" : "");
    facing << std::fixed << std::setprecision(1) << "Facing: yaw " << player_.yaw << ", pitch "
           << player_.pitch;
    chunks << ui::withCommas(world_.activeChunkCount()) << " chunks of "
           << ui::withCommas(world_.capacity()) << " (limit " << ui::withCommas(world_.chunkLimit())
           << ")" << (world_.limitReached() ? " LIMIT" : "") << "  |  GPU "
           << toMegabytes(buffers_.bytesAllocated()) << " MB";
    blocks << ui::withCommas(drawnBlocks_) << " blocks drawn"
           << (visibleBlocks_ > drawnBlocks_ ? " (capped)" : "");
    timing << std::fixed << std::setprecision(2) << "GPU " << costs.stepMs << " ms/gen, "
           << costs.drawMs << " ms/block list  |  sim " << std::setprecision(1) << lastSimMs_
           << " ms/frame  |  " << std::lround(fps_) << " fps";

    std::vector<HudLine> lines;
    for (const std::string& text : {describeRule(rule()), position.str(), chunkLine.str(),
                                    facing.str(), chunks.str(), blocks.str(), timing.str()}) {
        lines.push_back({text, detail});
    }
    if (target_.hit) {
        std::optional<CellKind> kind = world_.cellKind(target_.block);
        std::string name = kind ? cellType(*kind).name : "?";
        lines.push_back({"Targeted: " + name + " at " + std::to_string(target_.block.x) + " " +
                             std::to_string(target_.block.y) + " " +
                             std::to_string(target_.block.z),
                         detail});
    }
    return lines;
}

// The status panel at the top left: run state and speed (with what the governor
// actually reaches), population with its recent history, the building material,
// a fast-forward's progress, and with F3 the debug lines.
void Game::drawHud() {
    if (!hudVisible_) return;
    const std::string title = std::string(rule().name) + "   gen " + ui::withCommas(generation_);
    const HudLine speed = speedLine();
    std::vector<HudLine> lines = {speed, {ui::withCommas(population_) + " alive", IM_COL32_WHITE}};
    if (showDebug_) {
        for (const HudLine& line : debugLines()) {
            lines.push_back(line);
        }
    }
    drawStatusPanel(title, speed.color, lines);
    drawSelectedStampName(ImGui::GetForegroundDrawList());
}

void Game::drawStatusPanel(const std::string& title, ImU32 accent,
                           const std::vector<HudLine>& lines) {
    using ui::px;
    ImDrawList* draw = ImGui::GetForegroundDrawList();
    const float line = ImGui::GetTextLineHeight();
    const ImU32 white = IM_COL32_WHITE;
    const ImU32 muted = ui::color(ui::palette::HUD_TEXT);
    const std::string materialText = std::string("building with ") + cellType(brush_.material).name;

    // Panel geometry: title, lines, material line, then the population graph.
    const float icon = line;
    float width = ImGui::CalcTextSize(title.c_str()).x + icon + px(6);
    for (const HudLine& row : lines) {
        width = std::max(width, ImGui::CalcTextSize(row.text.c_str()).x);
    }
    width = std::max(width, ImGui::CalcTextSize(materialText.c_str()).x + line);
    width = std::max(width, px(200));
    const float graphHeight = line * 1.6f;
    const bool showGraph = populationHistory_.size() >= 2;
    const bool showProgress = fastForward_ > 0;
    float height = line * static_cast<float>(lines.size() + 2) + px(12);
    if (showGraph) height += graphHeight + px(6);
    if (showProgress) height += px(8);
    const ImVec2 min(px(8), px(8));
    const ImVec2 max(min.x + width + px(22), min.y + height);
    draw->AddRectFilled(min, max, ui::color(ui::palette::PANEL, 180), px(8));
    draw->AddRect(min, max, ui::color(ui::palette::BORDER, 50), px(8), px(1));
    // A stripe down the left edge in the speed's color.
    draw->AddRectFilled(min, ImVec2(min.x + px(3), max.y), accent, px(8),
                        ImDrawFlags_RoundCornersLeft);

    // Run state icon, then the title.
    float x = min.x + px(12);
    float y = min.y + px(6);
    drawRunStateIcon(draw, x, y, icon, running_ || fastForward_ > 0, accent, muted);
    draw->AddText(ImVec2(x + icon * ICON_WIDTH + px(6), y), white, title.c_str());
    y += line;
    for (const HudLine& row : lines) {
        draw->AddText(ImVec2(x, y), row.color, row.text.c_str());
        y += line;
    }

    // Material swatch and name.
    const float swatch = line * 0.62f;
    const ImVec2 swatchMin(x, y + (line - swatch) * 0.5f);
    draw->AddRectFilled(swatchMin, ImVec2(swatchMin.x + swatch, swatchMin.y + swatch),
                        ui::materialColor(brush_.material), px(2));
    draw->AddText(ImVec2(x + swatch + px(6), y), muted, materialText.c_str());
    y += line;

    if (showProgress && fastForwardTotal_ > 0) {
        float doneFraction = static_cast<float>(fastForwardTotal_ - fastForward_) /
                             static_cast<float>(fastForwardTotal_);
        ImVec2 barMin(x, y + px(2));
        ImVec2 barMax(min.x + width + px(10), y + px(6));
        draw->AddRectFilled(barMin, barMax, ui::color(ui::palette::TRACK), px(2));
        const float filledRight = barMin.x + (barMax.x - barMin.x) * doneFraction;
        draw->AddRectFilled(barMin, ImVec2(filledRight, barMax.y), ui::accentColor(), px(2));
        y += px(8);
    }
    if (showGraph) {
        drawPopulationGraph(draw, ImVec2(x, y + px(4)),
                            ImVec2(min.x + width + px(10), y + px(4) + graphHeight));
    }
}

// Population over the last generations as a filled sparkline, scaled to its peak.
void Game::drawPopulationGraph(ImDrawList* draw, ImVec2 min, ImVec2 max) {
    using ui::px;
    float peak = 1.0f;
    for (float value : populationHistory_) {
        peak = std::max(peak, value);
    }
    const size_t count = populationHistory_.size();
    // Sample i, from the oldest at the left edge to the newest at the right.
    auto point = [&](size_t i) {
        float across = static_cast<float>(i) / static_cast<float>(count - 1);
        return ImVec2(min.x + across * (max.x - min.x),
                      max.y - (max.y - min.y) * populationHistory_[i] / peak);
    };
    draw->AddLine(ImVec2(min.x, max.y), max, ui::color(ui::palette::BORDER, 60), px(1)); // baseline
    // The fill: one quad from the baseline up to each segment of the line.
    for (size_t i = 0; i + 1 < count; ++i) {
        ImVec2 left = point(i);
        ImVec2 right = point(i + 1);
        draw->AddQuadFilled(ImVec2(left.x, max.y), left, right, ImVec2(right.x, max.y),
                            ui::accentColor(46));
    }
    std::vector<ImVec2> points(count);
    for (size_t i = 0; i < count; ++i) {
        points[i] = point(i);
    }
    draw->AddPolyline(points.data(), static_cast<int>(count), ui::accentColor(220), px(1.5f));
    const std::string label = "peak " + ui::withCommas(static_cast<uint64_t>(peak));
    const ImVec2 labelSize = ImGui::CalcTextSize(label.c_str());
    draw->AddText(ImGui::GetFont(), ImGui::GetFontSize() * PEAK_LABEL_SCALE,
                  ImVec2(max.x - labelSize.x * PEAK_LABEL_SCALE, min.y - px(2)),
                  ui::color(ui::palette::HUD_TEXT, 200), label.c_str());
}

// The selected stamp's name above the hotbar, fading out. The hotbar itself is
// drawn by hotbar() in shaders/life3d_screen.frag, whose scale (whole steps
// with the window height) this repeats.
void Game::drawSelectedStampName(ImDrawList* draw) {
    const double secondsLeft = slotNameUntil_ - glfwGetTime();
    if (secondsLeft <= 0.0 || brush_.emptyHand()) return;
    const ImVec2 screen = ImGui::GetIO().DisplaySize;
    const float hotbarScale = std::max(1.0f, std::floor(screen.y / HUD_SCALE_HEIGHT));
    const float slotSize = HOTBAR_SLOT * hotbarScale;
    const std::string name = handLabel();
    const ImVec2 textSize = ImGui::CalcTextSize(name.c_str());
    const ImVec2 position((screen.x - textSize.x) * 0.5f,
                          screen.y - slotSize - STAMP_NAME_LIFT * hotbarScale - textSize.y);
    ui::shadowText(draw, position, name, IM_COL32(255, 255, 255, fadeOutAlpha(secondsLeft)));
}

// A rounded message pill near the top of the screen.
void Game::drawToast() {
    using ui::px;
    const double secondsLeft = toastUntil_ - glfwGetTime();
    if (secondsLeft <= 0.0 || toast_.empty()) return;
    ImDrawList* draw = ImGui::GetForegroundDrawList();
    const ImVec2 screen = ImGui::GetIO().DisplaySize;
    const ImVec2 textSize = ImGui::CalcTextSize(toast_.c_str());
    const int alpha = fadeOutAlpha(secondsLeft);
    const ImVec2 position((screen.x - textSize.x) * 0.5f, screen.y * 0.1f);
    const ImVec2 min(position.x - px(14), position.y - px(6));
    const ImVec2 max(position.x + textSize.x + px(14), position.y + textSize.y + px(6));
    const float radius = (max.y - min.y) * 0.5f;
    draw->AddRectFilled(min, max, ui::color(ui::palette::PANEL, alpha * 9 / 10), radius);
    draw->AddRect(min, max, ui::accentColor(alpha * 2 / 3), radius, px(1));
    draw->AddText(position, IM_COL32(255, 255, 255, alpha), toast_.c_str());
}

// The window title doubles as a compact status line (and a frame-rate meter),
// refreshed four times a second.
void Game::updateWindowTitle(float deltaTime) {
    titleTimer_ += deltaTime;
    titleFrames_++;
    if (titleTimer_ < TITLE_REFRESH_SECONDS) return;
    fps_ = static_cast<float>(titleFrames_) / titleTimer_;
    titleTimer_ = 0.0f;
    titleFrames_ = 0;
    const glm::vec3 feet = player_.feet();
    std::ostringstream title;
    title << std::fixed << std::setprecision(1) << "3D Life  |  " << rule().name << " "
          << describeRule(rule()) << "  |  gen " << generation_ << "  |  " << population_
          << " alive"
          << "  |  " << world_.activeChunkCount() << " chunks"
          << (world_.limitReached() ? " (LIMIT)" : "") << "  |  " << speed_.label() << " "
          << (running_ ? "running" : "paused") << "  |  " << (player_.flying ? "flying" : "walking")
          << " XYZ " << feet.x << " " << feet.y << " " << feet.z << "  |  "
          << (brush_.hotbarSlot + 1) << " " << handName() << "  |  " << std::lround(fps_) << " fps";
    glfwSetWindowTitle(window_, title.str().c_str());
}

} // namespace gol3d

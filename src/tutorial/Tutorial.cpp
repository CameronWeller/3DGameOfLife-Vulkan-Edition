// The tutorial's lesson panel, drawn with Dear ImGui at the right edge of the
// screen over the running world. It shows the current lesson's text and legend,
// a status line about the world, and buttons to move between lessons.

#include "tutorial/Tutorial.h"

#include <algorithm>
#include <cmath>

#include <GLFW/glfw3.h>

#include "imgui.h"

#include "ui/Theme.h"
#include "ui/Widgets.h"

namespace gol3d::tutorial {
namespace {

// Panel layout. Lengths are in pixels at GUI scale 1.
constexpr float PANEL_MARGIN = 4.0f; // gap to the screen edges
constexpr float PANEL_WIDTH = 270.0f;
constexpr float PANEL_MAX_WIDTH_FRACTION = 0.45f; // of the window width, for small windows
constexpr float PANEL_BACKGROUND_ALPHA = 0.72f;   // the world stays visible behind it
constexpr float PARAGRAPH_GAP = 2.0f;
constexpr float SWATCH_SIZE_IN_LINES = 0.7f; // legend swatch edge, as a fraction of a text line
constexpr float SWATCH_LINE_WIDTH = 2.0f;
constexpr int NAVIGATION_BUTTON_COUNT = 3; // Back, Replay, Next/Finish

// No keyboard navigation and no focus stealing: Space and the arrows belong to
// the game.
constexpr ImGuiWindowFlags PANEL_FLAGS =
    ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove |
    ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoFocusOnAppearing;

// Pins the next window to the top right corner, as tall as the content needs
// but no taller than the screen.
void placePanel(float scale) {
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const float margin = PANEL_MARGIN * scale;
    const float width = std::min(PANEL_WIDTH * scale, viewport->Size.x * PANEL_MAX_WIDTH_FRACTION);
    ImGui::SetNextWindowPos(
        ImVec2(viewport->Pos.x + viewport->Size.x - width - margin, viewport->Pos.y + margin));
    ImGui::SetNextWindowSizeConstraints(ImVec2(width, 0.0f),
                                        ImVec2(width, viewport->Size.y - 2.0f * margin));
    ImGui::SetNextWindowBgAlpha(PANEL_BACKGROUND_ALPHA);
}

// "Tutorial 3/8" with a Close button at the right. Returns true when Close was
// clicked.
bool drawHeader(size_t index, size_t count) {
    ImGui::TextDisabled("Tutorial %zu/%zu", index + 1, count);
    const char* closeLabel = "Close";
    const float closeWidth =
        ImGui::CalcTextSize(closeLabel).x + ImGui::GetStyle().FramePadding.x * 2.0f;
    ui::rightAlignNext(closeWidth);
    return ImGui::SmallButton(closeLabel);
}

// One line per outline color: a hollow square in that color, then its meaning.
void drawLegend(const std::vector<LegendEntry>& legend, float scale) {
    const float lineHeight = ImGui::GetTextLineHeight();
    const float swatch = lineHeight * SWATCH_SIZE_IN_LINES;
    for (const LegendEntry& entry : legend) {
        const glm::vec3 rgb = Tutorial::markColor(entry.mark);
        const ImVec2 at = ImGui::GetCursorScreenPos();
        const float inset = (lineHeight - swatch) * 0.5f; // centers the swatch in the line
        ImGui::GetWindowDrawList()->AddRect(
            ImVec2(at.x + inset, at.y + inset),
            ImVec2(at.x + inset + swatch, at.y + inset + swatch),
            ImGui::ColorConvertFloat4ToU32(ImVec4(rgb.r, rgb.g, rgb.b, 1.0f)), 0.0f,
            SWATCH_LINE_WIDTH * scale);
        // The square was drawn directly; reserve its space in the layout.
        ImGui::Dummy(ImVec2(lineHeight, lineHeight));
        ImGui::SameLine();
        ImGui::TextUnformatted(entry.text.c_str());
    }
}

// The title, paragraphs, legend and the "Try:" suggestion, wrapped to the panel.
void drawLessonText(const Lesson& lesson, float scale) {
    const ImVec4 titleColor = ui::linearFromSrgb(0.37f, 0.9f, 0.66f); // the menu's mint
    const ImVec4 tryColor = ui::linearFromSrgb(0.6f, 0.8f, 1.0f);     // light blue

    ImGui::PushStyleColor(ImGuiCol_Text, titleColor);
    ImGui::TextUnformatted(lesson.title.c_str());
    ImGui::PopStyleColor();
    ImGui::Separator();

    ImGui::PushTextWrapPos(0.0f); // wrap at the window's right edge
    for (const std::string& paragraph : lesson.paragraphs) {
        ImGui::TextUnformatted(paragraph.c_str());
        ImGui::Dummy(ImVec2(0.0f, PARAGRAPH_GAP * scale));
    }
    drawLegend(lesson.legend, scale);
    ImGui::Dummy(ImVec2(0.0f, PARAGRAPH_GAP * scale));
    ImGui::PushStyleColor(ImGuiCol_Text, tryColor);
    ImGui::TextUnformatted(("Try: " + lesson.tryThis).c_str());
    ImGui::PopStyleColor();
    ImGui::PopTextWrapPos();
}

// "S5-7/B6 | gen 12 | 40 alive".
void drawStatusLine(const Tutorial::Status& status) {
    ImGui::TextDisabled("%s | gen %llu | %llu alive", status.rule.c_str(),
                        static_cast<unsigned long long>(status.generation),
                        static_cast<unsigned long long>(status.population));
}

} // namespace

glm::vec3 Tutorial::markColor(Mark mark) {
    // Must match BOX_COLORS 3-6 in shaders/life3d_boxes.vert.
    static const glm::vec3 COLORS[4] = {
        {1.00f, 0.62f, 0.05f}, // Neighbor: amber
        {0.05f, 0.75f, 0.08f}, // Born: green
        {0.90f, 0.05f, 0.03f}, // Dies: red
        {0.10f, 0.35f, 1.00f}, // Survives: blue
    };
    return COLORS[static_cast<int>(mark)];
}

void Tutorial::open(size_t lesson) {
    isOpen_ = true;
    index_ = std::min(lesson, lessons().size() - 1);
}

Tutorial::Request Tutorial::go(size_t lesson) {
    if (lesson >= lessons().size()) return Request::None;
    index_ = lesson;
    return Request::LoadScene;
}

Tutorial::Request Tutorial::handleKey(int key) {
    if (!isOpen_) return Request::None;
    switch (key) {
        case GLFW_KEY_LEFT:
            return index_ > 0 ? go(index_ - 1) : Request::None;
        case GLFW_KEY_RIGHT:
            return go(index_ + 1);
        case GLFW_KEY_BACKSPACE:
            return Request::LoadScene;
        default:
            return Request::None;
    }
}

Tutorial::Request Tutorial::drawNavigation() {
    Request request = Request::None;
    const size_t count = lessons().size();
    // Three equal buttons filling the row.
    const float spacing = ImGui::GetStyle().ItemSpacing.x;
    const float buttonWidth =
        (ImGui::GetContentRegionAvail().x - 2.0f * spacing) / NAVIGATION_BUTTON_COUNT;
    const ImVec2 button(buttonWidth, ImGui::GetFrameHeight());

    ImGui::BeginDisabled(index_ == 0);
    if (ImGui::Button("< Back", button)) request = go(index_ - 1);
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button("Replay", button)) request = Request::LoadScene;
    ImGui::SameLine();
    // The last lesson's Next button closes the tutorial instead.
    if (index_ + 1 < count) {
        if (ImGui::Button("Next >", button)) request = go(index_ + 1);
    } else if (ImGui::Button("Finish", button)) {
        request = Request::Close;
    }
    ImGui::TextDisabled("Keys: Left/Right, Backspace");
    return request;
}

Tutorial::Request Tutorial::draw(float scale, const Status& status) {
    if (!isOpen_) return Request::None;
    Request request = Request::None;
    placePanel(scale);
    if (ImGui::Begin("##tutorial", nullptr, PANEL_FLAGS)) {
        if (drawHeader(index_, lessons().size())) request = Request::Close;
        drawLessonText(lesson(), scale);
        ImGui::Separator();
        drawStatusLine(status);
        // A navigation click overrides Close; at most one happens per frame.
        const Request navigation = drawNavigation();
        if (navigation != Request::None) request = navigation;
    }
    ImGui::End();
    if (request == Request::Close) close();
    return request;
}

} // namespace gol3d::tutorial

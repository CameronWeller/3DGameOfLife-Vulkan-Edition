#include "tutorial/Tutorial.h"

#include <algorithm>
#include <cmath>

#include <GLFW/glfw3.h>

#include "imgui.h"

#include "ui/Theme.h"
#include "ui/Widgets.h"

namespace gol3d::tutorial {

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

Tutorial::Request Tutorial::draw(float scale, const Status& status) {
    if (!isOpen_) return Request::None;
    Request request = Request::None;
    const Lesson& current = lesson();
    const size_t count = lessons().size();

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const float margin = 4.0f * scale;
    const float width = std::min(270.0f * scale, viewport->Size.x * 0.45f);
    ImGui::SetNextWindowPos(
        ImVec2(viewport->Pos.x + viewport->Size.x - width - margin, viewport->Pos.y + margin));
    ImGui::SetNextWindowSizeConstraints(ImVec2(width, 0.0f),
                                        ImVec2(width, viewport->Size.y - 2.0f * margin));
    ImGui::SetNextWindowBgAlpha(0.72f);
    // No keyboard navigation: Space and the arrows belong to the game.
    const ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration |
                                   ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove |
                                   ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoNav |
                                   ImGuiWindowFlags_NoFocusOnAppearing;
    if (ImGui::Begin("##tutorial", nullptr, flags)) {
        ImGui::TextDisabled("Tutorial %zu/%zu", index_ + 1, count);
        const char* closeLabel = "Close";
        float closeWidth =
            ImGui::CalcTextSize(closeLabel).x + ImGui::GetStyle().FramePadding.x * 2.0f;
        ui::rightAlignNext(closeWidth);
        if (ImGui::SmallButton(closeLabel)) request = Request::Close;

        const ImVec4 accent = ui::linearFromSrgb(0.37f, 0.9f, 0.66f); // the menu's mint
        ImGui::PushStyleColor(ImGuiCol_Text, accent);
        ImGui::TextUnformatted(current.title.c_str());
        ImGui::PopStyleColor();
        ImGui::Separator();

        ImGui::PushTextWrapPos(0.0f);
        for (const std::string& paragraph : current.paragraphs) {
            ImGui::TextUnformatted(paragraph.c_str());
            ImGui::Dummy(ImVec2(0.0f, 2.0f * scale));
        }
        const float swatch = ImGui::GetTextLineHeight() * 0.7f;
        for (const LegendEntry& entry : current.legend) {
            const glm::vec3 rgb = markColor(entry.mark);
            ImVec2 at = ImGui::GetCursorScreenPos();
            float inset = (ImGui::GetTextLineHeight() - swatch) * 0.5f;
            ImGui::GetWindowDrawList()->AddRect(
                ImVec2(at.x + inset, at.y + inset),
                ImVec2(at.x + inset + swatch, at.y + inset + swatch),
                ImGui::ColorConvertFloat4ToU32(ImVec4(rgb.r, rgb.g, rgb.b, 1.0f)), 0.0f,
                2.0f * scale);
            ImGui::Dummy(ImVec2(ImGui::GetTextLineHeight(), ImGui::GetTextLineHeight()));
            ImGui::SameLine();
            ImGui::TextUnformatted(entry.text.c_str());
        }
        ImGui::Dummy(ImVec2(0.0f, 2.0f * scale));
        ImGui::PushStyleColor(ImGuiCol_Text, ui::linearFromSrgb(0.6f, 0.8f, 1.0f));
        ImGui::TextUnformatted(("Try: " + current.tryThis).c_str());
        ImGui::PopStyleColor();
        ImGui::PopTextWrapPos();

        ImGui::Separator();
        ImGui::TextDisabled("%s | gen %llu | %llu alive", status.rule.c_str(),
                            static_cast<unsigned long long>(status.generation),
                            static_cast<unsigned long long>(status.population));

        const float spacing = ImGui::GetStyle().ItemSpacing.x;
        const ImVec2 button((ImGui::GetContentRegionAvail().x - 2.0f * spacing) / 3.0f,
                            ImGui::GetFrameHeight());
        ImGui::BeginDisabled(index_ == 0);
        if (ImGui::Button("< Back", button)) request = go(index_ - 1);
        ImGui::EndDisabled();
        ImGui::SameLine();
        if (ImGui::Button("Replay", button)) request = Request::LoadScene;
        ImGui::SameLine();
        if (index_ + 1 < count) {
            if (ImGui::Button("Next >", button)) request = go(index_ + 1);
        } else if (ImGui::Button("Finish", button)) {
            request = Request::Close;
        }
        ImGui::TextDisabled("Keys: Left/Right, Backspace");
    }
    ImGui::End();
    if (request == Request::Close) close();
    return request;
}

} // namespace gol3d::tutorial

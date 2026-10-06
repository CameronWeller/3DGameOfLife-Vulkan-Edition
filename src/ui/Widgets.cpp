#include "ui/Widgets.h"

#include <algorithm>
#include <cmath>

#include "ui/Theme.h"

namespace gol3d::ui {
namespace {

float layoutScale = 1.0f;
ImFont* layoutTitleFont = nullptr;

ImVec4 toVec4(ImU32 packed) {
    return ImGui::ColorConvertU32ToFloat4(packed);
}

} // namespace

void setLayout(float scale, ImFont* titleFont) {
    layoutScale = scale;
    layoutTitleFont = titleFont;
}

float scale() {
    return layoutScale;
}

float px(float value) {
    return value * layoutScale;
}

bool beginCard(const char* id, float width) {
    // A fullscreen, invisible window behind the card that only draws the dimming.
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->Pos);
    ImGui::SetNextWindowSize(viewport->Size);
    ImGui::SetNextWindowBgAlpha(0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::Begin((std::string(id) + "-backdrop").c_str(), nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus |
                     ImGuiWindowFlags_NoNav);
    // Dim the world, darker toward the top and bottom edges, so the card stands out.
    ImDrawList* draw = ImGui::GetWindowDrawList();
    const ImVec2 low = viewport->Pos;
    const ImVec2 high(viewport->Pos.x + viewport->Size.x, viewport->Pos.y + viewport->Size.y);
    const float band = viewport->Size.y * 0.35f;
    const ImU32 dim = color(palette::BACKDROP, 120);
    const ImU32 dark = color(palette::BACKDROP, 200);
    const ImU32 clear = color(palette::BACKDROP, 0);
    draw->AddRectFilled(low, high, dim);
    draw->AddRectFilledMultiColor(low, ImVec2(high.x, low.y + band), dark, dark, clear, clear);
    draw->AddRectFilledMultiColor(ImVec2(low.x, high.y - band), high, clear, clear, dark, dark);
    ImGui::End();
    ImGui::PopStyleVar(2);

    ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSizeConstraints(ImVec2(px(width), 0.0f),
                                        ImVec2(px(width), viewport->Size.y - px(24)));
    return ImGui::Begin(id, nullptr,
                        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                            ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize);
}

void cardHeader(const char* title, const char* subtitle) {
    ImGui::PushFont(layoutTitleFont, TITLE_FONT_SIZE);
    const float height = ImGui::GetFontSize();
    ImGui::PopFont();

    // Conway's glider, 3x3 cells, vertically centered on the title.
    static constexpr int GLIDER[5][2] = {{1, 0}, {2, 1}, {0, 2}, {1, 2}, {2, 2}};
    const float cell = std::floor(height / 3.4f);
    const ImVec2 at = ImGui::GetCursorScreenPos();
    const float top = at.y + std::floor((height - 3.0f * cell) * 0.5f);
    ImDrawList* draw = ImGui::GetWindowDrawList();
    for (const auto& [x, y] : GLIDER) {
        ImVec2 min(at.x + x * cell, top + y * cell);
        draw->AddRectFilled(ImVec2(min.x + px(1), min.y + px(1)),
                            ImVec2(min.x + cell - px(1), min.y + cell - px(1)), accentColor(),
                            px(1.5f));
    }
    ImGui::Dummy(ImVec2(3.0f * cell + px(6), height));
    ImGui::SameLine();

    ImGui::PushFont(layoutTitleFont, TITLE_FONT_SIZE);
    ImGui::TextUnformatted(title);
    ImGui::PopFont();
    if (subtitle) {
        ImVec2 size = ImGui::CalcTextSize(subtitle);
        rightAlignNext(size.x);
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (height - size.y) * 0.5f);
        ImGui::TextDisabled("%s", subtitle);
    }
    ImGui::Dummy(ImVec2(0, px(2)));
    ImGui::Separator();
    ImGui::Dummy(ImVec2(0, px(2)));
}

void sectionLabel(const char* text) {
    ImGui::PushStyleColor(ImGuiCol_Text, toVec4(accentColor()));
    ImGui::TextUnformatted(text);
    ImGui::PopStyleColor();
}

void note(const std::string& text, ImU32 textColor) {
    ImGui::PushStyleColor(ImGuiCol_Text, toVec4(textColor));
    ImGui::PushTextWrapPos(0.0f);
    ImGui::TextUnformatted(text.c_str());
    ImGui::PopTextWrapPos();
    ImGui::PopStyleColor();
}

void mutedText(const std::string& text) {
    note(text, color(palette::MUTED_TEXT));
}

void rightAlignNext(float width) {
    ImGui::SameLine();
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() +
                         std::max(0.0f, ImGui::GetContentRegionAvail().x - width));
}

void hoverHint(const char* text) {
    if (!ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) return;
    ImGui::SetNextWindowSize(ImVec2(ImGui::GetFontSize() * 18.0f, 0));
    ImGui::BeginTooltip();
    ImGui::TextWrapped("%s", text);
    ImGui::EndTooltip();
}

float buttonHeight() {
    return ImGui::GetFrameHeight() + px(6);
}

void pushAccentButton() {
    ImGui::PushStyleColor(ImGuiCol_Button, toVec4(accentColor()));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, toVec4(color(palette::ACCENT_HOVERED)));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, toVec4(color(palette::ACCENT_PRESSED)));
    ImGui::PushStyleColor(ImGuiCol_Text, toVec4(color(palette::ON_ACCENT)));
}

void popAccentButton() {
    ImGui::PopStyleColor(4);
}

bool menuItem(const char* label, const char* keyHint, ButtonKind kind) {
    const bool primary = kind == ButtonKind::Primary;
    const bool danger = kind == ButtonKind::Danger;
    if (primary) pushAccentButton();
    if (danger) {
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, toVec4(color(palette::DANGER_HOVERED)));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, toVec4(color(palette::DANGER_PRESSED)));
    }
    ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(primary ? 0.5f : 0.0f, 0.5f));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,
                        ImVec2(px(12), ImGui::GetStyle().FramePadding.y));
    const ImVec2 at = ImGui::GetCursorScreenPos();
    const float width = ImGui::GetContentRegionAvail().x;
    const bool clicked = ImGui::Button(label, ImVec2(width, buttonHeight()));
    ImGui::PopStyleVar(2);
    if (primary) popAccentButton();
    if (danger) ImGui::PopStyleColor(2);

    if (keyHint) {
        const ImVec2 size = ImGui::CalcTextSize(keyHint);
        const ImU32 hintColor =
            primary ? color(palette::ON_ACCENT, 170) : ImGui::GetColorU32(ImGuiCol_TextDisabled);
        ImVec2 position(at.x + width - size.x - px(12), at.y + (buttonHeight() - size.y) * 0.5f);
        ImGui::GetWindowDrawList()->AddText(position, hintColor, keyHint);
    }
    return clicked;
}

PairChoice buttonPair(const char* left, const char* right, ButtonKind rightKind) {
    const float half = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) * 0.5f;
    PairChoice choice = PairChoice::None;
    if (ImGui::Button(left, ImVec2(half, buttonHeight()))) choice = PairChoice::Left;
    ImGui::SameLine();
    const bool primary = rightKind == ButtonKind::Primary;
    if (primary) pushAccentButton();
    if (ImGui::Button(right, ImVec2(half, buttonHeight()))) choice = PairChoice::Right;
    if (primary) popAccentButton();
    return choice;
}

void statChips(std::initializer_list<std::pair<const char*, std::string>> chips) {
    ImDrawList* draw = ImGui::GetWindowDrawList();
    const float gap = px(6);
    const float count = static_cast<float>(chips.size());
    const float width = (ImGui::GetContentRegionAvail().x - gap * (count - 1.0f)) / count;
    const float labelSize = ImGui::GetFontSize() * 0.72f;
    const float height = labelSize + ImGui::GetFontSize() + px(14);
    ImVec2 at = ImGui::GetCursorScreenPos();
    for (const auto& [name, value] : chips) {
        const ImVec2 corner(at.x + width, at.y + height);
        draw->AddRectFilled(at, corner, color(255, 255, 255, 10), px(6));
        draw->AddRect(at, corner, color(palette::BORDER, 50), px(6), px(1));
        draw->AddText(ImGui::GetFont(), labelSize, ImVec2(at.x + px(8), at.y + px(5)),
                      color(palette::MUTED_TEXT), name);
        draw->AddText(ImVec2(at.x + px(8), at.y + px(7) + labelSize), IM_COL32(255, 255, 255, 255),
                      value.c_str());
        at.x += width + gap;
    }
    ImGui::Dummy(ImVec2(0, height));
}

void shadowText(ImDrawList* draw, ImVec2 position, const std::string& text, ImU32 textColor) {
    const int alpha = (textColor >> IM_COL32_A_SHIFT) & 0xFF;
    const ImU32 shadow = color(0, 0, 0, alpha * 3 / 5);
    draw->AddText(ImVec2(position.x + px(1), position.y + px(1)), shadow, text.c_str());
    draw->AddText(position, textColor, text.c_str());
}

void drawIcon5x5(ImDrawList* draw, ImVec2 min, float size, uint32_t bits, ImU32 iconColor) {
    const float cell = std::floor(size * 0.14f);
    const float inset = std::floor((size - 5.0f * cell) * 0.5f);
    const ImVec2 origin(min.x + inset, min.y + inset);
    for (int y = 0; y < 5; ++y) {
        for (int x = 0; x < 5; ++x) {
            const int bit = 24 - (y * 5 + x);
            if (!((bits >> bit) & 1u)) continue;
            draw->AddRectFilled(ImVec2(origin.x + x * cell, origin.y + y * cell),
                                ImVec2(origin.x + (x + 1) * cell, origin.y + (y + 1) * cell),
                                iconColor);
        }
    }
}

void drawCubeIcon(ImDrawList* draw, ImVec2 center, float size, const uint8_t rgb[3]) {
    auto shade = [&](float brightness) {
        return color(static_cast<int>(rgb[0] * brightness), static_cast<int>(rgb[1] * brightness),
                     static_cast<int>(rgb[2] * brightness));
    };
    const float halfWidth = size * 0.5f;
    const float step = size * 0.29f; // vertical distance between the cube's corner rows
    const ImVec2 top(center.x, center.y - 2.0f * step);
    const ImVec2 left(center.x - halfWidth, center.y - step);
    const ImVec2 right(center.x + halfWidth, center.y - step);
    const ImVec2 middle(center.x, center.y);
    const ImVec2 bottomLeft(center.x - halfWidth, center.y + step);
    const ImVec2 bottomRight(center.x + halfWidth, center.y + step);
    const ImVec2 bottom(center.x, center.y + 2.0f * step);
    draw->AddQuadFilled(top, right, middle, left, shade(1.0f));
    draw->AddQuadFilled(left, middle, bottom, bottomLeft, shade(0.72f));
    draw->AddQuadFilled(middle, right, bottomRight, bottom, shade(0.52f));
}

std::string withCommas(uint64_t value) {
    const std::string digits = std::to_string(value);
    std::string out;
    for (size_t i = 0; i < digits.size(); ++i) {
        bool startsGroupOfThree = i > 0 && (digits.size() - i) % 3 == 0;
        if (startsGroupOfThree) out += ',';
        out += digits[i];
    }
    return out;
}

} // namespace gol3d::ui

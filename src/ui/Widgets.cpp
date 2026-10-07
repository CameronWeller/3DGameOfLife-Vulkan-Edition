// The menu building blocks declared in Widgets.h: cards, headers, buttons,
// stat chips and small draw-list icons. Every length goes through px() so the
// menus scale with the GUI scale.

#include "ui/Widgets.h"

#include <algorithm>
#include <cmath>

#include "ui/Theme.h"

namespace gol3d::ui {
namespace {

// Set once per frame by setLayout(); Dear ImGui itself is a global, so these are too.
float layoutScale = 1.0f;
ImFont* layoutTitleFont = nullptr;

// Card backdrop: the world is dimmed everywhere, and darker still in a band
// along the top and bottom edges that fades toward the middle.
constexpr float BACKDROP_EDGE_BAND = 0.35f; // of the screen height, per edge
constexpr int BACKDROP_DIM_ALPHA = 120;
constexpr int BACKDROP_EDGE_ALPHA = 200;
constexpr float CARD_SCREEN_MARGIN = 24.0f; // a tall card stops this short of the screen height

// A menu item's label and key hint sit this far in from the button's edges.
constexpr float MENU_ITEM_INSET = 12.0f;
// Buttons are this much taller than Dear ImGui's standard frame height.
constexpr float BUTTON_EXTRA_HEIGHT = 6.0f;
// Hover hints wrap at this many font heights.
constexpr float HINT_WIDTH_EMS = 18.0f;

// The 5x5 icons of drawIcon5x5().
constexpr int ICON_CELLS = 5;           // per side
constexpr float ICON_CELL_SIZE = 0.14f; // of the icon square; 5 cells leave a small margin

ImVec4 toVec4(ImU32 packed) {
    return ImGui::ColorConvertU32ToFloat4(packed);
}

// The fullscreen dimming behind a card, drawn in its own invisible window so
// it sits under the card.
void drawBackdrop(const char* cardId) {
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->Pos);
    ImGui::SetNextWindowSize(viewport->Size);
    ImGui::SetNextWindowBgAlpha(0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::Begin((std::string(cardId) + "-backdrop").c_str(), nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus |
                     ImGuiWindowFlags_NoNav);
    ImDrawList* draw = ImGui::GetWindowDrawList();
    const ImVec2 low = viewport->Pos;
    const ImVec2 high(viewport->Pos.x + viewport->Size.x, viewport->Pos.y + viewport->Size.y);
    const float band = viewport->Size.y * BACKDROP_EDGE_BAND;
    const ImU32 dim = color(palette::BACKDROP, BACKDROP_DIM_ALPHA);
    const ImU32 dark = color(palette::BACKDROP, BACKDROP_EDGE_ALPHA);
    const ImU32 clear = color(palette::BACKDROP, 0);
    draw->AddRectFilled(low, high, dim);
    // AddRectFilledMultiColor takes corner colors clockwise from the top left:
    // the top band is dark at its top edge, the bottom band at its bottom edge.
    draw->AddRectFilledMultiColor(low, ImVec2(high.x, low.y + band), dark, dark, clear, clear);
    draw->AddRectFilledMultiColor(ImVec2(low.x, high.y - band), high, clear, clear, dark, dark);
    ImGui::End();
    ImGui::PopStyleVar(2);
}

// The 3D Life mark: Conway's glider, three cells tall, vertically centered on a
// title line of the given height. Ends with the cursor on the same line, after it.
void drawGliderMark(float titleHeight) {
    // The glider's live cells as {x, y}, y growing downward.
    static constexpr int GLIDER[5][2] = {{1, 0}, {2, 1}, {0, 2}, {1, 2}, {2, 2}};
    // Three cells span about 88% of the title height.
    const float cell = std::floor(titleHeight / 3.4f);
    const ImVec2 at = ImGui::GetCursorScreenPos();
    const float top = at.y + std::floor((titleHeight - 3.0f * cell) * 0.5f);
    ImDrawList* draw = ImGui::GetWindowDrawList();
    for (const auto& [x, y] : GLIDER) {
        const ImVec2 cellMin(at.x + x * cell, top + y * cell);
        // Inset each cell by a pixel so neighbors read as separate cells.
        draw->AddRectFilled(ImVec2(cellMin.x + px(1), cellMin.y + px(1)),
                            ImVec2(cellMin.x + cell - px(1), cellMin.y + cell - px(1)),
                            accentColor(), px(1.5f));
    }
    ImGui::Dummy(ImVec2(3.0f * cell + px(6), titleHeight));
    ImGui::SameLine();
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
    drawBackdrop(id);

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    // Fixed width; the height follows the content, up to nearly the screen height.
    ImGui::SetNextWindowSizeConstraints(
        ImVec2(px(width), 0.0f), ImVec2(px(width), viewport->Size.y - px(CARD_SCREEN_MARGIN)));
    return ImGui::Begin(id, nullptr,
                        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                            ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize);
}

void cardHeader(const char* title, const char* subtitle) {
    // Measure the title line at the title font's size.
    ImGui::PushFont(layoutTitleFont, TITLE_FONT_SIZE);
    const float height = ImGui::GetFontSize();
    ImGui::PopFont();

    drawGliderMark(height);

    ImGui::PushFont(layoutTitleFont, TITLE_FONT_SIZE);
    ImGui::TextUnformatted(title);
    ImGui::PopFont();
    if (subtitle) {
        // Right-aligned on the title's line, vertically centered on it.
        const ImVec2 size = ImGui::CalcTextSize(subtitle);
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
    ImGui::PushTextWrapPos(0.0f); // wrap at the window's right edge
    ImGui::TextUnformatted(text.c_str());
    ImGui::PopTextWrapPos();
    ImGui::PopStyleColor();
}

void mutedText(const std::string& text) {
    note(text, color(palette::MUTED_TEXT));
}

void rightAlignNext(float width) {
    ImGui::SameLine();
    const float slack = std::max(0.0f, ImGui::GetContentRegionAvail().x - width);
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + slack);
}

void hoverHint(const char* text) {
    if (!ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) return;
    ImGui::SetNextWindowSize(ImVec2(ImGui::GetFontSize() * HINT_WIDTH_EMS, 0));
    ImGui::BeginTooltip();
    ImGui::TextWrapped("%s", text);
    ImGui::EndTooltip();
}

float buttonHeight() {
    return ImGui::GetFrameHeight() + px(BUTTON_EXTRA_HEIGHT);
}

void pushAccentButton() {
    ImGui::PushStyleColor(ImGuiCol_Button, toVec4(accentColor()));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, toVec4(color(palette::ACCENT_HOVERED)));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, toVec4(color(palette::ACCENT_PRESSED)));
    ImGui::PushStyleColor(ImGuiCol_Text, toVec4(color(palette::ON_ACCENT)));
}

void popAccentButton() {
    ImGui::PopStyleColor(4); // the four colors pushAccentButton() pushed
}

bool menuItem(const char* label, const char* keyHint, ButtonKind kind) {
    const bool primary = kind == ButtonKind::Primary;
    const bool danger = kind == ButtonKind::Danger;
    if (primary) pushAccentButton();
    if (danger) {
        // Danger buttons look normal until hovered, then turn red.
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, toVec4(color(palette::DANGER_HOVERED)));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, toVec4(color(palette::DANGER_PRESSED)));
    }
    const float labelAlignX = primary ? 0.5f : 0.0f; // centered or left-aligned
    ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(labelAlignX, 0.5f));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,
                        ImVec2(px(MENU_ITEM_INSET), ImGui::GetStyle().FramePadding.y));
    const ImVec2 at = ImGui::GetCursorScreenPos();
    const float width = ImGui::GetContentRegionAvail().x;
    const bool clicked = ImGui::Button(label, ImVec2(width, buttonHeight()));
    ImGui::PopStyleVar(2);
    if (primary) popAccentButton();
    if (danger) ImGui::PopStyleColor(2);

    // The key hint is drawn over the button, right-aligned, in a quieter color.
    if (keyHint) {
        const ImVec2 size = ImGui::CalcTextSize(keyHint);
        const ImU32 hintColor =
            primary ? color(palette::ON_ACCENT, 170) : ImGui::GetColorU32(ImGuiCol_TextDisabled);
        const ImVec2 position(at.x + width - size.x - px(MENU_ITEM_INSET),
                              at.y + (buttonHeight() - size.y) * 0.5f);
        ImGui::GetWindowDrawList()->AddText(position, hintColor, keyHint);
    }
    return clicked;
}

PairChoice buttonPair(const char* left, const char* right, ButtonKind rightKind) {
    const float buttonWidth =
        (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) * 0.5f;
    PairChoice choice = PairChoice::None;
    if (ImGui::Button(left, ImVec2(buttonWidth, buttonHeight()))) choice = PairChoice::Left;
    ImGui::SameLine();
    const bool primary = rightKind == ButtonKind::Primary;
    if (primary) pushAccentButton();
    if (ImGui::Button(right, ImVec2(buttonWidth, buttonHeight()))) choice = PairChoice::Right;
    if (primary) popAccentButton();
    return choice;
}

void statChips(std::initializer_list<std::pair<const char*, std::string>> chips) {
    // Each chip: a small muted label above its value, in a faint rounded box.
    constexpr float LABEL_SCALE = 0.72f; // label size, relative to the body font
    constexpr float GAP = 6.0f;          // between chips
    constexpr float CORNER_RADIUS = 6.0f;
    constexpr float PADDING_X = 8.0f;
    constexpr float LABEL_TOP = 5.0f;
    constexpr float VALUE_TOP = 7.0f;         // plus the label's height
    constexpr float VERTICAL_PADDING = 14.0f; // in total, around the label and the value
    ImDrawList* draw = ImGui::GetWindowDrawList();
    const float gap = px(GAP);
    const float count = static_cast<float>(chips.size());
    const float width = (ImGui::GetContentRegionAvail().x - gap * (count - 1.0f)) / count;
    const float labelSize = ImGui::GetFontSize() * LABEL_SCALE;
    const float height = labelSize + ImGui::GetFontSize() + px(VERTICAL_PADDING);
    ImVec2 at = ImGui::GetCursorScreenPos();
    for (const auto& [name, value] : chips) {
        const ImVec2 corner(at.x + width, at.y + height);
        draw->AddRectFilled(at, corner, color(255, 255, 255, 10), px(CORNER_RADIUS));
        draw->AddRect(at, corner, color(palette::BORDER, 50), px(CORNER_RADIUS), px(1));
        draw->AddText(ImGui::GetFont(), labelSize,
                      ImVec2(at.x + px(PADDING_X), at.y + px(LABEL_TOP)),
                      color(palette::MUTED_TEXT), name);
        draw->AddText(ImVec2(at.x + px(PADDING_X), at.y + px(VALUE_TOP) + labelSize),
                      IM_COL32_WHITE, value.c_str());
        at.x += width + gap;
    }
    // The chips were drawn directly; reserve their row in the layout.
    ImGui::Dummy(ImVec2(0, height));
}

void shadowText(ImDrawList* draw, ImVec2 position, const std::string& text, ImU32 textColor) {
    // The shadow is black at 60% of the text's own opacity, so fading text fades its shadow.
    const int alpha = static_cast<int>((textColor >> IM_COL32_A_SHIFT) & 0xFF);
    const ImU32 shadow = color(0, 0, 0, alpha * 3 / 5);
    draw->AddText(ImVec2(position.x + px(1), position.y + px(1)), shadow, text.c_str());
    draw->AddText(position, textColor, text.c_str());
}

void drawIcon5x5(ImDrawList* draw, ImVec2 min, float size, uint32_t bits, ImU32 iconColor) {
    const float cell = std::floor(size * ICON_CELL_SIZE);
    const float inset = std::floor((size - ICON_CELLS * cell) * 0.5f);
    const ImVec2 origin(min.x + inset, min.y + inset);
    constexpr int TOP_LEFT_BIT = ICON_CELLS * ICON_CELLS - 1;
    for (int y = 0; y < ICON_CELLS; ++y) {
        for (int x = 0; x < ICON_CELLS; ++x) {
            // Row-major from the most significant of the 25 bits.
            const int bit = TOP_LEFT_BIT - (y * ICON_CELLS + x);
            const bool lit = ((bits >> bit) & 1u) != 0;
            if (!lit) continue;
            draw->AddRectFilled(ImVec2(origin.x + x * cell, origin.y + y * cell),
                                ImVec2(origin.x + (x + 1) * cell, origin.y + (y + 1) * cell),
                                iconColor);
        }
    }
}

void drawCubeIcon(ImDrawList* draw, ImVec2 center, float size, const uint8_t rgb[3]) {
    // Fixed face shading, as if lit from above and a little to the left.
    constexpr float TOP_BRIGHTNESS = 1.0f;
    constexpr float LEFT_BRIGHTNESS = 0.72f;
    constexpr float RIGHT_BRIGHTNESS = 0.52f;
    auto shade = [&](float brightness) {
        return color(static_cast<int>(rgb[0] * brightness), static_cast<int>(rgb[1] * brightness),
                     static_cast<int>(rgb[2] * brightness));
    };
    // The cube's outline is a hexagon: three columns of corners (left, middle,
    // right), with the middle column one step higher and lower than the sides.
    const float halfWidth = size * 0.5f;
    const float step = size * 0.29f; // vertical distance between the cube's corner rows
    const ImVec2 top(center.x, center.y - 2.0f * step);
    const ImVec2 left(center.x - halfWidth, center.y - step);
    const ImVec2 right(center.x + halfWidth, center.y - step);
    const ImVec2 middle(center.x, center.y);
    const ImVec2 bottomLeft(center.x - halfWidth, center.y + step);
    const ImVec2 bottomRight(center.x + halfWidth, center.y + step);
    const ImVec2 bottom(center.x, center.y + 2.0f * step);
    draw->AddQuadFilled(top, right, middle, left, shade(TOP_BRIGHTNESS));
    draw->AddQuadFilled(left, middle, bottom, bottomLeft, shade(LEFT_BRIGHTNESS));
    draw->AddQuadFilled(middle, right, bottomRight, bottom, shade(RIGHT_BRIGHTNESS));
}

std::string withCommas(uint64_t value) {
    const std::string digits = std::to_string(value);
    std::string grouped;
    for (size_t i = 0; i < digits.size(); ++i) {
        const size_t digitsLeft = digits.size() - i;
        const bool startsGroupOfThree = i > 0 && digitsLeft % 3 == 0;
        if (startsGroupOfThree) grouped += ',';
        grouped += digits[i];
    }
    return grouped;
}

} // namespace gol3d::ui

#pragma once

// Building blocks for the menus, on top of Dear ImGui. Dear ImGui is an
// "immediate mode" UI: there are no widget objects; each frame the code simply
// calls a function per widget, and a function returns true when its widget was
// clicked. These helpers follow the same style.
//
// Sizes are given at GUI scale 1 and multiplied by the current scale (px()), so
// call setLayout() once per frame before drawing.

#include <cstdint>
#include <initializer_list>
#include <string>
#include <utility>

#include "imgui.h"

namespace gol3d::ui {

// The GUI scale and the font for card titles, for the widgets below.
void setLayout(float scale, ImFont* titleFont);
// The GUI scale given to setLayout().
float scale();
// A length at the current GUI scale.
float px(float value);

// Dims the world, then opens a card of the given width centered on screen.
// Always pair with ImGui::End(), whether or not it returns true.
bool beginCard(const char* id, float width);
// The 3D Life mark (Conway's glider in the accent color), a title, and an
// optional muted subtitle on the right, followed by a separator.
void cardHeader(const char* title, const char* subtitle = nullptr);
// A small accent-colored heading inside a card.
void sectionLabel(const char* text);
// Wrapped text in a color.
void note(const std::string& text, ImU32 textColor);
void mutedText(const std::string& text);
// Places the next item on this line, flush with the right edge of the window.
void rightAlignNext(float width);
// A tooltip for the widget just drawn, after a short hover.
void hoverHint(const char* text);

enum class ButtonKind { Normal, Primary, Danger };
// The height of menu buttons, a little taller than Dear ImGui's default.
float buttonHeight();
// Accent-colored button style; pop with popAccentButton().
void pushAccentButton();
void popAccentButton();
// A full-width button with its label on the left and an optional key hint on
// the right. Primary buttons are accent-filled with a centered label.
bool menuItem(const char* label, const char* keyHint = nullptr,
              ButtonKind kind = ButtonKind::Normal);
// Two buttons sharing a row.
enum class PairChoice { None, Left, Right };
PairChoice buttonPair(const char* left, const char* right,
                      ButtonKind rightKind = ButtonKind::Normal);
// A row of small labeled values ("GENERATION 1,204") in rounded boxes.
void statChips(std::initializer_list<std::pair<const char*, std::string>> chips);

// Drawing helpers for draw lists.

// Text with a soft shadow, so it reads over bright sky and blocks.
void shadowText(ImDrawList* draw, ImVec2 position, const std::string& text,
                ImU32 textColor = IM_COL32(255, 255, 255, 255));
// A 5x5 pixel icon (bit 24 = top-left, row by row) centered in a square.
void drawIcon5x5(ImDrawList* draw, ImVec2 min, float size, uint32_t bits, ImU32 iconColor);
// An isometric cube: lit top, mid left face, dark right face. `rgb` is sRGB.
void drawCubeIcon(ImDrawList* draw, ImVec2 center, float size, const uint8_t rgb[3]);

// 1234567 -> "1,234,567".
std::string withCommas(uint64_t value);

} // namespace gol3d::ui

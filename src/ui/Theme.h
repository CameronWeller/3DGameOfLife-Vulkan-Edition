#pragma once

// 3D Life's own menu look: dark translucent cards with rounded corners, a mint
// accent (the color of live cells), and the Karla font.
//
// Colors are written in sRGB, as a designer would pick them. The swapchain is
// sRGB, so the GPU applies the gamma curve when it writes pixels; every color is
// converted to linear before it reaches Dear ImGui.

#include <cstdint>

#include "imgui.h"

#include "life/CellTypes.h"

namespace gol3d::ui {

// An sRGB color, 0-255 per channel.
struct Rgb {
    int r, g, b;
};

namespace palette {
constexpr Rgb ACCENT{94, 230, 168}; // mint: live cells, primary buttons, highlights
constexpr Rgb ACCENT_HOVERED{130, 240, 192};
constexpr Rgb ACCENT_PRESSED{70, 200, 140};
constexpr Rgb ON_ACCENT{10, 30, 22};       // text on accent-filled buttons
constexpr Rgb PANEL{14, 18, 28};           // card and HUD backgrounds
constexpr Rgb BACKDROP{8, 10, 18};         // dims the world behind menus
constexpr Rgb BORDER{120, 140, 180};       // thin outlines (used translucent)
constexpr Rgb MUTED_TEXT{138, 149, 170};   // secondary text in menus
constexpr Rgb HUD_TEXT{170, 182, 200};     // secondary text over the world
constexpr Rgb HUD_DETAIL{200, 210, 225};   // F3 debug lines
constexpr Rgb TRACK{40, 49, 70};           // empty part of progress bars
constexpr Rgb SLOWED{255, 196, 90};        // amber: the governor slowed the simulation
constexpr Rgb NOTICE{255, 236, 150};       // update messages
constexpr Rgb PROBLEM{255, 160, 150};      // error messages
constexpr Rgb DANGER_HOVERED{150, 56, 64}; // Quit button
constexpr Rgb DANGER_PRESSED{180, 66, 74};
} // namespace palette

constexpr float BODY_FONT_SIZE = 15.0f;
constexpr float TITLE_FONT_SIZE = 22.0f;

// sRGB channel (0-1) to linear, with the common 2.2 gamma approximation.
float srgbToLinear(float channel);

// A packed linear color for ImGui draw lists.
ImU32 color(int r, int g, int b, int alpha = 255);
ImU32 color(Rgb rgb, int alpha = 255);
ImU32 accentColor(int alpha = 255);
// An ImVec4 for style colors, from sRGB channels in 0-1.
ImVec4 linearFromSrgb(float r, float g, float b, float alpha = 1.0f);
// A material's swatch color (CellType::rgb).
ImU32 materialColor(CellKind kind, int alpha = 255);

// Sets Dear ImGui's global style to the 3D Life look.
void applyMenuStyle();

} // namespace gol3d::ui

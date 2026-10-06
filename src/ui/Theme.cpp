#include "ui/Theme.h"

#include <cmath>

namespace gol3d::ui {

float srgbToLinear(float channel) {
    return std::pow(channel, 2.2f);
}

ImU32 color(int r, int g, int b, int alpha) {
    auto toLinear255 = [](int value) {
        return static_cast<int>(
            std::lround(255.0f * srgbToLinear(static_cast<float>(value) / 255.0f)));
    };
    return IM_COL32(toLinear255(r), toLinear255(g), toLinear255(b), alpha);
}

ImU32 color(Rgb rgb, int alpha) {
    return color(rgb.r, rgb.g, rgb.b, alpha);
}

ImU32 accentColor(int alpha) {
    return color(palette::ACCENT, alpha);
}

ImVec4 linearFromSrgb(float r, float g, float b, float alpha) {
    return ImVec4(srgbToLinear(r), srgbToLinear(g), srgbToLinear(b), alpha);
}

ImU32 materialColor(CellKind kind, int alpha) {
    const uint8_t* rgb = cellType(kind).rgb;
    return color(rgb[0], rgb[1], rgb[2], alpha);
}

void applyMenuStyle() {
    ImGuiStyle& style = ImGui::GetStyle();
    ImGui::StyleColorsDark(&style);
    style.WindowRounding = 12.0f;
    style.ChildRounding = 8.0f;
    style.PopupRounding = 8.0f;
    style.FrameRounding = 6.0f;
    style.GrabRounding = 6.0f;
    style.ScrollbarRounding = 6.0f;
    style.TabRounding = 6.0f;
    style.WindowBorderSize = 1.0f;
    style.ChildBorderSize = 1.0f;
    style.PopupBorderSize = 1.0f;
    style.FrameBorderSize = 0.0f;
    style.WindowPadding = ImVec2(18, 16);
    style.FramePadding = ImVec2(10, 5);
    style.ItemSpacing = ImVec2(8, 6);
    style.ItemInnerSpacing = ImVec2(6, 4);
    style.GrabMinSize = 10.0f;
    style.ScrollbarSize = 10.0f;

    // Style colors are given in sRGB here and converted to linear below.
    auto srgb = [](Rgb c, float alpha = 1.0f) {
        return ImVec4(c.r / 255.0f, c.g / 255.0f, c.b / 255.0f, alpha);
    };
    using namespace palette;
    ImVec4* colors = style.Colors;
    colors[ImGuiCol_Text] = srgb({232, 237, 245});
    colors[ImGuiCol_TextDisabled] = srgb(MUTED_TEXT);
    colors[ImGuiCol_WindowBg] = srgb(PANEL, 0.94f);
    colors[ImGuiCol_ChildBg] = srgb({255, 255, 255}, 0.03f);
    colors[ImGuiCol_PopupBg] = srgb({20, 26, 40}, 0.98f);
    colors[ImGuiCol_Border] = srgb(BORDER, 0.22f);
    colors[ImGuiCol_BorderShadow] = srgb({0, 0, 0}, 0.0f);
    colors[ImGuiCol_Button] = srgb({36, 44, 62});
    colors[ImGuiCol_ButtonHovered] = srgb({50, 62, 88});
    colors[ImGuiCol_ButtonActive] = srgb({62, 78, 110});
    colors[ImGuiCol_FrameBg] = srgb(TRACK);
    colors[ImGuiCol_FrameBgHovered] = srgb({52, 63, 90});
    colors[ImGuiCol_FrameBgActive] = srgb({62, 76, 108});
    colors[ImGuiCol_SliderGrab] = srgb(ACCENT);
    colors[ImGuiCol_SliderGrabActive] = srgb({150, 245, 200});
    colors[ImGuiCol_CheckMark] = srgb(ACCENT);
    colors[ImGuiCol_Header] = srgb(ACCENT, 0.18f);
    colors[ImGuiCol_HeaderHovered] = srgb(ACCENT, 0.28f);
    colors[ImGuiCol_HeaderActive] = srgb(ACCENT, 0.40f);
    colors[ImGuiCol_Separator] = srgb(BORDER, 0.22f);
    colors[ImGuiCol_ScrollbarBg] = srgb({0, 0, 0}, 0.0f);
    colors[ImGuiCol_ScrollbarGrab] = srgb({60, 72, 100});
    colors[ImGuiCol_ScrollbarGrabHovered] = srgb({76, 90, 124});
    colors[ImGuiCol_ScrollbarGrabActive] = srgb({90, 106, 144});
    colors[ImGuiCol_TextSelectedBg] = srgb(ACCENT, 0.35f);
    colors[ImGuiCol_NavCursor] = srgb(ACCENT);
    for (int i = 0; i < ImGuiCol_COUNT; ++i) {
        colors[i] = ImVec4(srgbToLinear(colors[i].x), srgbToLinear(colors[i].y),
                           srgbToLinear(colors[i].z), colors[i].w);
    }
}

} // namespace gol3d::ui

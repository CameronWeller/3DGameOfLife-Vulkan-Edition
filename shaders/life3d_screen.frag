#version 450
#extension GL_GOOGLE_include_directive : require

// Fullscreen passes (life3d_screen.vert), chosen by draw.mode:
//   0 sky: the gradient behind everything
//   1 grid: block lines on the y = 0 plane with chunk lines every 32 blocks
//     (depth-tested so blocks hide it; writes the plane's depth)
//   2 HUD: the crosshair and the hotbar

#include "life3d_frame.glsl"

const uint MODE_SKY = 0u;
const uint MODE_GRID = 1u;
const uint MODE_HUD = 2u;

const float CHUNK_BLOCKS = 32.0; // chunk edge, for the chunk grid lines

layout(location = 0) in vec2 ndc;
layout(location = 0) out vec4 outColor;

// The direction from the camera through this pixel: unproject the pixel's
// point on the far plane (depth 1) back to world space.
vec3 viewRay() {
    vec4 far = frame.inverseViewProjection * vec4(ndc, 1.0, 1.0);
    return normalize(far.xyz / far.w - frame.camera.xyz);
}

// 1 on a grid line `spacing` apart, fading to 0 within about a pixel (fwidth
// is how much the grid position changes from one pixel to the next), so lines
// stay one pixel wide and smooth at any distance.
float gridLine(vec2 p, float spacing) {
    vec2 gridPosition = p / spacing; // lines at whole numbers
    vec2 pixelWidth = fwidth(gridPosition);
    // abs(fract(x - 0.5) - 0.5) is the distance from x to the nearest whole number.
    vec2 pixelsToLine = abs(fract(gridPosition - 0.5) - 0.5) / max(pixelWidth, vec2(1e-5));
    return 1.0 - min(min(pixelsToLine.x, pixelsToLine.y), 1.0);
}

// Signed distance from p (relative to the box center) to a rounded box:
// negative inside, positive outside. The rounded-box distance function from
// Inigo Quilez's "2D distance functions" (iquilezles.org).
float roundedBox(vec2 p, vec2 halfSize, float radius) {
    vec2 q = abs(p) - halfSize + radius;
    return length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - radius;
}

// HUD metrics in pixels at scale 1. The scale grows by whole steps with the
// window height. GameHud.cpp (drawSelectedStampName) declares the same
// HUD_SCALE_HEIGHT and HOTBAR_SLOT to place the stamp name above the hotbar.
const float HUD_SCALE_HEIGHT = 540.0;
const float HOTBAR_SLOT = 40.0;
const float HOTBAR_GAP = 4.0;
const float HOTBAR_BOTTOM_MARGIN = 10.0;
const float HOTBAR_CORNER_RADIUS = 7.0;

// 5x5 hotbar icons, one bit per pixel, row by row from the top-left pixel at
// bit 24. Must match StampInfo::icon in src/game/Stamps.h.
const int ICON_SIZE = 5;              // icon squares per side
const float ICON_SQUARE_SHARE = 0.14; // an icon square's size, as a share of the slot
const int ICON_COUNT = 9;
const uint ICONS[ICON_COUNT] = uint[ICON_COUNT](
    0x0001000u,  // 1 single cell
    0x00739C0u,  // 2 2x2x2 block
    0x0023880u,  // 3 plus
    0x0051120u,  // 4 small soup
    0x165E9B6u,  // 5 big soup
    0x1FFFFFFu,  // 6 wall
    0x0421084u,  // 7 pillar
    0x1555555u,  // 8 rule seed
    0x00209C0u   // 9 glider
);

// Material colors in linear RGB (the swapchain is sRGB); must match the rgb
// swatches in src/life/CellTypes.h.
const vec3 MATERIAL[3] = vec3[3](vec3(0.112, 0.791, 0.392),  // Life: sRGB (94, 230, 168)
                                 vec3(0.305, 0.337, 0.402),  // Stone: sRGB (150, 156, 170)
                                 vec3(1.0, 0.305, 0.045));   // Ember: sRGB (255, 150, 60)
const vec3 PANEL = vec3(0.0044, 0.006, 0.011); // sRGB (14, 18, 28), the menu panel color

// A white cross with a dark outline at the screen center, or transparent.
vec4 crosshair(vec2 pixel, vec2 screen, float scale) {
    // gl_FragCoord is a pixel's center (n + 0.5), so this is 0 for the center pixel.
    vec2 fromCenter = abs(pixel - floor(screen * 0.5) - 0.5);
    float arm = 9.0 * scale;
    float thickness = 1.0 * scale;
    bool inCross = (fromCenter.x <= thickness && fromCenter.y <= arm) ||
                   (fromCenter.y <= thickness && fromCenter.x <= arm);
    bool inOutline = (fromCenter.x <= thickness + scale && fromCenter.y <= arm + scale) ||
                     (fromCenter.y <= thickness + scale && fromCenter.x <= arm + scale);
    if (inCross) return vec4(1.0, 1.0, 1.0, 0.95);
    if (inOutline) return vec4(0.0, 0.0, 0.0, 0.6);
    return vec4(0.0);
}

// The hotbar centered at the bottom: rounded dark slots in the menu style, the
// selected one bordered and tinted in the building material's color.
vec4 hotbar(vec2 pixel, vec2 screen, float scale) {
    int slots = frame.hotbar.y;
    float slot = HOTBAR_SLOT * scale;
    float gap = HOTBAR_GAP * scale;
    float width = float(slots) * slot + float(slots - 1) * gap;
    vec2 origin = vec2(floor(0.5 * (screen.x - width)), screen.y - slot - HOTBAR_BOTTOM_MARGIN * scale);
    vec2 relative = pixel - origin;
    if (relative.x < 0.0 || relative.y < 0.0 || relative.x >= width || relative.y >= slot) return vec4(0.0);
    // Slots repeat every slot + gap pixels: which one, and where inside it.
    int index = int(relative.x / (slot + gap));
    vec2 inSlot = vec2(relative.x - float(index) * (slot + gap), relative.y);
    if (index >= slots || index >= ICON_COUNT || inSlot.x >= slot) return vec4(0.0); // in a gap

    vec3 accent = MATERIAL[clamp(frame.hotbar.z, 0, 2)];
    bool selected = index == frame.hotbar.x;
    // The slot shape: `edge` is the signed distance to its outline, from the
    // pixel center (+ 0.5) relative to the slot center (0.5 * slot).
    float edge = roundedBox(inSlot + 0.5 - 0.5 * slot, vec2(0.5 * slot), HOTBAR_CORNER_RADIUS * scale);
    float coverage = clamp(0.5 - edge, 0.0, 1.0); // antialiased corners
    if (coverage <= 0.0) return vec4(0.0);
    float border = (selected ? 2.0 : 1.0) * scale;
    if (edge > -border) {
        return selected ? vec4(accent, coverage) : vec4(0.19, 0.26, 0.46, 0.55 * coverage);
    }

    // The icon: ICON_SIZE x ICON_SIZE squares of squareSize pixels in the middle
    // of the slot. Bits run in reading order from bit 24 at the top-left
    // square down to bit 0 at the bottom-right (in Vulkan, pixel y grows
    // downward, like the icon rows).
    float squareSize = floor(slot * ICON_SQUARE_SHARE);
    vec2 inIcon = inSlot - floor(0.5 * (slot - float(ICON_SIZE) * squareSize));
    bool insideIcon =
        all(greaterThanEqual(inIcon, vec2(0.0))) && all(lessThan(inIcon, vec2(float(ICON_SIZE) * squareSize)));
    if (insideIcon) {
        ivec2 iconSquare = ivec2(inIcon / squareSize);
        uint bit = uint(ICON_SIZE * ICON_SIZE - 1 - (iconSquare.y * ICON_SIZE + iconSquare.x));
        if (((ICONS[index] >> bit) & 1u) != 0u) return vec4(accent * (selected ? 1.0 : 0.6), 1.0);
    }
    return selected ? vec4(PANEL + accent * 0.06, 0.8) : vec4(PANEL, 0.6);
}

// The HUD color of this pixel: the crosshair over the hotbar.
vec4 hud() {
    vec2 pixel = gl_FragCoord.xy;
    vec2 screen = frame.viewport.xy;
    float scale = max(1.0, floor(screen.y / HUD_SCALE_HEIGHT));
    vec4 crosshairColor = crosshair(pixel, screen, scale);
    if (crosshairColor.a > 0.0) return crosshairColor;
    return hotbar(pixel, screen, scale);
}

// The ground grid where the view ray meets y = 0, fading out with distance.
void grid() {
    vec3 dir = viewRay();
    if (abs(dir.y) < 1e-5) discard; // looking parallel to the ground
    // camera + dir * rayDistance has y = 0. dir is unit length, so this is
    // also the distance in blocks.
    float rayDistance = -frame.camera.y / dir.y;
    if (rayDistance <= 0.0) discard; // the plane is behind the camera
    vec3 ground = frame.camera.xyz + dir * rayDistance;
    float blocks = gridLine(ground.xz, 1.0) * 0.35;
    float chunks = gridLine(ground.xz, CHUNK_BLOCKS);
    float fade = 1.0 - smoothstep(frame.fog.x * 0.5, frame.fog.y, rayDistance);
    float alpha = max(blocks, chunks * 0.8) * fade;
    if (alpha < 0.01) discard;
    // The triangle itself lies on no plane; write the depth of the ground
    // point so blocks in front of the plane hide it.
    vec4 clip = frame.viewProjection * vec4(ground, 1.0);
    gl_FragDepth = clip.z / clip.w;
    outColor = vec4(mix(vec3(0.9), vec3(1.0, 1.0, 0.8), chunks), alpha);
}

void main() {
    if (draw.mode == MODE_SKY) {
        outColor = vec4(skyColor(viewRay()), 1.0);
        return;
    }
    if (draw.mode == MODE_GRID) {
        grid();
        return;
    }
    bool hudVisible = frame.viewport.z >= 0.5;
    if (!hudVisible) discard;
    outColor = hud();
    if (outColor.a <= 0.0) discard;
}

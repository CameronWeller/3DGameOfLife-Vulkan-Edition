#version 450
#extension GL_GOOGLE_include_directive : require

// Screen-space passes for the prototype.
//   mode 0: sky gradient behind everything
//   mode 1: block grid on the y = 0 plane with chunk lines every 16 blocks
//           (depth-tested so blocks hide it; writes the plane's depth)
//   mode 2: HUD - crosshair and hotbar

#include "life3d_frame.glsl"

layout(location = 0) in vec2 ndc;
layout(location = 0) out vec4 outColor;

vec3 viewRay() {
    vec4 far = frame.invViewProj * vec4(ndc, 1.0, 1.0);
    return normalize(far.xyz / far.w - frame.camera.xyz);
}

float gridLine(vec2 p, float spacing) {
    vec2 q = p / spacing;
    vec2 width = fwidth(q);
    vec2 dist = abs(fract(q - 0.5) - 0.5) / max(width, vec2(1e-5));
    return 1.0 - min(min(dist.x, dist.y), 1.0);
}

// Signed distance from p (relative to the box center) to a rounded box.
float roundedBox(vec2 p, vec2 halfSize, float radius) {
    vec2 q = abs(p) - halfSize + radius;
    return length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - radius;
}

// 5x5 hotbar icons, one bit per pixel, row-major from the top-left.
const uint ICONS[8] = uint[8](
    0x0001000u,  // 1 single cell
    0x00739C0u,  // 2 2x2x2 block
    0x0023880u,  // 3 plus
    0x0051120u,  // 4 small soup
    0x165E9B6u,  // 5 big soup
    0x1FFFFFFu,  // 6 wall
    0x0421084u,  // 7 pillar
    0x1555555u  // 8 rule seed
);

vec4 hud() {
    vec2 pixel = gl_FragCoord.xy;
    vec2 size = frame.viewport.xy;
    float scale = max(1.0, floor(size.y / 540.0));

    // Crosshair: white with a dark outline.
    vec2 d = abs(pixel - floor(size * 0.5) - 0.5);
    float arm = 9.0 * scale, thick = 1.0 * scale;
    bool cross = (d.x <= thick && d.y <= arm) || (d.y <= thick && d.x <= arm);
    bool outline = (d.x <= thick + scale && d.y <= arm + scale) || (d.y <= thick + scale && d.x <= arm + scale);
    if (cross) return vec4(1.0, 1.0, 1.0, 0.95);
    if (outline) return vec4(0.0, 0.0, 0.0, 0.6);

    // Hotbar centered at the bottom.
    int slots = frame.hotbar.y;
    float slot = 40.0 * scale, gap = 4.0 * scale;
    float width = float(slots) * slot + float(slots - 1) * gap;
    vec2 origin = vec2(floor(0.5 * (size.x - width)), size.y - slot - 10.0 * scale);
    vec2 rel = pixel - origin;
    if (rel.x < 0.0 || rel.y < 0.0 || rel.x >= width || rel.y >= slot) return vec4(0.0);
    int index = int(rel.x / (slot + gap));
    vec2 inSlot = vec2(rel.x - float(index) * (slot + gap), rel.y);
    if (index >= slots || index >= 8 || inSlot.x >= slot) return vec4(0.0);

    // Rounded dark slots in the menu style: the selected one gets a mint border
    // and tint. Colors are linear (the swapchain is sRGB).
    const vec3 accent = vec3(0.112, 0.791, 0.392); // sRGB (94, 230, 168)
    const vec3 panel = vec3(0.0044, 0.006, 0.011); // sRGB (14, 18, 28)
    bool selected = index == frame.hotbar.x;
    float edge = roundedBox(inSlot + 0.5 - 0.5 * slot, vec2(0.5 * slot), 7.0 * scale);
    float coverage = clamp(0.5 - edge, 0.0, 1.0); // antialiased corners
    if (coverage <= 0.0) return vec4(0.0);
    float border = (selected ? 2.0 : 1.0) * scale;
    if (edge > -border) {
        return selected ? vec4(accent, coverage) : vec4(0.19, 0.26, 0.46, 0.55 * coverage);
    }
    // Icon: 5x5 cells in the middle of the slot.
    float cell = floor(slot * 0.14);
    vec2 iconRel = inSlot - floor(0.5 * (slot - 5.0 * cell));
    if (all(greaterThanEqual(iconRel, vec2(0.0))) && all(lessThan(iconRel, vec2(5.0 * cell)))) {
        ivec2 bit = ivec2(iconRel / cell);
        if (((ICONS[index] >> uint(24 - (bit.y * 5 + bit.x))) & 1u) != 0u) {
            return vec4(accent * (selected ? 1.0 : 0.6), 1.0);
        }
    }
    return selected ? vec4(panel + accent * 0.06, 0.8) : vec4(panel, 0.6);
}

void main() {
    if (draw.mode == 0u) {
        outColor = vec4(skyColor(viewRay()), 1.0);
        return;
    }
    if (draw.mode == 1u) {
        vec3 dir = viewRay();
        if (abs(dir.y) < 1e-5) discard;
        float t = -frame.camera.y / dir.y;
        if (t <= 0.0) discard;
        vec3 p = frame.camera.xyz + dir * t;
        float blocks = gridLine(p.xz, 1.0) * 0.35;
        float chunks = gridLine(p.xz, 16.0);
        float fade = 1.0 - smoothstep(frame.fog.x * 0.5, frame.fog.y, t);
        float alpha = max(blocks, chunks * 0.8) * fade;
        if (alpha < 0.01) discard;
        vec4 clip = frame.viewProj * vec4(p, 1.0);
        gl_FragDepth = clip.z / clip.w;
        outColor = vec4(mix(vec3(0.9), vec3(1.0, 1.0, 0.8), chunks), alpha);
        return;
    }
    if (frame.viewport.z < 0.5) discard;
    outColor = hud();
    if (outColor.a <= 0.0) discard;
}

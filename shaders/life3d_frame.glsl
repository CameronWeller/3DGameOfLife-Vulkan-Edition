// Per-frame uniforms shared by the prototype's world and screen shaders.
// Matches FrameUniforms in src/main_minimal.cpp (std140).
layout(std140, binding = 0) uniform Frame {
    mat4 viewProj;
    mat4 invViewProj;
    vec4 camera;   // xyz eye position, w seconds since start
    vec4 viewport; // xy framebuffer size, z 1 = HUD visible, w instance capacity
    ivec4 hotbar;  // x selected slot, y slot count
    vec4 fog;      // x fog start, y fog end (blocks)
} frame;

layout(push_constant) uniform Draw {
    uint mode;
} draw;

// Linear-space sky colors; the swapchain applies the sRGB curve.
const vec3 SKY_ZENITH = vec3(0.13, 0.30, 0.85);
const vec3 SKY_HORIZON = vec3(0.60, 0.75, 0.95);
const vec3 SKY_NADIR = vec3(0.20, 0.25, 0.35);

vec3 skyColor(vec3 dir) {
    if (dir.y >= 0.0) return mix(SKY_HORIZON, SKY_ZENITH, pow(dir.y, 0.55));
    return mix(SKY_HORIZON, SKY_NADIR, pow(-dir.y, 0.45));
}

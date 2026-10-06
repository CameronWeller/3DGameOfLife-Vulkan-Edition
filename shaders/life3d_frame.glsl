// Per-frame data shared by the world and screen shaders.
// Must match FrameUniforms in src/render/Renderer.h (std140 layout).
layout(std140, binding = 0) uniform Frame {
    mat4 viewProjection;
    mat4 inverseViewProjection;
    vec4 camera;   // xyz eye position, w seconds since start
    vec4 viewport; // xy framebuffer size, z 1 = HUD visible, w blocks in the block list to draw
    ivec4 hotbar;  // x selected slot (-1 = empty hand), y slot count, z selected cell kind
    vec4 fog;      // x fog start, y fog end (blocks)
    vec4 anim;     // x progress of the last change's birth/death animation (1 = done), y 1 = ambient occlusion on
    vec4 sun;      // xyz direction toward the sun
} frame;

// How the screen shaders draw (Renderer::ScreenMode).
layout(push_constant) uniform Draw {
    uint mode;
} draw;

// How a world fragment is lit (fragLit in the world shaders).
const float LIGHT_NONE = 0.0;  // flat color: outlines
const float LIGHT_FACES = 1.0; // shaded by face direction, sun and ambient occlusion
const float LIGHT_GLOW = 2.0;  // emits its own light: Ember

// Linear-space sky colors; the swapchain applies the sRGB curve.
const vec3 SKY_ZENITH = vec3(0.10, 0.24, 0.72);
const vec3 SKY_HORIZON = vec3(0.62, 0.74, 0.92);
const vec3 SKY_NADIR = vec3(0.16, 0.19, 0.26);
const vec3 SUN_COLOR = vec3(1.0, 0.93, 0.80);

// The sky behind everything and the color fog fades toward. A warm glow around
// the sun and a slightly brighter horizon band give depth without a texture.
vec3 skyColor(vec3 dir) {
    vec3 color = dir.y >= 0.0 ? mix(SKY_HORIZON, SKY_ZENITH, pow(dir.y, 0.5))
                              : mix(SKY_HORIZON, SKY_NADIR, pow(-dir.y, 0.4));
    color += vec3(0.05, 0.04, 0.02) * exp(-abs(dir.y) * 14.0); // horizon haze
    float toSun = max(dot(dir, frame.sun.xyz), 0.0);
    color += SUN_COLOR * (0.25 * pow(toSun, 12.0) + 0.6 * pow(toSun, 220.0));
    return color;
}

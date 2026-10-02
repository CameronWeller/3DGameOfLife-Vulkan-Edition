#version 450
#extension GL_GOOGLE_include_directive : require

// Box outlines drawn as thin instanced bars: instance = box * 12 + edge. Used for
// the targeted block, the placement preview, chunk borders and tutorial marks.

#include "life3d_frame.glsl"

// Two vec4 per box: (min.xyz, edge thickness), (max.xyz, color id).
layout(std430, binding = 2) readonly buffer Boxes { vec4 data[]; } boxes;

layout(location = 0) in vec3 inPosition; // unit cube corner in [-0.5, 0.5]
layout(location = 1) in vec3 inNormal;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec3 fragNormal;
layout(location = 2) out vec3 fragLocal;
layout(location = 3) out vec3 fragWorld;
layout(location = 4) out float fragShade;
layout(location = 5) out float fragLit;

void main() {
    int box = gl_InstanceIndex / 12;
    int edge = gl_InstanceIndex % 12;
    vec4 lo = boxes.data[box * 2];
    vec4 hi = boxes.data[box * 2 + 1];
    int axis = edge / 4;
    vec2 side = vec2((edge & 1) != 0 ? 1.0 : 0.0, (edge & 2) != 0 ? 1.0 : 0.0);
    vec3 center, size = vec3(lo.w);
    if (axis == 0) { center = vec3(0.5 * (lo.x + hi.x), mix(lo.y, hi.y, side.x), mix(lo.z, hi.z, side.y)); size.x = hi.x - lo.x + lo.w; }
    if (axis == 1) { center = vec3(mix(lo.x, hi.x, side.x), 0.5 * (lo.y + hi.y), mix(lo.z, hi.z, side.y)); size.y = hi.y - lo.y + lo.w; }
    if (axis == 2) { center = vec3(mix(lo.x, hi.x, side.x), mix(lo.y, hi.y, side.y), 0.5 * (lo.z + hi.z)); size.z = hi.z - lo.z + lo.w; }
    // Color ids: 0 targeted block, 1 chunk border, 2 placement in empty air,
    // 3-6 tutorial marks: neighbor, born, dies, survives (src/tutorial/Tutorial.cpp),
    // 7 Stone placement, 8 Ember placement.
    const vec3 BOX_COLORS[9] = vec3[9](vec3(0.02), vec3(1.0, 0.8, 0.15), vec3(0.95), vec3(1.0, 0.62, 0.05),
                                       vec3(0.05, 0.75, 0.08), vec3(0.9, 0.05, 0.03), vec3(0.1, 0.35, 1.0),
                                       vec3(0.55, 0.58, 0.66), vec3(1.0, 0.36, 0.06));
    fragColor = BOX_COLORS[clamp(int(hi.w + 0.5), 0, 8)];
    fragNormal = inNormal;
    fragLocal = inPosition;
    fragShade = 1.0;
    fragLit = 0.0;
    vec3 world = center + inPosition * size;
    fragWorld = world;
    gl_Position = frame.viewProj * vec4(world, 1.0);
}

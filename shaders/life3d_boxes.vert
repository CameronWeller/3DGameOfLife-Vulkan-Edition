#version 450
#extension GL_GOOGLE_include_directive : require

// Box outlines: the targeted block, the placement preview, chunk borders and
// tutorial marks. Each of a box's 12 edges is one instance of a unit cube,
// stretched into a thin bar: instance = box * 12 + edge.

#include "life3d_frame.glsl"

// Two vec4 per box: (min.xyz, edge thickness), (max.xyz, color id). Written by
// Game::outlineBoxes (struct Box in src/render/Renderer.h).
layout(std430, binding = 2) readonly buffer Boxes { vec4 data[]; } boxes;

layout(location = 0) in vec3 inPosition; // unit cube corner in [-0.5, 0.5]
layout(location = 1) in vec3 inNormal;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec3 fragNormal;
layout(location = 2) out vec3 fragLocal;
layout(location = 3) out vec3 fragWorld;
layout(location = 4) out float fragShade;
layout(location = 5) out float fragLit;

// Color ids; must match BoxColor in src/render/Renderer.h. 3-6 are the tutorial
// marks (neighbor, born, dies, survives), as in Tutorial::markColor.
const vec3 BOX_COLORS[9] = vec3[9](vec3(0.02),              // 0 targeted block
                                   vec3(1.0, 0.8, 0.15),    // 1 chunk border
                                   vec3(0.95),              // 2 placing Life
                                   vec3(1.0, 0.62, 0.05),   // 3 mark: neighbor
                                   vec3(0.05, 0.75, 0.08),  // 4 mark: born
                                   vec3(0.9, 0.05, 0.03),   // 5 mark: dies
                                   vec3(0.1, 0.35, 1.0),    // 6 mark: survives
                                   vec3(0.55, 0.58, 0.66),  // 7 placing Stone
                                   vec3(1.0, 0.36, 0.06));  // 8 placing Ember

void main() {
    int box = gl_InstanceIndex / 12;
    int edge = gl_InstanceIndex % 12;
    vec4 low = boxes.data[box * 2];
    vec4 high = boxes.data[box * 2 + 1];
    float thickness = low.w;

    // Edges 0-3 run along x, 4-7 along y, 8-11 along z. The two low bits of
    // the edge number pick which of the four parallel edges: low or high side
    // on each of the other two axes.
    int axis = edge / 4;
    vec2 side = vec2((edge & 1) != 0 ? 1.0 : 0.0, (edge & 2) != 0 ? 1.0 : 0.0);
    vec3 center;
    vec3 size = vec3(thickness);
    if (axis == 0) {
        center = vec3(0.5 * (low.x + high.x), mix(low.y, high.y, side.x), mix(low.z, high.z, side.y));
        size.x = high.x - low.x + thickness;
    } else if (axis == 1) {
        center = vec3(mix(low.x, high.x, side.x), 0.5 * (low.y + high.y), mix(low.z, high.z, side.y));
        size.y = high.y - low.y + thickness;
    } else {
        center = vec3(mix(low.x, high.x, side.x), mix(low.y, high.y, side.y), 0.5 * (low.z + high.z));
        size.z = high.z - low.z + thickness;
    }

    int colorId = clamp(int(high.w + 0.5), 0, 8);
    fragColor = BOX_COLORS[colorId];
    fragNormal = inNormal;
    fragLocal = inPosition;
    fragShade = 1.0;
    fragLit = LIGHT_NONE;
    vec3 world = center + inPosition * size;
    fragWorld = world;
    gl_Position = frame.viewProjection * vec4(world, 1.0);
}

#version 450
#extension GL_GOOGLE_include_directive : require

// World geometry for the prototype, drawn as instanced unit cubes.
//   mode 0: one block per live cell from the instance list built by life3d_chunks.comp
//   mode 1: box outlines; instance = box * 12 + edge (targeted block, chunk borders, tutorial marks)

#include "life3d_frame.glsl"

layout(std430, binding = 1) readonly buffer Instances { ivec4 cells[]; } instances;
// Two vec4 per box: (min.xyz, edge thickness), (max.xyz, color id).
layout(std430, binding = 2) readonly buffer Boxes { vec4 data[]; } boxes;

layout(location = 0) in vec3 inPosition; // unit cube corner in [-0.5, 0.5]
layout(location = 1) in vec3 inNormal;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec3 fragNormal;
layout(location = 2) out vec3 fragLocal;
layout(location = 3) out vec3 fragWorld;
layout(location = 4) out float fragLit;

float hash(ivec3 p) {
    uint h = uint(p.x) * 73856093u ^ uint(p.y) * 19349663u ^ uint(p.z) * 83492791u;
    h = (h ^ (h >> 13u)) * 1274126177u;
    return float(h & 0xFFFFu) / 65535.0;
}

void main() {
    fragNormal = inNormal;
    fragLocal = inPosition;
    vec3 world;

    if (draw.mode == 0u) {
        if (gl_InstanceIndex >= int(frame.viewport.w)) {
            gl_Position = vec4(0.0, 0.0, 2.0, 1.0); // beyond capacity: outside the clip volume
            return;
        }
        ivec3 cell = instances.cells[gl_InstanceIndex].xyz;
        // Hue drifts slowly through space; a per-block jitter keeps flat walls readable.
        float band = float(cell.y) * 0.021 + float(cell.x + cell.z) * 0.006;
        vec3 base = 0.55 + 0.45 * cos(6.28318 * (vec3(0.0, 0.33, 0.67) + band));
        fragColor = base * (0.88 + 0.16 * hash(cell));
        fragLit = 1.0;
        world = vec3(cell) + 0.5 + inPosition;
    } else {
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
        // 3-6 tutorial marks: neighbor, born, dies, survives (src/tutorial/Tutorial.cpp).
        const vec3 BOX_COLORS[7] = vec3[7](vec3(0.02), vec3(1.0, 0.8, 0.15), vec3(0.95), vec3(1.0, 0.62, 0.05),
                                           vec3(0.05, 0.75, 0.08), vec3(0.9, 0.05, 0.03), vec3(0.1, 0.35, 1.0));
        fragColor = BOX_COLORS[clamp(int(hi.w + 0.5), 0, 6)];
        fragLit = 0.0;
        world = center + inPosition * size;
    }

    fragWorld = world;
    gl_Position = frame.viewProj * vec4(world, 1.0);
}

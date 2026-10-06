#version 450
#extension GL_GOOGLE_include_directive : require

// Blocks, one instance each from the list life3d_build.comp writes. Only the
// three faces that can face the camera are drawn: 12 vertices per instance,
// indexed as two triangles per face (the index buffer is made in
// src/render/Renderer.cpp), and a face whose neighbor block covers it collapses
// to nothing. Corners are darkened by the blocks around them (Minecraft-style
// smooth ambient occlusion), and blocks born or killed by the last generation
// grow in or shrink away.

#include "life3d_frame.glsl"
#include "life3d_instances.glsl"

layout(std430, binding = 1) readonly buffer Instances { uvec2 cells[]; } instances;
layout(std430, binding = 3) readonly buffer Origins { ivec4 cell[]; } origins;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec3 fragNormal;
layout(location = 2) out vec3 fragLocal;
layout(location = 3) out vec3 fragWorld;
layout(location = 4) out float fragShade; // ambient occlusion
layout(location = 5) out float fragLit;   // LIGHT_* in life3d_frame.glsl

// A stable pseudo-random value in [0, 1] per cell, to vary block colors a
// little. The spatial hash of Teschner et al. (2003), then one round of mixing
// (a multiply by an odd constant) so neighboring cells differ.
float hash(ivec3 p) {
    uint h = uint(p.x) * 73856093u ^ uint(p.y) * 19349663u ^ uint(p.z) * 83492791u;
    h = (h ^ (h >> 13u)) * 1274126177u;
    return float(h & 0xFFFFu) / 65535.0;
}

// Whether the cell at offset o (each component -1..1) from this block is
// occupied, from the instance's 27 neighbor bits.
bool occupied(uint neighborBits, ivec3 o) {
    uint k = uint((o.z + 1) * 9 + (o.y + 1) * 3 + (o.x + 1));
    return ((neighborBits >> k) & 1u) != 0u;
}

// How much light reaches a face corner: 0 (in a crevice) to 3 (open), from the
// two blocks beside the corner (side1, side2) and the one diagonal to it, as in
// Minecraft's smooth lighting. n is the face normal; a and b the directions
// from the face center toward the corner.
float cornerLight(uint neighborBits, ivec3 n, ivec3 a, ivec3 b) {
    bool side1 = occupied(neighborBits, n + a);
    bool side2 = occupied(neighborBits, n + b);
    if (side1 && side2) return 0.0;
    return 3.0 - float(side1) - float(side2) - float(occupied(neighborBits, n + a + b));
}

// Emits a vertex outside the clip volume, which drops its triangle.
void cull() {
    gl_Position = vec4(0.0, 0.0, 2.0, 1.0);
    fragColor = vec3(0.0);
    fragNormal = vec3(0.0, 1.0, 0.0);
    fragLocal = vec3(0.0);
    fragWorld = vec3(0.0);
    fragShade = 1.0;
    fragLit = LIGHT_FACES;
}

void main() {
    // The build pass counts every visible block but writes only maxInstances;
    // instances past viewport.w hold no data.
    if (gl_InstanceIndex >= int(frame.viewport.w)) {
        cull();
        return;
    }
    uvec2 instance = instances.cells[gl_InstanceIndex];
    uint neighborBits = instanceNeighbors(instance);
    ivec3 cell = origins.cell[instanceSlot(instance)].xyz + instanceLocalCell(instance);
    vec3 center = vec3(cell) + 0.5;

    // The face for this vertex: vertices 0-3 are the x face, 4-7 y, 8-11 z, each
    // on whichever side of the block looks toward the camera.
    int axis = gl_VertexIndex / 4;
    vec3 toCamera = frame.camera.xyz - center;
    ivec3 normal = ivec3(0);
    normal[axis] = toCamera[axis] >= 0.0 ? 1 : -1;
    bool hidden = occupied(neighborBits, normal) || length(toCamera) > frame.fog.y + 2.0;
    if (hidden) {
        cull();
        return;
    }

    // Size: newborns grow in and the dying shrink away over the animation.
    float progress = frame.anim.x;
    bool born = instanceBorn(instance);
    bool dying = instanceDying(instance);
    float size = 1.0;
    if (born) {
        size = smoothstep(0.0, 1.0, progress);
    } else if (dying) {
        size = 1.0 - smoothstep(0.0, 1.0, progress);
    }
    if (size <= 0.001) {
        cull();
        return;
    }

    // Quad corners in the face plane. The index buffer splits each quad along
    // the diagonal from its first corner; starting one corner later splits it
    // along the other diagonal, which is chosen to be the brighter one so the
    // occlusion gradient stays symmetric.
    ivec3 u = ivec3(0);
    ivec3 v = ivec3(0);
    u[(axis + 1) % 3] = 1;
    v[(axis + 2) % 3] = 1;
    float light[4];
    light[0] = cornerLight(neighborBits, normal, -u, -v);
    light[1] = cornerLight(neighborBits, normal, u, -v);
    light[2] = cornerLight(neighborBits, normal, u, v);
    light[3] = cornerLight(neighborBits, normal, -u, v);
    bool flipDiagonal = light[0] + light[2] < light[1] + light[3];
    int corner = (gl_VertexIndex % 4 + (flipDiagonal ? 1 : 0)) % 4;
    vec2 side = vec2(corner == 1 || corner == 2 ? 0.5 : -0.5, corner >= 2 ? 0.5 : -0.5);
    vec3 offset = 0.5 * vec3(normal) + side.x * vec3(u) + side.y * vec3(v);

    uint kind = instanceKind(instance);
    float jitter = hash(cell);
    vec3 color;
    if (kind == KIND_STONE) {
        color = vec3(0.30, 0.31, 0.34) * (0.85 + 0.25 * jitter);
        fragLit = LIGHT_FACES;
    } else if (kind == KIND_EMBER) {
        // Glows and flickers gently.
        float flicker = 0.85 + 0.15 * sin(frame.camera.w * 3.0 + jitter * 6.28318);
        color = vec3(1.0, 0.36, 0.06) * flicker;
        fragLit = LIGHT_GLOW;
    } else {
        // Life: the hue drifts slowly through space; jitter keeps flat walls readable.
        float band = float(cell.y) * 0.021 + float(cell.x + cell.z) * 0.006;
        vec3 base = 0.55 + 0.45 * cos(6.28318 * (vec3(0.0, 0.33, 0.67) + band));
        color = base * (0.88 + 0.16 * jitter);
        if (born) color = mix(vec3(1.0), color, 0.35 + 0.65 * progress);       // newborns flash white
        if (dying) color = mix(color, vec3(0.35, 0.05, 0.04), 0.6 * progress); // the dying redden
        fragLit = LIGHT_FACES;
    }
    // Light at a corner, from 0 (crevice) to 3 (open), to brightness.
    const float AO_CURVE[4] = float[4](0.42, 0.62, 0.81, 1.0);
    bool smoothLighting = frame.anim.y > 0.5;
    fragColor = color;
    fragShade = smoothLighting ? AO_CURVE[int(light[corner])] : 1.0;
    fragNormal = vec3(normal);
    fragLocal = offset;
    vec3 world = center + offset * size;
    fragWorld = world;
    gl_Position = frame.viewProjection * vec4(world, 1.0);
}

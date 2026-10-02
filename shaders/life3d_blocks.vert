#version 450
#extension GL_GOOGLE_include_directive : require

// Blocks, one instance each from the list life3d_build.comp writes. Only the
// three faces that can face the camera are drawn: 12 vertices per instance,
// indexed as two triangles per face, and a face whose neighbor block covers it
// collapses to nothing. Corners are
// darkened by the blocks around them (Minecraft-style smooth ambient occlusion),
// and blocks born or killed by the last generation grow in or shrink away.

#include "life3d_frame.glsl"

layout(std430, binding = 1) readonly buffer Instances { uvec2 cells[]; } instances;
layout(std430, binding = 3) readonly buffer Origins { ivec4 cell[]; } origins;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec3 fragNormal;
layout(location = 2) out vec3 fragLocal;
layout(location = 3) out vec3 fragWorld;
layout(location = 4) out float fragShade; // ambient occlusion
layout(location = 5) out float fragLit;   // 0 unlit, 1 lit, 2 glowing

float hash(ivec3 p) {
    uint h = uint(p.x) * 73856093u ^ uint(p.y) * 19349663u ^ uint(p.z) * 83492791u;
    h = (h ^ (h >> 13u)) * 1274126177u;
    return float(h & 0xFFFFu) / 65535.0;
}

uint neighborBits;
bool occupied(ivec3 o) {
    return ((neighborBits >> uint((o.z + 1) * 9 + (o.y + 1) * 3 + (o.x + 1))) & 1u) != 0u;
}

// 0 (a crevice) to 3 (open) for a face corner, as in Minecraft's smooth lighting.
float cornerLight(ivec3 n, ivec3 a, ivec3 b) {
    bool s1 = occupied(n + a), s2 = occupied(n + b);
    if (s1 && s2) return 0.0;
    return 3.0 - float(s1) - float(s2) - float(occupied(n + a + b));
}

void cull() {
    gl_Position = vec4(0.0, 0.0, 2.0, 1.0); // outside the clip volume: the triangle is dropped
    fragColor = vec3(0.0);
    fragNormal = vec3(0.0, 1.0, 0.0);
    fragLocal = vec3(0.0);
    fragWorld = vec3(0.0);
    fragShade = 1.0;
    fragLit = 1.0;
}

void main() {
    if (gl_InstanceIndex >= int(frame.viewport.w)) { cull(); return; }
    uvec2 data = instances.cells[gl_InstanceIndex];
    neighborBits = data.x;
    uint local = data.y & 0x7FFFu;
    ivec3 cell = origins.cell[data.y >> 15].xyz + ivec3(local & 31u, (local >> 5) & 31u, local >> 10);
    vec3 center = vec3(cell) + 0.5;

    // Face: the side of this axis that looks toward the camera.
    int axis = gl_VertexIndex / 4;
    vec3 toCamera = frame.camera.xyz - center;
    ivec3 normal = ivec3(0);
    normal[axis] = toCamera[axis] >= 0.0 ? 1 : -1;
    if (occupied(normal) || length(toCamera) > frame.fog.y + 2.0) { cull(); return; }

    float progress = frame.anim.x;
    bool born = (data.x & (1u << 29)) != 0u, dying = (data.x & (1u << 30)) != 0u;
    float size = born ? smoothstep(0.0, 1.0, progress) : (dying ? 1.0 - smoothstep(0.0, 1.0, progress) : 1.0);
    if (size <= 0.001) { cull(); return; }

    // Quad corners in the face plane. The index buffer splits each quad along
    // the diagonal from its first corner; starting one corner later splits it
    // along the other diagonal, which is chosen to be the brighter one so the
    // occlusion gradient stays symmetric.
    ivec3 u = ivec3(0), v = ivec3(0);
    u[(axis + 1) % 3] = 1;
    v[(axis + 2) % 3] = 1;
    float light[4];
    light[0] = cornerLight(normal, -u, -v);
    light[1] = cornerLight(normal, u, -v);
    light[2] = cornerLight(normal, u, v);
    light[3] = cornerLight(normal, -u, v);
    int corner = (gl_VertexIndex % 4 + (light[0] + light[2] < light[1] + light[3] ? 1 : 0)) % 4;
    vec2 side = vec2(corner == 1 || corner == 2 ? 0.5 : -0.5, corner >= 2 ? 0.5 : -0.5);
    vec3 offset = 0.5 * vec3(normal) + side.x * vec3(u) + side.y * vec3(v);

    uint kind = (data.x >> 27) & 3u;
    float jitter = hash(cell);
    vec3 color;
    if (kind == 1u) { // Stone
        color = vec3(0.30, 0.31, 0.34) * (0.85 + 0.25 * jitter);
        fragLit = 1.0;
    } else if (kind == 2u) { // Ember: glows and flickers gently
        float flicker = 0.85 + 0.15 * sin(frame.camera.w * 3.0 + jitter * 6.28318);
        color = vec3(1.0, 0.36, 0.06) * flicker;
        fragLit = 2.0;
    } else { // Life: hue drifts slowly through space, jitter keeps flat walls readable
        float band = float(cell.y) * 0.021 + float(cell.x + cell.z) * 0.006;
        vec3 base = 0.55 + 0.45 * cos(6.28318 * (vec3(0.0, 0.33, 0.67) + band));
        color = base * (0.88 + 0.16 * jitter);
        if (born) color = mix(vec3(1.0), color, 0.35 + 0.65 * progress);       // newborns flash white
        if (dying) color = mix(color, vec3(0.35, 0.05, 0.04), 0.6 * progress); // the dying redden
        fragLit = 1.0;
    }
    const float AO_CURVE[4] = float[4](0.42, 0.62, 0.81, 1.0);
    fragColor = color;
    fragShade = frame.anim.y > 0.5 ? AO_CURVE[int(light[corner])] : 1.0;
    fragNormal = vec3(normal);
    fragLocal = offset;
    vec3 world = center + offset * size;
    fragWorld = world;
    gl_Position = frame.viewProj * vec4(world, 1.0);
}

#version 450
#extension GL_GOOGLE_include_directive : require

// Shading for blocks (life3d_blocks.vert) and outlines (life3d_boxes.vert).

#include "life3d_frame.glsl"

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec3 fragLocal;
layout(location = 3) in vec3 fragWorld;
layout(location = 4) in float fragShade;
layout(location = 5) in float fragLit;

layout(location = 0) out vec4 outColor;

void main() {
    vec3 color = fragColor;
    if (fragLit > 0.5) {
        vec3 normal = normalize(fragNormal);
        // Fixed per-face shading (top brightest, bottom darkest) warmed by the sun
        // and cooled by the sky, times the corner occlusion.
        float face = normal.y > 0.5 ? 1.0 : normal.y < -0.5 ? 0.5 : abs(normal.x) > 0.5 ? 0.8 : 0.66;
        float sun = max(dot(normal, frame.sun.xyz), 0.0);
        vec3 light = face * (vec3(0.80, 0.84, 0.92) + vec3(0.26, 0.22, 0.14) * sun) * fragShade;
        // Darken block edges so neighboring blocks stay distinguishable.
        vec3 a = abs(fragLocal) * 2.0;
        float edge = max(max(a.x * (1.0 - abs(normal.x)), a.y * (1.0 - abs(normal.y))), a.z * (1.0 - abs(normal.z)));
        light *= 1.0 - 0.28 * smoothstep(0.86, 0.97, edge);
        color = fragLit > 1.5 ? color * (0.75 + 0.5 * fragShade) : color * light; // embers glow
    }
    vec3 toFragment = fragWorld - frame.camera.xyz;
    float fogAmount = smoothstep(frame.fog.x, frame.fog.y, length(toFragment));
    outColor = vec4(mix(color, skyColor(normalize(toFragment)), fogAmount), 1.0);
}

#version 450
#extension GL_GOOGLE_include_directive : require

#include "life3d_frame.glsl"

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec3 fragLocal;
layout(location = 3) in vec3 fragWorld;
layout(location = 4) in float fragLit;

layout(location = 0) out vec4 outColor;

void main() {
    vec3 color = fragColor;
    if (fragLit > 0.5) {
        vec3 normal = normalize(fragNormal);
        // Fixed per-face shading like Minecraft: top brightest, bottom darkest.
        float shade = normal.y > 0.5 ? 1.0 : normal.y < -0.5 ? 0.5 : abs(normal.x) > 0.5 ? 0.8 : 0.65;
        // Darken block edges so neighboring blocks stay distinguishable.
        vec3 a = abs(fragLocal) * 2.0;
        float edge = max(max(a.x * (1.0 - abs(normal.x)), a.y * (1.0 - abs(normal.y))), a.z * (1.0 - abs(normal.z)));
        color *= shade * (1.0 - 0.3 * smoothstep(0.86, 0.97, edge));
    }
    vec3 toFragment = fragWorld - frame.camera.xyz;
    float fogAmount = smoothstep(frame.fog.x, frame.fog.y, length(toFragment));
    outColor = vec4(mix(color, skyColor(normalize(toFragment)), fogAmount), 1.0);
}

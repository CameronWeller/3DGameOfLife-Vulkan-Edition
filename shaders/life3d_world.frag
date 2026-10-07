#version 450
#extension GL_GOOGLE_include_directive : require

// Shading for blocks (life3d_blocks.vert) and outlines (life3d_boxes.vert):
// one of three lighting styles (fragLit), then distance fog toward the sky.

#include "life3d_frame.glsl"

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec3 fragLocal; // position on the block, -0.5..0.5 per axis
layout(location = 3) in vec3 fragWorld;
layout(location = 4) in float fragShade; // ambient occlusion, 1 = none
layout(location = 5) in float fragLit;   // LIGHT_* in life3d_frame.glsl

layout(location = 0) out vec4 outColor;

// Fixed brightness per face direction, as in Minecraft: tops brightest,
// bottoms darkest, the x and z sides in between.
float faceBrightness(vec3 normal) {
    if (normal.y > 0.5) return 1.0;
    if (normal.y < -0.5) return 0.5;
    if (abs(normal.x) > 0.5) return 0.8;
    return 0.66;
}

void main() {
    vec3 color = fragColor;
    // fragLit is a whole number per triangle; comparing against the midpoints
    // between the LIGHT_* values is robust to interpolation rounding.
    if (fragLit > LIGHT_GLOW - 0.5) {
        color = color * (0.75 + 0.5 * fragShade); // embers glow; occlusion only dims them a little
    } else if (fragLit > LIGHT_FACES - 0.5) {
        vec3 normal = normalize(fragNormal);
        // Face brightness, warmed by the sun and cooled by the sky, times the
        // corner occlusion.
        float sun = max(dot(normal, frame.sun.xyz), 0.0);
        vec3 light = faceBrightness(normal) * (vec3(0.80, 0.84, 0.92) + vec3(0.26, 0.22, 0.14) * sun) * fragShade;
        // Darken a thin band along block edges so neighboring blocks stay
        // distinguishable. fromCenter is 0 at the block center and 1 at its
        // surface on each axis; masking out the normal's axis leaves the two
        // in-face axes, so `edge` reaches 1 at the face's border.
        vec3 fromCenter = abs(fragLocal) * 2.0;
        float edge = max(max(fromCenter.x * (1.0 - abs(normal.x)), fromCenter.y * (1.0 - abs(normal.y))),
                         fromCenter.z * (1.0 - abs(normal.z)));
        light *= 1.0 - 0.28 * smoothstep(0.86, 0.97, edge);
        color = color * light;
    }
    // Fog: fade toward the sky color with distance.
    vec3 toFragment = fragWorld - frame.camera.xyz;
    float fogAmount = smoothstep(frame.fog.x, frame.fog.y, length(toFragment));
    outColor = vec4(mix(color, skyColor(normalize(toFragment)), fogAmount), 1.0);
}

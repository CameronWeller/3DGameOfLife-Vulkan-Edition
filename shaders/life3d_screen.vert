#version 450

// Fullscreen triangle for the sky, ground grid and HUD passes.
layout(location = 0) out vec2 ndc;

void main() {
    ndc = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2) * 2.0 - 1.0;
    gl_Position = vec4(ndc, 0.0, 1.0);
}

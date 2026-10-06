#version 450

// One triangle that covers the whole screen, for the sky, ground grid and HUD
// passes. Vertices 0, 1, 2 land at (-1, -1), (3, -1) and (-1, 3) in clip space:
// a triangle twice the size of the screen, so the screen is inside it and no
// diagonal seam is drawn.
layout(location = 0) out vec2 ndc;

void main() {
    // Bit 1 of (index << 1) is bit 0 of the index; bit 1 of index is itself.
    ndc = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2) * 2.0 - 1.0;
    gl_Position = vec4(ndc, 0.0, 1.0);
}

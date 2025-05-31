#version 450

layout(location = 0) out vec4 outColor;

void main() {
    // Create a nice gradient based on screen position
    vec2 uv = gl_FragCoord.xy / vec2(1280.0, 720.0); // Assuming default resolution
    
    // Blue to purple gradient
    vec3 color = mix(
        vec3(0.1, 0.2, 0.4),  // Deep blue
        vec3(0.4, 0.1, 0.5),  // Purple
        uv.x + uv.y * 0.5
    );
    
    outColor = vec4(color, 1.0);
} 
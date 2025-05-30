#version 450

// Uniform buffers
layout(binding = 0) uniform ViewProjUBO {
    mat4 view;
    mat4 proj;
} viewProj;

// Push constants for model matrix
layout(push_constant) uniform ModelUBO {
    mat4 model;
} model;

// Input attributes
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;

// Output to fragment shader
layout(location = 0) out vec3 fragNormal;
layout(location = 1) out vec3 fragPos;
layout(location = 2) out vec2 fragTexCoord;
layout(location = 3) out vec3 fragLightPos;

void main() {
    // Calculate world position
    vec4 worldPos = model.model * vec4(inPosition, 1.0);
    fragPos = worldPos.xyz;
    
    // Transform normal to world space
    fragNormal = mat3(transpose(inverse(model.model))) * inNormal;
    
    // Pass texture coordinates
    fragTexCoord = inTexCoord;
    
    // Pass light position
    fragLightPos = vec3(10.0, 10.0, 10.0); // Light position in world space
    
    // Project vertex
    gl_Position = viewProj.proj * viewProj.view * worldPos;
} 
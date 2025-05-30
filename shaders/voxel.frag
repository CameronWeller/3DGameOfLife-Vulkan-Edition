#version 450

layout(location = 0) in vec3 inNormal;
layout(location = 1) in vec3 inWorldPos;
layout(location = 2) in float inLOD;

layout(location = 0) out vec4 outColor;

layout(push_constant) uniform PushConstants {
    mat4 viewProj;
    vec3 cameraPos;
    float voxelSize;
    vec4 frustumPlanes[6];
} pushConstants;

layout(binding = 0) uniform sampler3D stateTexture;

// LOD levels for different distances
const float LOD_DISTANCES[4] = float[4](1.0, 2.0, 4.0, 8.0);
const float LOD_ALPHAS[4] = float[4](0.8, 0.6, 0.4, 0.2);

void main() {
    // Early depth test
    if (gl_FragCoord.z > 0.99) {
        discard;
    }
    
    // Calculate distance-based LOD
    float dist = length(pushConstants.cameraPos - inWorldPos);
    float lodLevel = min(3.0, floor(log2(dist / pushConstants.voxelSize)));
    
    // Sample voxel state with LOD
    vec3 texCoord = inWorldPos / (vec3(256.0) * pushConstants.voxelSize);
    float state = textureLod(stateTexture, texCoord, lodLevel).r;
    
    if (state < 0.5) {
        discard;
    }
    
    // Simplified lighting for better performance
    vec3 normal = normalize(inNormal);
    vec3 lightDir = normalize(vec3(1.0, 1.0, 1.0));
    
    float diffuse = max(dot(normal, lightDir), 0.0);
    float ambient = 0.2;
    
    // Color based on LOD level
    vec3 baseColor = vec3(0.2, 0.6, 1.0);
    vec3 color = baseColor * (ambient + diffuse);
    
    // Adjust alpha based on LOD
    float alpha = LOD_ALPHAS[int(lodLevel)];
    
    // Add some variation based on position
    color += vec3(0.1) * sin(inWorldPos.x * 0.1 + inWorldPos.y * 0.1 + inWorldPos.z * 0.1);
    
    outColor = vec4(color, alpha);
} 
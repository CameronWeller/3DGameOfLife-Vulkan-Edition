#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inInstancePosition;
layout(location = 3) in float inLOD;
layout(location = 4) in vec4 inColor;  // Instance color
layout(location = 5) in float inAge;   // Cell age for color variation

layout(location = 0) out vec3 outNormal;
layout(location = 1) out vec3 outWorldPos;
layout(location = 2) out float outLOD;
layout(location = 3) out vec4 outColor;
layout(location = 4) out float outAge;

layout(push_constant) uniform PushConstants {
    mat4 viewProj;
    vec3 cameraPos;
    float voxelSize;
    vec4 frustumPlanes[6];    // For frustum culling
    float minLODDistance;     // Minimum distance for LOD transitions
    float maxLODDistance;     // Maximum distance for LOD transitions
    float time;               // For animation effects
    int renderMode;           // 0: Solid, 1: Wireframe, 2: Points
} pushConstants;

// LOD distance calculation
float calculateLOD(vec3 worldPos) {
    float distance = length(worldPos - pushConstants.cameraPos);
    float lod = clamp((distance - pushConstants.minLODDistance) / 
                     (pushConstants.maxLODDistance - pushConstants.minLODDistance), 
                     0.0, 1.0);
    return lod;
}

bool isVisible(vec3 position, float size) {
    // Check if the voxel's bounding sphere is visible
    for (int i = 0; i < 6; i++) {
        if (dot(vec4(position, 1.0), pushConstants.frustumPlanes[i]) + size < 0.0) {
            return false;
        }
    }
    return true;
}

// Calculate vertex position based on LOD
vec3 calculateVertexPosition(vec3 basePosition, float lod) {
    // For higher LODs, we can simplify the geometry
    if (lod > 0.8) {
        // At highest LOD, use a simplified cube
        return round(basePosition * 2.0) * 0.5;
    } else if (lod > 0.5) {
        // At medium LOD, use a slightly simplified cube
        return round(basePosition * 4.0) * 0.25;
    }
    // At lowest LOD, use full detail
    return basePosition;
}

void main() {
    vec3 worldPos = inInstancePosition + inPosition * pushConstants.voxelSize;
    
    // Calculate dynamic LOD based on distance
    float dynamicLOD = max(inLOD, calculateLOD(worldPos));
    
    // Frustum culling with LOD-aware size
    float cullingSize = pushConstants.voxelSize * (1.0 + dynamicLOD) * 0.866;
    if (!isVisible(worldPos, cullingSize)) {
        gl_Position = vec4(0.0);
        return;
    }
    
    // Calculate final vertex position with LOD
    vec3 finalPosition = worldPos + calculateVertexPosition(inPosition, dynamicLOD) * pushConstants.voxelSize;
    
    // Apply render mode specific transformations
    if (pushConstants.renderMode == 1) { // Wireframe
        // Slightly expand vertices for wireframe
        finalPosition += inNormal * pushConstants.voxelSize * 0.01;
    } else if (pushConstants.renderMode == 2) { // Points
        // Center vertices for point rendering
        finalPosition = worldPos;
    }
    
    gl_Position = pushConstants.viewProj * vec4(finalPosition, 1.0);
    
    outNormal = inNormal;
    outWorldPos = finalPosition;
    outLOD = dynamicLOD;
    outColor = inColor;
    outAge = inAge;
} 
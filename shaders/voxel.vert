#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inInstancePosition;
layout(location = 3) in float inLOD;

layout(location = 0) out vec3 outNormal;
layout(location = 1) out vec3 outWorldPos;
layout(location = 2) out float outLOD;

layout(push_constant) uniform PushConstants {
    mat4 viewProj;
    vec3 cameraPos;
    float voxelSize;
    vec4 frustumPlanes[6]; // For frustum culling
} pushConstants;

bool isVisible(vec3 position, float size) {
    // Check if the voxel's bounding sphere is visible
    for (int i = 0; i < 6; i++) {
        if (dot(vec4(position, 1.0), pushConstants.frustumPlanes[i]) + size < 0.0) {
            return false;
        }
    }
    return true;
}

void main() {
    vec3 worldPos = inInstancePosition + inPosition * pushConstants.voxelSize;
    
    // Frustum culling
    if (!isVisible(worldPos, pushConstants.voxelSize * 0.866)) { // 0.866 is sqrt(3)/2 for sphere radius
        gl_Position = vec4(0.0);
        return;
    }
    
    gl_Position = pushConstants.viewProj * vec4(worldPos, 1.0);
    
    outNormal = inNormal;
    outWorldPos = worldPos;
    outLOD = inLOD;
} 
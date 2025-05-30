#version 450

layout(location = 0) in vec3 inNormal;
layout(location = 1) in vec3 inWorldPos;
layout(location = 2) in float inLOD;
layout(location = 3) in vec4 inColor;
layout(location = 4) in float inAge;

layout(location = 0) out vec4 outColor;

layout(push_constant) uniform PushConstants {
    mat4 viewProj;
    vec3 cameraPos;
    float voxelSize;
    vec4 frustumPlanes[6];
    float minLODDistance;
    float maxLODDistance;
    float time;
    int renderMode;
} pushConstants;

layout(binding = 0) uniform sampler3D stateTexture;

// LOD levels for different distances
const float LOD_DISTANCES[4] = float[4](1.0, 2.0, 4.0, 8.0);
const float LOD_ALPHAS[4] = float[4](0.8, 0.6, 0.4, 0.2);

// Lighting parameters
const vec3 lightPos = vec3(10.0, 10.0, 10.0);
const vec3 lightColor = vec3(1.0, 1.0, 1.0);
const float ambientStrength = 0.2;
const float specularStrength = 0.5;
const float shininess = 32.0;

// Color transition parameters
const float colorTransitionSpeed = 0.5;
const vec4 youngColor = vec4(0.0, 0.8, 0.2, 1.0);  // Green
const vec4 matureColor = vec4(0.8, 0.2, 0.0, 1.0); // Red
const vec4 oldColor = vec4(0.2, 0.2, 0.8, 1.0);    // Blue

// Calculate lighting
vec3 calculateLighting(vec3 normal, vec3 worldPos) {
    // Ambient
    vec3 ambient = ambientStrength * lightColor;
    
    // Diffuse
    vec3 lightDir = normalize(lightPos - worldPos);
    float diff = max(dot(normal, lightDir), 0.0);
    vec3 diffuse = diff * lightColor;
    
    // Specular
    vec3 viewDir = normalize(pushConstants.cameraPos - worldPos);
    vec3 reflectDir = reflect(-lightDir, normal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), shininess);
    vec3 specular = specularStrength * spec * lightColor;
    
    return ambient + diffuse + specular;
}

// Calculate age-based color
vec4 calculateAgeColor(float age) {
    float normalizedAge = clamp(age / 100.0, 0.0, 1.0);
    vec4 color;
    
    if (normalizedAge < 0.33) {
        color = mix(youngColor, matureColor, normalizedAge * 3.0);
    } else if (normalizedAge < 0.66) {
        color = mix(matureColor, oldColor, (normalizedAge - 0.33) * 3.0);
    } else {
        color = oldColor;
    }
    
    return color;
}

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
    
    // Calculate lighting
    vec3 lighting = calculateLighting(normalize(inNormal), inWorldPos);
    
    // Calculate base color based on age and instance color
    vec4 ageColor = calculateAgeColor(inAge);
    vec4 baseColor = mix(ageColor, inColor, 0.5);
    
    // Apply LOD-based transparency
    float alpha = 1.0;
    if (pushConstants.renderMode == 1) { // Wireframe
        alpha = 0.8;
    } else if (pushConstants.renderMode == 2) { // Points
        alpha = 0.6;
    } else {
        // Fade out with distance
        alpha = 1.0 - inLOD * 0.5;
    }
    
    // Apply lighting and final color
    vec3 finalColor = baseColor.rgb * lighting;
    
    // Add subtle pulsing effect based on age
    float pulse = sin(pushConstants.time * colorTransitionSpeed + inAge * 0.1) * 0.1 + 0.9;
    finalColor *= pulse;
    
    outColor = vec4(finalColor, baseColor.a * alpha);
} 
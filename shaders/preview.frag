#version 450

// Input from vertex shader
layout(location = 0) in vec3 fragNormal;
layout(location = 1) in vec3 fragPos;
layout(location = 2) in vec2 fragTexCoord;
layout(location = 3) in vec3 fragLightPos;

// Output color
layout(location = 0) out vec4 outColor;

// Material properties
struct Material {
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
    float shininess;
};

// Light properties
struct Light {
    vec3 position;
    vec3 color;
    float intensity;
};

// Hardcoded material and light for preview
const Material material = Material(
    vec3(0.1, 0.1, 0.1),  // ambient
    vec3(0.7, 0.7, 0.7),  // diffuse
    vec3(1.0, 1.0, 1.0),  // specular
    32.0                   // shininess
);

const Light light = Light(
    vec3(10.0, 10.0, 10.0),  // position
    vec3(1.0, 1.0, 1.0),     // color
    1.0                       // intensity
);

void main() {
    // Normalize vectors
    vec3 normal = normalize(fragNormal);
    vec3 lightDir = normalize(fragLightPos - fragPos);
    vec3 viewDir = normalize(-fragPos);
    vec3 reflectDir = reflect(-lightDir, normal);
    
    // Calculate distances for attenuation
    float distance = length(fragLightPos - fragPos);
    float attenuation = 1.0 / (1.0 + 0.09 * distance + 0.032 * distance * distance);
    
    // Ambient
    vec3 ambient = material.ambient * light.color;
    
    // Diffuse
    float diff = max(dot(normal, lightDir), 0.0);
    vec3 diffuse = diff * material.diffuse * light.color;
    
    // Specular
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), material.shininess);
    vec3 specular = spec * material.specular * light.color;
    
    // Combine lighting components
    vec3 result = (ambient + diffuse + specular) * light.intensity * attenuation;
    
    // Output final color
    outColor = vec4(result, 1.0);
} 
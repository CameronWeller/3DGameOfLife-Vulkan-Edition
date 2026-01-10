#pragma once

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <string>
#include <memory>
#include <vector>
#include "memory/OpenGLMemoryManager.h"
#include "diagnostics/OpenGLProfiler.h"

namespace OpenGLHIP {

struct RenderPushConstants {
    glm::mat4 viewProj;
    glm::vec3 cameraPos;
    float voxelSize;
    glm::vec4 frustumPlanes[6];
    float minLODDistance;
    float maxLODDistance;
    float time;
    int renderMode;
};

class OpenGLRenderer {
public:
    OpenGLRenderer();
    ~OpenGLRenderer();

    void initialize();
    void cleanup();
    
    // Shader management
    bool loadShaders(const std::string& vertexPath, const std::string& fragmentPath);
    bool compileShader(const std::string& vertexSource, const std::string& fragmentSource);
    bool linkProgram();
    
    // Pipeline setup
    void setupRenderState();
    void setViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height);
    void clear(float r = 0.1f, float g = 0.1f, float b = 0.1f, float a = 1.0f);
    void clearColor(float r = 0.1f, float g = 0.1f, float b = 0.1f, float a = 1.0f);
    void clearDepth(float depth = 1.0f);
    
    // Uniform buffer management
    void createUniformBuffer();
    void updateUniformBuffer(const RenderPushConstants& constants);
    void bindUniformBuffer();
    
    // Rendering
    void beginFrame();
    void endFrame();
    void setRenderMode(int mode);
    
    // Getters
    GLuint getProgram() const { return program_; }
    bool isInitialized() const { return initialized_; }

private:
    GLuint program_ = 0;
    GLuint vertexShader_ = 0;
    GLuint fragmentShader_ = 0;
    
    std::unique_ptr<OpenGLBuffer> uniformBuffer_;
    
    bool initialized_ = false;
    int renderMode_ = 0;
    
    // Shader compilation helpers
    std::string readShaderFile(const std::string& path);
    bool compileShaderStage(GLuint shader, const std::string& source, const std::string& type);
    void logShaderError(GLuint shader, const std::string& type);
    void logProgramError(GLuint program);
    
    void cleanup();
};

} // namespace OpenGLHIP

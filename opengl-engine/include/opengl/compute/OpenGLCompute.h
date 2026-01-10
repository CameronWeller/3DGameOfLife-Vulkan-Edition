#pragma once

#include <glad/glad.h>
#include <string>
#include <vector>
#include <memory>
#include <cstdint>
#include <unordered_map>
#include "memory/OpenGLMemoryManager.h"
#include "diagnostics/OpenGLProfiler.h"

namespace OpenGLHIP {

struct ComputePushConstants {
    uint32_t width;
    uint32_t height;
    uint32_t depth;
    float time;
    uint32_t ruleSet;  // 0: Classic, 1: HighLife, 2: Day & Night, 3: Custom, 4: 5766, 5: 4555
    uint32_t surviveMin;
    uint32_t surviveMax;
    uint32_t birthCount;
    
    // Padding to ensure std140 alignment
    uint32_t padding[3];
};

class OpenGLCompute {
public:
    OpenGLCompute();
    ~OpenGLCompute();

    // Shader management
    bool loadShader(const std::string& shaderPath);
    bool compileShader(const std::string& shaderSource);
    bool linkProgram();
    
    // Buffer management
    void createStateBuffers(uint32_t width, uint32_t height, uint32_t depth);
    void destroyStateBuffers();
    void updateStateBuffer(const std::vector<uint32_t>& state, bool isCurrent);
    void readStateBuffer(std::vector<uint32_t>& state, bool isCurrent);
    
    // Push constants (converted to uniform buffer)
    void updatePushConstants(const ComputePushConstants& constants);
    void createUniformBuffer();
    void destroyUniformBuffer();
    
    // Dispatch
    void dispatch(uint32_t width, uint32_t height, uint32_t depth);
    void waitForCompletion();
    
    // Binding management
    void bindBuffers();
    void unbindBuffers();
    
    // Getters
    GLuint getProgram() const { return program_; }
    bool isInitialized() const { return initialized_; }
    
    // Memory access
    const std::unique_ptr<OpenGLBuffer>& getCurrentStateBuffer() const { return currentStateBuffer_; }
    const std::unique_ptr<OpenGLBuffer>& getNextStateBuffer() const { return nextStateBuffer_; }

private:
    GLuint program_ = 0;
    GLuint computeShader_ = 0;
    GLuint uniformBuffer_ = 0;
    
    std::unique_ptr<OpenGLBuffer> currentStateBuffer_;
    std::unique_ptr<OpenGLBuffer> nextStateBuffer_;
    std::unique_ptr<OpenGLBuffer> pushConstantsBuffer_;
    
    uint32_t gridWidth_ = 0;
    uint32_t gridHeight_ = 0;
    uint32_t gridDepth_ = 0;
    size_t bufferSize_ = 0;
    
    bool initialized_ = false;
    std::string shaderSource_;
    std::string shaderPath_;
    
    GLsync fence_ = nullptr;
    
    // Shader compilation helpers
    std::string readShaderFile(const std::string& path);
    bool compileShaderStage(GLuint shader, const std::string& source);
    void logShaderError(GLuint shader, const std::string& type);
    void logProgramError(GLuint program);
    
    void cleanup();
};

} // namespace OpenGLHIP

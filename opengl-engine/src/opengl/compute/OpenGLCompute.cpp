#include "opengl/compute/OpenGLCompute.h"
#include "../../../../include/Logger.h"
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <algorithm>
#include <filesystem>

namespace OpenGLHIP {

OpenGLCompute::OpenGLCompute() = default;

OpenGLCompute::~OpenGLCompute() {
    cleanup();
}

void OpenGLCompute::cleanup() {
    if (fence_ != nullptr) {
        glDeleteSync(fence_);
        fence_ = nullptr;
    }
    
    destroyUniformBuffer();
    destroyStateBuffers();
    
    if (computeShader_ != 0) {
        glDeleteShader(computeShader_);
        computeShader_ = 0;
    }
    
    if (program_ != 0) {
        glDeleteProgram(program_);
        program_ = 0;
    }
    
    initialized_ = false;
}

std::string OpenGLCompute::readShaderFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open shader file: " + path);
    }
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

bool OpenGLCompute::loadShader(const std::string& shaderPath) {
    try {
        shaderPath_ = shaderPath;
        shaderSource_ = readShaderFile(shaderPath);
        
        VulkanHIP::Logger::getInstance().log(VulkanHIP::Logger::LogLevel::Info,
            "Shader loaded from: " + shaderPath);
        return true;
    } catch (const std::exception& e) {
        VulkanHIP::Logger::getInstance().log(VulkanHIP::Logger::LogLevel::Error,
            std::string("Failed to load shader: ") + e.what());
        return false;
    }
}

bool OpenGLCompute::compileShader(const std::string& shaderSource) {
    shaderSource_ = shaderSource;
    
    // Create compute shader
    computeShader_ = glCreateShader(GL_COMPUTE_SHADER);
    if (computeShader_ == 0) {
        VulkanHIP::Logger::getInstance().log(VulkanHIP::Logger::LogLevel::Error,
            "Failed to create compute shader");
        return false;
    }
    
    // Compile shader
    if (!compileShaderStage(computeShader_, shaderSource_)) {
        glDeleteShader(computeShader_);
        computeShader_ = 0;
        return false;
    }
    
    VulkanHIP::Logger::getInstance().log(VulkanHIP::Logger::LogLevel::Info,
        "Compute shader compiled successfully");
    return true;
}

bool OpenGLCompute::compileShaderStage(GLuint shader, const std::string& source) {
    const char* sourceCStr = source.c_str();
    glShaderSource(shader, 1, &sourceCStr, nullptr);
    glCompileShader(shader);
    
    GLint success = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (success == GL_FALSE) {
        logShaderError(shader, "COMPUTE");
        return false;
    }
    
    return true;
}

void OpenGLCompute::logShaderError(GLuint shader, const std::string& type) {
    GLint logLength = 0;
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLength);
    
    if (logLength > 0) {
        std::vector<char> log(logLength);
        glGetShaderInfoLog(shader, logLength, nullptr, log.data());
        
        std::string errorMsg = "Shader compilation error (" + type + "):\n" + std::string(log.data());
        VulkanHIP::Logger::getInstance().log(VulkanHIP::Logger::LogLevel::Error, errorMsg);
    }
}

void OpenGLCompute::logProgramError(GLuint program) {
    GLint logLength = 0;
    glGetProgramiv(program, GL_INFO_LOG_LENGTH, &logLength);
    
    if (logLength > 0) {
        std::vector<char> log(logLength);
        glGetProgramInfoLog(program, logLength, nullptr, log.data());
        
        std::string errorMsg = "Program linking error:\n" + std::string(log.data());
        VulkanHIP::Logger::getInstance().log(VulkanHIP::Logger::LogLevel::Error, errorMsg);
    }
}

bool OpenGLCompute::linkProgram() {
    if (computeShader_ == 0) {
        VulkanHIP::Logger::getInstance().log(VulkanHIP::Logger::LogLevel::Error,
            "Cannot link program: compute shader not compiled");
        return false;
    }
    
    // Create program
    program_ = glCreateProgram();
    if (program_ == 0) {
        VulkanHIP::Logger::getInstance().log(VulkanHIP::Logger::LogLevel::Error,
            "Failed to create compute program");
        return false;
    }
    
    // Attach shader
    glAttachShader(program_, computeShader_);
    
    // Link program
    glLinkProgram(program_);
    
    GLint success = 0;
    glGetProgramiv(program_, GL_LINK_STATUS, &success);
    if (success == GL_FALSE) {
        logProgramError(program_);
        glDeleteProgram(program_);
        program_ = 0;
        return false;
    }
    
    // Detach shader (no longer needed after linking)
    glDetachShader(program_, computeShader_);
    
    VulkanHIP::Logger::getInstance().log(VulkanHIP::Logger::LogLevel::Info,
        "Compute program linked successfully");
    
    initialized_ = true;
    return true;
}

void OpenGLCompute::createStateBuffers(uint32_t width, uint32_t height, uint32_t depth) {
    gridWidth_ = width;
    gridHeight_ = height;
    gridDepth_ = depth;
    
    bufferSize_ = width * height * depth * sizeof(uint32_t);
    
    auto& memoryManager = OpenGLMemoryManager::getInstance();
    
    // Create current state buffer (SSBO)
    currentStateBuffer_ = memoryManager.createBuffer(GL_SHADER_STORAGE_BUFFER, bufferSize_, 
                                                     nullptr, GL_DYNAMIC_DRAW);
    if (!currentStateBuffer_) {
        throw std::runtime_error("Failed to create current state buffer");
    }
    currentStateBuffer_->bind();
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, currentStateBuffer_->getId());
    currentStateBuffer_->unbind();
    
    // Create next state buffer (SSBO)
    nextStateBuffer_ = memoryManager.createBuffer(GL_SHADER_STORAGE_BUFFER, bufferSize_,
                                                  nullptr, GL_DYNAMIC_DRAW);
    if (!nextStateBuffer_) {
        throw std::runtime_error("Failed to create next state buffer");
    }
    nextStateBuffer_->bind();
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, nextStateBuffer_->getId());
    nextStateBuffer_->unbind();
    
    std::stringstream ss;
    ss << "Created state buffers: " << width << "x" << height << "x" << depth 
       << " (" << (bufferSize_ / 1024 / 1024) << " MB)";
    VulkanHIP::Logger::getInstance().log(VulkanHIP::Logger::LogLevel::Info, ss.str());
}

void OpenGLCompute::destroyStateBuffers() {
    currentStateBuffer_.reset();
    nextStateBuffer_.reset();
    bufferSize_ = 0;
}

void OpenGLCompute::updateStateBuffer(const std::vector<uint32_t>& state, bool isCurrent) {
    auto& buffer = isCurrent ? currentStateBuffer_ : nextStateBuffer_;
    if (!buffer || state.size() * sizeof(uint32_t) > bufferSize_) {
        throw std::runtime_error("Invalid buffer or state size");
    }
    
    buffer->bind();
    buffer->update(state.data(), 0, state.size() * sizeof(uint32_t));
    buffer->unbind();
}

void OpenGLCompute::readStateBuffer(std::vector<uint32_t>& state, bool isCurrent) {
    auto& buffer = isCurrent ? currentStateBuffer_ : nextStateBuffer_;
    if (!buffer) {
        throw std::runtime_error("Buffer not created");
    }
    
    size_t elementCount = bufferSize_ / sizeof(uint32_t);
    state.resize(elementCount);
    
    buffer->bind();
    void* mapped = buffer->map(GL_READ_ONLY);
    if (mapped) {
        std::memcpy(state.data(), mapped, bufferSize_);
        buffer->unmap();
    } else {
        buffer->unbind();
        throw std::runtime_error("Failed to map buffer for reading");
    }
    buffer->unbind();
}

void OpenGLCompute::createUniformBuffer() {
    auto& memoryManager = OpenGLMemoryManager::getInstance();
    
    // Create uniform buffer for push constants (std140 layout)
    size_t uniformBufferSize = sizeof(ComputePushConstants);
    pushConstantsBuffer_ = memoryManager.createBuffer(GL_UNIFORM_BUFFER, uniformBufferSize,
                                                      nullptr, GL_DYNAMIC_DRAW);
    if (!pushConstantsBuffer_) {
        throw std::runtime_error("Failed to create uniform buffer for push constants");
    }
    
    pushConstantsBuffer_->bind();
    glBindBufferBase(GL_UNIFORM_BUFFER, 0, pushConstantsBuffer_->getId());
    pushConstantsBuffer_->unbind();
    
    VulkanHIP::Logger::getInstance().log(VulkanHIP::Logger::LogLevel::Info,
        "Created uniform buffer for push constants");
}

void OpenGLCompute::destroyUniformBuffer() {
    pushConstantsBuffer_.reset();
}

void OpenGLCompute::updatePushConstants(const ComputePushConstants& constants) {
    if (!pushConstantsBuffer_) {
        createUniformBuffer();
    }
    
    pushConstantsBuffer_->bind();
    pushConstantsBuffer_->update(&constants, 0, sizeof(ComputePushConstants));
    pushConstantsBuffer_->unbind();
    
    // Bind to binding point 0
    glBindBufferBase(GL_UNIFORM_BUFFER, 0, pushConstantsBuffer_->getId());
}

void OpenGLCompute::bindBuffers() {
    if (currentStateBuffer_) {
        currentStateBuffer_->bind();
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, currentStateBuffer_->getId());
    }
    if (nextStateBuffer_) {
        nextStateBuffer_->bind();
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, nextStateBuffer_->getId());
    }
    if (pushConstantsBuffer_) {
        pushConstantsBuffer_->bind();
        glBindBufferBase(GL_UNIFORM_BUFFER, 0, pushConstantsBuffer_->getId());
    }
}

void OpenGLCompute::unbindBuffers() {
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, 0);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, 0);
    glBindBufferBase(GL_UNIFORM_BUFFER, 0, 0);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);
}

void OpenGLCompute::dispatch(uint32_t width, uint32_t height, uint32_t depth) {
    if (!initialized_ || program_ == 0) {
        throw std::runtime_error("Compute program not initialized");
    }
    
    if (!currentStateBuffer_ || !nextStateBuffer_) {
        throw std::runtime_error("State buffers not created");
    }
    
    // Use profiler
    auto& profiler = OpenGLProfiler::getInstance();
    profiler.beginCompute();
    
    // Bind program
    glUseProgram(program_);
    
    // Bind buffers
    bindBuffers();
    
    // Calculate dispatch dimensions (workgroup size is 8x8x8)
    const uint32_t workgroupSize = 8;
    uint32_t numGroupsX = (width + workgroupSize - 1) / workgroupSize;
    uint32_t numGroupsY = (height + workgroupSize - 1) / workgroupSize;
    uint32_t numGroupsZ = (depth + workgroupSize - 1) / workgroupSize;
    
    // Dispatch compute shader
    glDispatchCompute(numGroupsX, numGroupsY, numGroupsZ);
    
    // Memory barrier to ensure compute shader completion
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_BUFFER_UPDATE_BARRIER_BIT);
    
    // Create fence for synchronization
    if (fence_ != nullptr) {
        glDeleteSync(fence_);
    }
    fence_ = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
    
    profiler.endCompute();
    
    // Unbind
    unbindBuffers();
    glUseProgram(0);
}

void OpenGLCompute::waitForCompletion() {
    if (fence_ == nullptr) {
        return;
    }
    
    GLenum result = glClientWaitSync(fence_, GL_SYNC_FLUSH_COMMANDS_BIT, 
                                     GL_TIMEOUT_IGNORED);
    if (result == GL_ALREADY_SIGNALED || result == GL_CONDITION_SATISFIED) {
        glDeleteSync(fence_);
        fence_ = nullptr;
    }
}

} // namespace OpenGLHIP

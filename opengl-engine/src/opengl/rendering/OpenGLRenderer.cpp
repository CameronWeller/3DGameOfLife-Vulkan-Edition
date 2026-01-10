#include "opengl/rendering/OpenGLRenderer.h"
#include "../../../../include/Logger.h"
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <filesystem>

namespace OpenGLHIP {

OpenGLRenderer::OpenGLRenderer() = default;

OpenGLRenderer::~OpenGLRenderer() {
    cleanup();
}

void OpenGLRenderer::cleanup() {
    if (fragmentShader_ != 0) {
        glDeleteShader(fragmentShader_);
        fragmentShader_ = 0;
    }
    
    if (vertexShader_ != 0) {
        glDeleteShader(vertexShader_);
        vertexShader_ = 0;
    }
    
    if (program_ != 0) {
        glDeleteProgram(program_);
        program_ = 0;
    }
    
    uniformBuffer_.reset();
    initialized_ = false;
}

void OpenGLRenderer::initialize() {
    if (initialized_) {
        return;
    }
    
    setupRenderState();
    initialized_ = true;
    
    VulkanHIP::Logger::getInstance().log(VulkanHIP::Logger::LogLevel::Info,
        "OpenGL Renderer initialized");
}

void OpenGLRenderer::setupRenderState() {
    // Enable depth testing
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    
    // Enable face culling
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);
    
    // Enable blending for transparency
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    // Set default clear color
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
}

void OpenGLRenderer::setViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height) {
    glViewport(x, y, width, height);
}

void OpenGLRenderer::clear(float r, float g, float b, float a) {
    glClearColor(r, g, b, a);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void OpenGLRenderer::clearColor(float r, float g, float b, float a) {
    glClearColor(r, g, b, a);
    glClear(GL_COLOR_BUFFER_BIT);
}

void OpenGLRenderer::clearDepth(float depth) {
    glClearDepth(depth);
    glClear(GL_DEPTH_BUFFER_BIT);
}

std::string OpenGLRenderer::readShaderFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open shader file: " + path);
    }
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

bool OpenGLRenderer::loadShaders(const std::string& vertexPath, const std::string& fragmentPath) {
    try {
        std::string vertexSource = readShaderFile(vertexPath);
        std::string fragmentSource = readShaderFile(fragmentPath);
        
        return compileShader(vertexSource, fragmentSource);
    } catch (const std::exception& e) {
        VulkanHIP::Logger::getInstance().log(VulkanHIP::Logger::LogLevel::Error,
            std::string("Failed to load shaders: ") + e.what());
        return false;
    }
}

bool OpenGLRenderer::compileShader(const std::string& vertexSource, const std::string& fragmentSource) {
    // Create shaders
    vertexShader_ = glCreateShader(GL_VERTEX_SHADER);
    fragmentShader_ = glCreateShader(GL_FRAGMENT_SHADER);
    
    if (vertexShader_ == 0 || fragmentShader_ == 0) {
        VulkanHIP::Logger::getInstance().log(VulkanHIP::Logger::LogLevel::Error,
            "Failed to create shaders");
        return false;
    }
    
    // Compile shaders
    if (!compileShaderStage(vertexShader_, vertexSource, "VERTEX")) {
        return false;
    }
    
    if (!compileShaderStage(fragmentShader_, fragmentSource, "FRAGMENT")) {
        return false;
    }
    
    VulkanHIP::Logger::getInstance().log(VulkanHIP::Logger::LogLevel::Info,
        "Shaders compiled successfully");
    return true;
}

bool OpenGLRenderer::compileShaderStage(GLuint shader, const std::string& source, const std::string& type) {
    const char* sourceCStr = source.c_str();
    glShaderSource(shader, 1, &sourceCStr, nullptr);
    glCompileShader(shader);
    
    GLint success = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (success == GL_FALSE) {
        logShaderError(shader, type);
        return false;
    }
    
    return true;
}

void OpenGLRenderer::logShaderError(GLuint shader, const std::string& type) {
    GLint logLength = 0;
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLength);
    
    if (logLength > 0) {
        std::vector<char> log(logLength);
        glGetShaderInfoLog(shader, logLength, nullptr, log.data());
        
        std::string errorMsg = "Shader compilation error (" + type + "):\n" + std::string(log.data());
        VulkanHIP::Logger::getInstance().log(VulkanHIP::Logger::LogLevel::Error, errorMsg);
    }
}

void OpenGLRenderer::logProgramError(GLuint program) {
    GLint logLength = 0;
    glGetProgramiv(program, GL_INFO_LOG_LENGTH, &logLength);
    
    if (logLength > 0) {
        std::vector<char> log(logLength);
        glGetProgramInfoLog(program, logLength, nullptr, log.data());
        
        std::string errorMsg = "Program linking error:\n" + std::string(log.data());
        VulkanHIP::Logger::getInstance().log(VulkanHIP::Logger::LogLevel::Error, errorMsg);
    }
}

bool OpenGLRenderer::linkProgram() {
    if (vertexShader_ == 0 || fragmentShader_ == 0) {
        VulkanHIP::Logger::getInstance().log(VulkanHIP::Logger::LogLevel::Error,
            "Cannot link program: shaders not compiled");
        return false;
    }
    
    // Create program
    program_ = glCreateProgram();
    if (program_ == 0) {
        VulkanHIP::Logger::getInstance().log(VulkanHIP::Logger::LogLevel::Error,
            "Failed to create render program");
        return false;
    }
    
    // Attach shaders
    glAttachShader(program_, vertexShader_);
    glAttachShader(program_, fragmentShader_);
    
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
    
    // Detach shaders (no longer needed after linking)
    glDetachShader(program_, vertexShader_);
    glDetachShader(program_, fragmentShader_);
    
    VulkanHIP::Logger::getInstance().log(VulkanHIP::Logger::LogLevel::Info,
        "Render program linked successfully");
    
    // Create uniform buffer after program is linked
    createUniformBuffer();
    
    return true;
}

void OpenGLRenderer::createUniformBuffer() {
    if (uniformBuffer_) {
        return;  // Already created
    }
    
    auto& memoryManager = OpenGLMemoryManager::getInstance();
    
    size_t uniformBufferSize = sizeof(RenderPushConstants);
    uniformBuffer_ = memoryManager.createBuffer(GL_UNIFORM_BUFFER, uniformBufferSize,
                                               nullptr, GL_DYNAMIC_DRAW);
    if (!uniformBuffer_) {
        throw std::runtime_error("Failed to create uniform buffer for render constants");
    }
    
    uniformBuffer_->bind();
    glBindBufferBase(GL_UNIFORM_BUFFER, 0, uniformBuffer_->getId());
    uniformBuffer_->unbind();
    
    VulkanHIP::Logger::getInstance().log(VulkanHIP::Logger::LogLevel::Info,
        "Created uniform buffer for render constants");
}

void OpenGLRenderer::updateUniformBuffer(const RenderPushConstants& constants) {
    if (!uniformBuffer_) {
        createUniformBuffer();
    }
    
    uniformBuffer_->bind();
    uniformBuffer_->update(&constants, 0, sizeof(RenderPushConstants));
    uniformBuffer_->unbind();
    
    // Bind to binding point 0
    bindUniformBuffer();
}

void OpenGLRenderer::bindUniformBuffer() {
    if (uniformBuffer_) {
        glBindBufferBase(GL_UNIFORM_BUFFER, 0, uniformBuffer_->getId());
    }
}

void OpenGLRenderer::beginFrame() {
    auto& profiler = OpenGLProfiler::getInstance();
    profiler.beginFrame();
    profiler.beginRender();
}

void OpenGLRenderer::endFrame() {
    auto& profiler = OpenGLProfiler::getInstance();
    profiler.endRender();
    profiler.endFrame();
}

void OpenGLRenderer::setRenderMode(int mode) {
    renderMode_ = mode;
    
    if (mode == 1) {  // Wireframe
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    } else if (mode == 2) {  // Points
        glPolygonMode(GL_FRONT_AND_BACK, GL_POINT);
    } else {  // Solid
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    }
}

} // namespace OpenGLHIP

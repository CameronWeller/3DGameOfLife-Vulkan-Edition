#include "opengl/memory/OpenGLMemoryManager.h"
#include "opengl/diagnostics/HealthMonitor.h"
#include "../../../../include/Logger.h"
#include <algorithm>
#include <cstring>

namespace OpenGLHIP {

// OpenGLBuffer implementation
OpenGLBuffer::OpenGLBuffer(GLenum target, size_t size, const void* data, GLenum usage)
    : target_(target), size_(size) {
    glGenBuffers(1, &id_);
    if (id_ == 0) {
        throw std::runtime_error("Failed to create OpenGL buffer");
    }
    
    bind();
    glBufferData(target_, size_, data, usage);
    unbind();
    
    // Record creation in health monitor
    HealthMonitor::getInstance().recordResourceCreation("buffer");
}

OpenGLBuffer::~OpenGLBuffer() {
    if (id_ != 0) {
        glDeleteBuffers(1, &id_);
        HealthMonitor::getInstance().recordResourceDeletion("buffer");
    }
}

OpenGLBuffer::OpenGLBuffer(OpenGLBuffer&& other) noexcept
    : id_(other.id_), target_(other.target_), size_(other.size_), mapped_(other.mapped_) {
    other.id_ = 0;
    other.size_ = 0;
    other.mapped_ = false;
}

OpenGLBuffer& OpenGLBuffer::operator=(OpenGLBuffer&& other) noexcept {
    if (this != &other) {
        if (id_ != 0) {
            glDeleteBuffers(1, &id_);
            HealthMonitor::getInstance().recordResourceDeletion("buffer");
        }
        
        id_ = other.id_;
        target_ = other.target_;
        size_ = other.size_;
        mapped_ = other.mapped_;
        
        other.id_ = 0;
        other.size_ = 0;
        other.mapped_ = false;
    }
    return *this;
}

void OpenGLBuffer::bind() const {
    glBindBuffer(target_, id_);
}

void OpenGLBuffer::unbind() const {
    glBindBuffer(target_, 0);
}

void OpenGLBuffer::update(const void* data, size_t offset, size_t size) {
    if (mapped_) {
        throw std::runtime_error("Cannot update buffer while mapped");
    }
    
    bind();
    size_t updateSize = size > 0 ? size : (size_ - offset);
    glBufferSubData(target_, offset, updateSize, data);
    unbind();
}

void* OpenGLBuffer::map(GLenum access) {
    if (mapped_) {
        throw std::runtime_error("Buffer already mapped");
    }
    
    bind();
    void* ptr = glMapBuffer(target_, access);
    if (ptr) {
        mapped_ = true;
    }
    return ptr;
}

void OpenGLBuffer::unmap() {
    if (!mapped_) {
        return;
    }
    
    bind();
    glUnmapBuffer(target_);
    mapped_ = false;
    unbind();
}

// OpenGLTexture implementation
OpenGLTexture::OpenGLTexture(GLenum target, uint32_t width, uint32_t height, uint32_t depth,
                             GLenum internalFormat, GLenum format, GLenum type, const void* data)
    : target_(target), width_(width), height_(height), depth_(depth),
      internalFormat_(internalFormat), format_(format), type_(type) {
    glGenTextures(1, &id_);
    if (id_ == 0) {
        throw std::runtime_error("Failed to create OpenGL texture");
    }
    
    bind();
    
    switch (target_) {
        case GL_TEXTURE_1D:
            glTexImage1D(target_, 0, internalFormat_, width_, 0, format_, type_, data);
            break;
        case GL_TEXTURE_2D:
            glTexImage2D(target_, 0, internalFormat_, width_, height_, 0, format_, type_, data);
            break;
        case GL_TEXTURE_3D:
            glTexImage3D(target_, 0, internalFormat_, width_, height_, depth_, 0, format_, type_, data);
            break;
        default:
            throw std::runtime_error("Unsupported texture target");
    }
    
    // Set default parameters
    glTexParameteri(target_, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(target_, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(target_, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(target_, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    if (target_ == GL_TEXTURE_3D) {
        glTexParameteri(target_, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    }
    
    unbind();
    
    HealthMonitor::getInstance().recordResourceCreation("texture");
}

OpenGLTexture::~OpenGLTexture() {
    if (id_ != 0) {
        glDeleteTextures(1, &id_);
        HealthMonitor::getInstance().recordResourceDeletion("texture");
    }
}

OpenGLTexture::OpenGLTexture(OpenGLTexture&& other) noexcept
    : id_(other.id_), target_(other.target_), width_(other.width_),
      height_(other.height_), depth_(other.depth_), internalFormat_(other.internalFormat_),
      format_(other.format_), type_(other.type_) {
    other.id_ = 0;
}

OpenGLTexture& OpenGLTexture::operator=(OpenGLTexture&& other) noexcept {
    if (this != &other) {
        if (id_ != 0) {
            glDeleteTextures(1, &id_);
            HealthMonitor::getInstance().recordResourceDeletion("texture");
        }
        
        id_ = other.id_;
        target_ = other.target_;
        width_ = other.width_;
        height_ = other.height_;
        depth_ = other.depth_;
        internalFormat_ = other.internalFormat_;
        format_ = other.format_;
        type_ = other.type_;
        
        other.id_ = 0;
    }
    return *this;
}

void OpenGLTexture::bind(GLuint unit) const {
    glActiveTexture(GL_TEXTURE0 + unit);
    glBindTexture(target_, id_);
}

void OpenGLTexture::unbind(GLuint unit) const {
    glActiveTexture(GL_TEXTURE0 + unit);
    glBindTexture(target_, 0);
}

void OpenGLTexture::update(const void* data, uint32_t x, uint32_t y, uint32_t z,
                          uint32_t width, uint32_t height, uint32_t depth) {
    bind();
    uint32_t updateWidth = width > 0 ? width : width_;
    uint32_t updateHeight = height > 0 ? height : height_;
    uint32_t updateDepth = depth > 0 ? depth : depth_;
    
    switch (target_) {
        case GL_TEXTURE_1D:
            glTexSubImage1D(target_, 0, x, updateWidth, format_, type_, data);
            break;
        case GL_TEXTURE_2D:
            glTexSubImage2D(target_, 0, x, y, updateWidth, updateHeight, format_, type_, data);
            break;
        case GL_TEXTURE_3D:
            glTexSubImage3D(target_, 0, x, y, z, updateWidth, updateHeight, updateDepth, format_, type_, data);
            break;
    }
    unbind();
}

void OpenGLTexture::setParameter(GLenum pname, GLint param) {
    bind();
    glTexParameteri(target_, pname, param);
    unbind();
}

// OpenGLVertexArray implementation
OpenGLVertexArray::OpenGLVertexArray() {
    glGenVertexArrays(1, &id_);
    if (id_ == 0) {
        throw std::runtime_error("Failed to create OpenGL vertex array");
    }
    HealthMonitor::getInstance().recordResourceCreation("vertexarray");
}

OpenGLVertexArray::~OpenGLVertexArray() {
    if (id_ != 0) {
        glDeleteVertexArrays(1, &id_);
        HealthMonitor::getInstance().recordResourceDeletion("vertexarray");
    }
}

OpenGLVertexArray::OpenGLVertexArray(OpenGLVertexArray&& other) noexcept : id_(other.id_) {
    other.id_ = 0;
}

OpenGLVertexArray& OpenGLVertexArray::operator=(OpenGLVertexArray&& other) noexcept {
    if (this != &other) {
        if (id_ != 0) {
            glDeleteVertexArrays(1, &id_);
            HealthMonitor::getInstance().recordResourceDeletion("vertexarray");
        }
        id_ = other.id_;
        other.id_ = 0;
    }
    return *this;
}

void OpenGLVertexArray::bind() const {
    glBindVertexArray(id_);
}

void OpenGLVertexArray::unbind() const {
    glBindVertexArray(0);
}

void OpenGLVertexArray::enableAttribute(GLuint index) {
    bind();
    glEnableVertexAttribArray(index);
}

void OpenGLVertexArray::disableAttribute(GLuint index) {
    bind();
    glDisableVertexAttribArray(index);
}

void OpenGLVertexArray::setAttributePointer(GLuint index, GLint size, GLenum type,
                                           GLboolean normalized, GLsizei stride, const void* pointer) {
    bind();
    glVertexAttribPointer(index, size, type, normalized, stride, pointer);
    glEnableVertexAttribArray(index);
}

// OpenGLMemoryManager implementation
void OpenGLMemoryManager::initialize() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (initialized_) {
        return;
    }
    
    bufferCount_ = 0;
    textureCount_ = 0;
    vertexArrayCount_ = 0;
    totalMemoryUsed_ = 0;
    initialized_ = true;
    
    VulkanHIP::Logger::getInstance().log(VulkanHIP::Logger::LogLevel::Info,
        "OpenGL Memory Manager initialized");
}

void OpenGLMemoryManager::shutdown() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (initialized_) {
        VulkanHIP::Logger::getInstance().log(VulkanHIP::Logger::LogLevel::Info,
            "OpenGL Memory Manager shutdown");
        initialized_ = false;
    }
}

OpenGLMemoryManager::~OpenGLMemoryManager() {
    shutdown();
}

std::unique_ptr<OpenGLBuffer> OpenGLMemoryManager::createBuffer(GLenum target, size_t size,
                                                                 const void* data, GLenum usage) {
    std::lock_guard<std::mutex> lock(mutex_);
    try {
        auto buffer = std::make_unique<OpenGLBuffer>(target, size, data, usage);
        recordBufferCreation(size);
        return buffer;
    } catch (const std::exception& e) {
        VulkanHIP::Logger::getInstance().log(VulkanHIP::Logger::LogLevel::Error,
            std::string("Failed to create buffer: ") + e.what());
        return nullptr;
    }
}

std::unique_ptr<OpenGLTexture> OpenGLMemoryManager::createTexture(GLenum target, uint32_t width,
                                                                  uint32_t height, uint32_t depth,
                                                                  GLenum internalFormat, GLenum format,
                                                                  GLenum type, const void* data) {
    std::lock_guard<std::mutex> lock(mutex_);
    try {
        auto texture = std::make_unique<OpenGLTexture>(target, width, height, depth,
                                                       internalFormat, format, type, data);
        size_t texSize = width * height * depth * 4;  // Approximate size (RGBA)
        recordTextureCreation(texSize);
        return texture;
    } catch (const std::exception& e) {
        VulkanHIP::Logger::getInstance().log(VulkanHIP::Logger::LogLevel::Error,
            std::string("Failed to create texture: ") + e.what());
        return nullptr;
    }
}

std::unique_ptr<OpenGLVertexArray> OpenGLMemoryManager::createVertexArray() {
    std::lock_guard<std::mutex> lock(mutex_);
    try {
        auto vao = std::make_unique<OpenGLVertexArray>();
        recordVertexArrayCreation();
        return vao;
    } catch (const std::exception& e) {
        VulkanHIP::Logger::getInstance().log(VulkanHIP::Logger::LogLevel::Error,
            std::string("Failed to create vertex array: ") + e.what());
        return nullptr;
    }
}

size_t OpenGLMemoryManager::getBufferCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return bufferCount_;
}

size_t OpenGLMemoryManager::getTextureCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return textureCount_;
}

size_t OpenGLMemoryManager::getVertexArrayCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return vertexArrayCount_;
}

uint64_t OpenGLMemoryManager::getTotalMemoryUsed() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return totalMemoryUsed_;
}

void OpenGLMemoryManager::recordBufferCreation(size_t size) {
    bufferCount_++;
    totalMemoryUsed_ += size;
}

void OpenGLMemoryManager::recordBufferDeletion(size_t size) {
    if (bufferCount_ > 0) bufferCount_--;
    if (totalMemoryUsed_ >= size) totalMemoryUsed_ -= size;
}

void OpenGLMemoryManager::recordTextureCreation(size_t size) {
    textureCount_++;
    totalMemoryUsed_ += size;
}

void OpenGLMemoryManager::recordTextureDeletion(size_t size) {
    if (textureCount_ > 0) textureCount_--;
    if (totalMemoryUsed_ >= size) totalMemoryUsed_ -= size;
}

void OpenGLMemoryManager::recordVertexArrayCreation() {
    vertexArrayCount_++;
}

void OpenGLMemoryManager::recordVertexArrayDeletion() {
    if (vertexArrayCount_ > 0) vertexArrayCount_--;
}

} // namespace OpenGLHIP

#pragma once

#include <glad/glad.h>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>
#include <cstdint>
#include "diagnostics/HealthMonitor.h"

namespace OpenGLHIP {

// RAII wrapper for OpenGL buffers
class OpenGLBuffer {
public:
    OpenGLBuffer(GLenum target, size_t size, const void* data = nullptr, GLenum usage = GL_DYNAMIC_DRAW);
    ~OpenGLBuffer();
    
    // Move-only semantics
    OpenGLBuffer(OpenGLBuffer&& other) noexcept;
    OpenGLBuffer& operator=(OpenGLBuffer&& other) noexcept;
    OpenGLBuffer(const OpenGLBuffer&) = delete;
    OpenGLBuffer& operator=(const OpenGLBuffer&) = delete;
    
    GLuint getId() const { return id_; }
    GLenum getTarget() const { return target_; }
    size_t getSize() const { return size_; }
    
    void bind() const;
    void unbind() const;
    void update(const void* data, size_t offset = 0, size_t size = 0);
    void* map(GLenum access = GL_WRITE_ONLY);
    void unmap();
    
    bool isValid() const { return id_ != 0; }

private:
    GLuint id_ = 0;
    GLenum target_ = GL_ARRAY_BUFFER;
    size_t size_ = 0;
    bool mapped_ = false;
};

// RAII wrapper for OpenGL textures
class OpenGLTexture {
public:
    OpenGLTexture(GLenum target, uint32_t width, uint32_t height = 1, uint32_t depth = 1,
                  GLenum internalFormat = GL_RGBA8, GLenum format = GL_RGBA, 
                  GLenum type = GL_UNSIGNED_BYTE, const void* data = nullptr);
    ~OpenGLTexture();
    
    // Move-only semantics
    OpenGLTexture(OpenGLTexture&& other) noexcept;
    OpenGLTexture& operator=(OpenGLTexture&& other) noexcept;
    OpenGLTexture(const OpenGLTexture&) = delete;
    OpenGLTexture& operator=(const OpenGLTexture&) = delete;
    
    GLuint getId() const { return id_; }
    GLenum getTarget() const { return target_; }
    uint32_t getWidth() const { return width_; }
    uint32_t getHeight() const { return height_; }
    uint32_t getDepth() const { return depth_; }
    
    void bind(GLuint unit = 0) const;
    void unbind(GLuint unit = 0) const;
    void update(const void* data, uint32_t x = 0, uint32_t y = 0, uint32_t z = 0,
                uint32_t width = 0, uint32_t height = 0, uint32_t depth = 0);
    void setParameter(GLenum pname, GLint param);
    
    bool isValid() const { return id_ != 0; }

private:
    GLuint id_ = 0;
    GLenum target_ = GL_TEXTURE_2D;
    uint32_t width_ = 0;
    uint32_t height_ = 0;
    uint32_t depth_ = 0;
    GLenum internalFormat_ = GL_RGBA8;
    GLenum format_ = GL_RGBA;
    GLenum type_ = GL_UNSIGNED_BYTE;
};

// RAII wrapper for Vertex Array Objects
class OpenGLVertexArray {
public:
    OpenGLVertexArray();
    ~OpenGLVertexArray();
    
    // Move-only semantics
    OpenGLVertexArray(OpenGLVertexArray&& other) noexcept;
    OpenGLVertexArray& operator=(OpenGLVertexArray&& other) noexcept;
    OpenGLVertexArray(const OpenGLVertexArray&) = delete;
    OpenGLVertexArray& operator=(const OpenGLVertexArray&) = delete;
    
    GLuint getId() const { return id_; }
    
    void bind() const;
    void unbind() const;
    void enableAttribute(GLuint index);
    void disableAttribute(GLuint index);
    void setAttributePointer(GLuint index, GLint size, GLenum type, GLboolean normalized,
                            GLsizei stride, const void* pointer);
    
    bool isValid() const { return id_ != 0; }

private:
    GLuint id_ = 0;
};

// Memory manager for tracking resources
class OpenGLMemoryManager {
public:
    static OpenGLMemoryManager& getInstance() {
        static OpenGLMemoryManager instance;
        return instance;
    }

    void initialize();
    void shutdown();
    
    // Factory methods
    std::unique_ptr<OpenGLBuffer> createBuffer(GLenum target, size_t size, const void* data = nullptr,
                                               GLenum usage = GL_DYNAMIC_DRAW);
    std::unique_ptr<OpenGLTexture> createTexture(GLenum target, uint32_t width, uint32_t height = 1,
                                                 uint32_t depth = 1, GLenum internalFormat = GL_RGBA8,
                                                 GLenum format = GL_RGBA, GLenum type = GL_UNSIGNED_BYTE,
                                                 const void* data = nullptr);
    std::unique_ptr<OpenGLVertexArray> createVertexArray();
    
    // Statistics
    size_t getBufferCount() const;
    size_t getTextureCount() const;
    size_t getVertexArrayCount() const;
    uint64_t getTotalMemoryUsed() const;

private:
    OpenGLMemoryManager() = default;
    ~OpenGLMemoryManager();
    OpenGLMemoryManager(const OpenGLMemoryManager&) = delete;
    OpenGLMemoryManager& operator=(const OpenGLMemoryManager&) = delete;

    mutable std::mutex mutex_;
    size_t bufferCount_ = 0;
    size_t textureCount_ = 0;
    size_t vertexArrayCount_ = 0;
    uint64_t totalMemoryUsed_ = 0;
    bool initialized_ = false;
    
    void recordBufferCreation(size_t size);
    void recordBufferDeletion(size_t size);
    void recordTextureCreation(size_t size);
    void recordTextureDeletion(size_t size);
    void recordVertexArrayCreation();
    void recordVertexArrayDeletion();
};

} // namespace OpenGLHIP

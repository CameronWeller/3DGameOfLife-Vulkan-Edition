#include "opengl/rendering/OpenGLVoxelRenderer.h"
#include "../../../../include/Logger.h"
#include <algorithm>
#include <cmath>
#include <cstddef>

namespace OpenGLHIP {

OpenGLVoxelRenderer::OpenGLVoxelRenderer() {
    renderer_ = std::make_unique<OpenGLRenderer>();
    for (auto& plane : frustumPlanes_) {
        plane = glm::vec4(0.0f);
    }
}

OpenGLVoxelRenderer::~OpenGLVoxelRenderer() {
    cleanup();
}

void OpenGLVoxelRenderer::initialize() {
    if (initialized_) {
        return;
    }
    
    renderer_->initialize();
    generateCubeGeometry();
    createBuffers();
    
    initialized_ = true;
    VulkanHIP::Logger::getInstance().log(VulkanHIP::Logger::LogLevel::Info,
        "OpenGL Voxel Renderer initialized");
}

void OpenGLVoxelRenderer::cleanup() {
    destroyVoxelGeometry();
    if (renderer_) {
        renderer_->cleanup();
    }
    initialized_ = false;
}

void OpenGLVoxelRenderer::generateCubeGeometry() {
    // Define cube vertices with normals (same as Vulkan version)
    vertices_ = {
        // Front face
        {{-0.5f, -0.5f,  0.5f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}},
        {{ 0.5f, -0.5f,  0.5f}, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f}},
        {{ 0.5f,  0.5f,  0.5f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},
        {{-0.5f,  0.5f,  0.5f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}},
        
        // Back face
        {{ 0.5f, -0.5f, -0.5f}, {0.0f, 0.0f, -1.0f}, {0.0f, 0.0f}},
        {{-0.5f, -0.5f, -0.5f}, {0.0f, 0.0f, -1.0f}, {1.0f, 0.0f}},
        {{-0.5f,  0.5f, -0.5f}, {0.0f, 0.0f, -1.0f}, {1.0f, 1.0f}},
        {{ 0.5f,  0.5f, -0.5f}, {0.0f, 0.0f, -1.0f}, {0.0f, 1.0f}},
        
        // Left face
        {{-0.5f, -0.5f, -0.5f}, {-1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
        {{-0.5f, -0.5f,  0.5f}, {-1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}},
        {{-0.5f,  0.5f,  0.5f}, {-1.0f, 0.0f, 0.0f}, {1.0f, 1.0f}},
        {{-0.5f,  0.5f, -0.5f}, {-1.0f, 0.0f, 0.0f}, {0.0f, 1.0f}},
        
        // Right face
        {{ 0.5f, -0.5f,  0.5f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
        {{ 0.5f, -0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}},
        {{ 0.5f,  0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}, {1.0f, 1.0f}},
        {{ 0.5f,  0.5f,  0.5f}, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f}},
        
        // Bottom face
        {{-0.5f, -0.5f, -0.5f}, {0.0f, -1.0f, 0.0f}, {0.0f, 0.0f}},
        {{ 0.5f, -0.5f, -0.5f}, {0.0f, -1.0f, 0.0f}, {1.0f, 0.0f}},
        {{ 0.5f, -0.5f,  0.5f}, {0.0f, -1.0f, 0.0f}, {1.0f, 1.0f}},
        {{-0.5f, -0.5f,  0.5f}, {0.0f, -1.0f, 0.0f}, {0.0f, 1.0f}},
        
        // Top face
        {{-0.5f,  0.5f,  0.5f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}},
        {{ 0.5f,  0.5f,  0.5f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}},
        {{ 0.5f,  0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f}},
        {{-0.5f,  0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f}}
    };
    
    // Define cube indices for triangulated faces
    indices_ = {
        0,  1,  2,   2,  3,  0,   // Front face
        4,  5,  6,   6,  7,  4,   // Back face
        8,  9,  10,  10, 11, 8,   // Left face
        12, 13, 14,  14, 15, 12,  // Right face
        16, 17, 18,  18, 19, 16,  // Bottom face
        20, 21, 22,  22, 23, 20   // Top face
    };
}

void OpenGLVoxelRenderer::createBuffers() {
    auto& memoryManager = OpenGLMemoryManager::getInstance();
    
    // Create VAO
    vao_ = memoryManager.createVertexArray();
    if (!vao_) {
        throw std::runtime_error("Failed to create vertex array");
    }
    
    vao_->bind();
    
    // Create vertex buffer
    vertexBuffer_ = memoryManager.createBuffer(GL_ARRAY_BUFFER, 
        vertices_.size() * sizeof(VoxelVertex), vertices_.data(), GL_STATIC_DRAW);
    if (!vertexBuffer_) {
        throw std::runtime_error("Failed to create vertex buffer");
    }
    
    // Create index buffer
    indexBuffer_ = memoryManager.createBuffer(GL_ELEMENT_ARRAY_BUFFER,
        indices_.size() * sizeof(uint32_t), indices_.data(), GL_STATIC_DRAW);
    if (!indexBuffer_) {
        throw std::runtime_error("Failed to create index buffer");
    }
    
    // Set vertex attributes
    vertexBuffer_->bind();
    vao_->setAttributePointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(VoxelVertex),
                              reinterpret_cast<void*>(offsetof(VoxelVertex, position)));
    vao_->setAttributePointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(VoxelVertex),
                              reinterpret_cast<void*>(offsetof(VoxelVertex, normal)));
    vao_->setAttributePointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(VoxelVertex),
                              reinterpret_cast<void*>(offsetof(VoxelVertex, uv)));
    
    // Create instance buffer (will be updated dynamically)
    instanceBuffer_ = memoryManager.createBuffer(GL_ARRAY_BUFFER, 0, nullptr, GL_DYNAMIC_DRAW);
    if (!instanceBuffer_) {
        throw std::runtime_error("Failed to create instance buffer");
    }
    
    // Set instance attributes (stride is size of VoxelInstance)
    instanceBuffer_->bind();
    vao_->setAttributePointer(3, 3, GL_FLOAT, GL_FALSE, sizeof(VoxelInstance),
                              reinterpret_cast<void*>(offsetof(VoxelInstance, position)));
    vao_->enableAttribute(3);
    vao_->setAttributePointer(4, 4, GL_FLOAT, GL_FALSE, sizeof(VoxelInstance),
                              reinterpret_cast<void*>(offsetof(VoxelInstance, color)));
    vao_->enableAttribute(4);
    vao_->setAttributePointer(5, 1, GL_FLOAT, GL_FALSE, sizeof(VoxelInstance),
                              reinterpret_cast<void*>(offsetof(VoxelInstance, age)));
    vao_->enableAttribute(5);
    vao_->setAttributePointer(6, 1, GL_FLOAT, GL_FALSE, sizeof(VoxelInstance),
                              reinterpret_cast<void*>(offsetof(VoxelInstance, lod)));
    vao_->enableAttribute(6);
    
    // Set instance divisor
    glVertexAttribDivisor(3, 1);
    glVertexAttribDivisor(4, 1);
    glVertexAttribDivisor(5, 1);
    glVertexAttribDivisor(6, 1);
    
    vao_->unbind();
    vertexBuffer_->unbind();
    instanceBuffer_->unbind();
}

void OpenGLVoxelRenderer::destroyVoxelGeometry() {
    vao_.reset();
    vertexBuffer_.reset();
    indexBuffer_.reset();
    instanceBuffer_.reset();
    instances_.clear();
}

void OpenGLVoxelRenderer::updateInstances(const std::vector<VoxelInstance>& instances) {
    instances_ = instances;
    updateInstanceBuffer();
}

void OpenGLVoxelRenderer::clearInstances() {
    instances_.clear();
    if (instanceBuffer_) {
        instanceBuffer_->bind();
        instanceBuffer_->update(nullptr, 0, 0);
        instanceBuffer_->unbind();
    }
}

void OpenGLVoxelRenderer::updateInstanceBuffer() {
    if (!instanceBuffer_ || instances_.empty()) {
        return;
    }
    
    instanceBuffer_->bind();
    size_t bufferSize = instances_.size() * sizeof(VoxelInstance);
    
    // Reallocate if needed
    if (bufferSize > instanceBuffer_->getSize()) {
        // Buffer needs to be recreated with new size
        auto& memoryManager = OpenGLMemoryManager::getInstance();
        instanceBuffer_ = memoryManager.createBuffer(GL_ARRAY_BUFFER, bufferSize,
                                                     instances_.data(), GL_DYNAMIC_DRAW);
        if (!instanceBuffer_) {
            throw std::runtime_error("Failed to recreate instance buffer");
        }
        
        // Re-setup instance attributes
        if (vao_) {
            vao_->bind();
            instanceBuffer_->bind();
            vao_->setAttributePointer(3, 3, GL_FLOAT, GL_FALSE, sizeof(VoxelInstance),
                                      reinterpret_cast<void*>(offsetof(VoxelInstance, position)));
            vao_->setAttributePointer(4, 4, GL_FLOAT, GL_FALSE, sizeof(VoxelInstance),
                                      reinterpret_cast<void*>(offsetof(VoxelInstance, color)));
            vao_->setAttributePointer(5, 1, GL_FLOAT, GL_FALSE, sizeof(VoxelInstance),
                                      reinterpret_cast<void*>(offsetof(VoxelInstance, age)));
            vao_->setAttributePointer(6, 1, GL_FLOAT, GL_FALSE, sizeof(VoxelInstance),
                                      reinterpret_cast<void*>(offsetof(VoxelInstance, lod)));
            glVertexAttribDivisor(3, 1);
            glVertexAttribDivisor(4, 1);
            glVertexAttribDivisor(5, 1);
            glVertexAttribDivisor(6, 1);
            vao_->unbind();
        }
    } else {
        instanceBuffer_->update(instances_.data(), 0, bufferSize);
    }
    
    instanceBuffer_->unbind();
}

void OpenGLVoxelRenderer::setLODDistances(float minDist, float maxDist) {
    minLODDistance_ = minDist;
    maxLODDistance_ = maxDist;
}

void OpenGLVoxelRenderer::setFrustumPlanes(const glm::vec4 planes[6]) {
    for (int i = 0; i < 6; ++i) {
        frustumPlanes_[i] = planes[i];
    }
}

bool OpenGLVoxelRenderer::isInstanceVisible(const VoxelInstance& instance, const glm::vec3& cameraPos) const {
    float distance = glm::length(instance.position - cameraPos);
    float cullingSize = voxelSize_ * (1.0f + instance.lod) * 0.866f;
    
    for (int i = 0; i < 6; ++i) {
        if (glm::dot(glm::vec4(instance.position, 1.0f), frustumPlanes_[i]) + cullingSize < 0.0f) {
            return false;
        }
    }
    return true;
}

void OpenGLVoxelRenderer::render(const glm::mat4& viewMatrix, const glm::mat4& projMatrix,
                                 const glm::vec3& cameraPos, float voxelSize,
                                 int renderMode, float time) {
    if (!initialized_ || !renderer_ || !renderer_->isInitialized() ||
        !vao_ || instances_.empty()) {
        return;
    }
    
    renderer_->beginFrame();
    
    // Set render mode
    renderer_->setRenderMode(renderMode);
    
    // Update uniform buffer with render constants
    RenderPushConstants pushConstants;
    pushConstants.viewProj = projMatrix * viewMatrix;
    pushConstants.cameraPos = cameraPos;
    pushConstants.voxelSize = voxelSize > 0.0f ? voxelSize : voxelSize_;
    pushConstants.minLODDistance = minLODDistance_;
    pushConstants.maxLODDistance = maxLODDistance_;
    pushConstants.time = time;
    pushConstants.renderMode = renderMode;
    for (int i = 0; i < 6; ++i) {
        pushConstants.frustumPlanes[i] = frustumPlanes_[i];
    }
    
    renderer_->updateUniformBuffer(pushConstants);
    
    // Bind program
    glUseProgram(renderer_->getProgram());
    
    // Bind VAO
    vao_->bind();
    
    // Perform frustum culling (optional - can be done on CPU before rendering)
    size_t visibleCount = 0;
    for (const auto& instance : instances_) {
        if (isInstanceVisible(instance, cameraPos)) {
            visibleCount++;
        }
    }
    
    // Render using instanced drawing
    if (!indices_.empty() && visibleCount > 0) {
        indexBuffer_->bind();
        glDrawElementsInstanced(GL_TRIANGLES, static_cast<GLsizei>(indices_.size()),
                                GL_UNSIGNED_INT, nullptr, static_cast<GLsizei>(instances_.size()));
        
        // Record draw call for profiling
        auto& profiler = OpenGLProfiler::getInstance();
        profiler.recordDrawCall(static_cast<uint32_t>(indices_.size() * instances_.size()));
    }
    
    vao_->unbind();
    glUseProgram(0);
    
    renderer_->endFrame();
}

} // namespace OpenGLHIP

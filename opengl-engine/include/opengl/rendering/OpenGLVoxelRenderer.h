#pragma once

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <vector>
#include <memory>
#include "rendering/OpenGLRenderer.h"
#include "memory/OpenGLMemoryManager.h"
#include "../../../../include/Camera.h"

namespace OpenGLHIP {

struct VoxelInstance {
    glm::vec3 position;
    glm::vec4 color;
    float age;
    float lod;
};

struct VoxelVertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 uv;
};

class OpenGLVoxelRenderer {
public:
    OpenGLVoxelRenderer();
    ~OpenGLVoxelRenderer();

    void initialize();
    void cleanup();
    
    // Geometry setup
    void createVoxelGeometry();
    void destroyVoxelGeometry();
    
    // Instance management
    void updateInstances(const std::vector<VoxelInstance>& instances);
    void clearInstances();
    size_t getInstanceCount() const { return instances_.size(); }
    
    // Rendering
    void render(const glm::mat4& viewMatrix, const glm::mat4& projMatrix,
                const glm::vec3& cameraPos, float voxelSize = 1.0f,
                int renderMode = 0, float time = 0.0f);
    
    // Settings
    void setVoxelSize(float size) { voxelSize_ = size; }
    void setLODDistances(float minDist, float maxDist);
    void setFrustumPlanes(const glm::vec4 planes[6]);
    
    // Getters
    bool isInitialized() const { return initialized_; }
    const std::unique_ptr<OpenGLRenderer>& getRenderer() const { return renderer_; }

private:
    std::unique_ptr<OpenGLRenderer> renderer_;
    
    // Geometry
    std::unique_ptr<OpenGLVertexArray> vao_;
    std::unique_ptr<OpenGLBuffer> vertexBuffer_;
    std::unique_ptr<OpenGLBuffer> indexBuffer_;
    std::unique_ptr<OpenGLBuffer> instanceBuffer_;
    
    std::vector<VoxelVertex> vertices_;
    std::vector<uint32_t> indices_;
    std::vector<VoxelInstance> instances_;
    
    // Rendering state
    bool initialized_ = false;
    float voxelSize_ = 1.0f;
    float minLODDistance_ = 10.0f;
    float maxLODDistance_ = 100.0f;
    glm::vec4 frustumPlanes_[6];
    
    // Geometry generation
    void generateCubeGeometry();
    void createBuffers();
    void updateInstanceBuffer();
    
    // Frustum culling
    bool isInstanceVisible(const VoxelInstance& instance, const glm::vec3& cameraPos) const;
};

} // namespace OpenGLHIP

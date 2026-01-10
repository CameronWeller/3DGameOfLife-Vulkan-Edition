#include <gtest/gtest.h>
#include "opengl/rendering/OpenGLRenderer.h"
#include "opengl/rendering/OpenGLVoxelRenderer.h"
#include "opengl/Grid3D.h"
#include "opengl/OpenGLContext.h"
#include "opengl/OpenGLWindowManager.h"
#include "../../../include/Logger.h"
#include "../../../include/Camera.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

using namespace OpenGLHIP;
using namespace VulkanHIP;

class RenderingPipelineIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        OpenGLWindowManager::WindowConfig windowConfig;
        windowConfig.width = 800;
        windowConfig.height = 600;
        windowConfig.title = "Rendering Integration Test";
        windowConfig.visible = false;
        
        windowManager_.init(windowConfig);
        
        OpenGLContextConfig contextConfig;
        contextConfig.majorVersion = 4;
        contextConfig.minorVersion = 3;
        
        context_.initialize(windowManager_.getWindow(), contextConfig);
        
        renderer_ = std::make_unique<OpenGLRenderer>();
        renderer_->initialize();
        
        voxelRenderer_ = std::make_unique<OpenGLVoxelRenderer>();
        voxelRenderer_->initialize();
        
        grid_ = std::make_unique<Grid3D>(16, 16, 16);
    }
    
    void TearDown() override {
        voxelRenderer_.reset();
        renderer_.reset();
        grid_.reset();
        context_.cleanup();
        windowManager_.cleanup();
    }
    
    OpenGLWindowManager& windowManager_ = OpenGLWindowManager::getInstance();
    OpenGLContext& context_ = OpenGLContext::getInstance();
    std::unique_ptr<OpenGLRenderer> renderer_;
    std::unique_ptr<OpenGLVoxelRenderer> voxelRenderer_;
    std::unique_ptr<Grid3D> grid_;
};

TEST_F(RenderingPipelineIntegrationTest, GridToRendererIntegration) {
    // Set some cells in grid
    grid_->setCell(8, 8, 8, true);
    grid_->setCell(7, 8, 8, true);
    grid_->setCell(9, 8, 8, true);
    
    // Update voxel renderer with grid state
    ASSERT_NO_THROW(grid_->updateVoxelRenderer(*voxelRenderer_));
    
    EXPECT_GT(voxelRenderer_->getInstanceCount(), 0);
}

TEST_F(RenderingPipelineIntegrationTest, FullRenderPipeline) {
    // Load simple shaders
    const char* vertexShader = R"(
#version 430
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inInstancePosition;
layout(location = 3) in float inLOD;
layout(location = 4) in vec4 inColor;
layout(location = 5) in float inAge;

out vec3 outNormal;
out vec3 outWorldPos;
out float outLOD;
out vec4 outColor;
out float outAge;

layout(std140, binding = 0) uniform PushConstants {
    mat4 viewProj;
    vec3 cameraPos;
    float voxelSize;
    vec4 frustumPlanes[6];
    float minLODDistance;
    float maxLODDistance;
    float time;
    int renderMode;
} pushConstants;

void main() {
    vec3 worldPos = inInstancePosition + inPosition * pushConstants.voxelSize;
    gl_Position = pushConstants.viewProj * vec4(worldPos, 1.0);
    outNormal = inNormal;
    outWorldPos = worldPos;
    outLOD = inLOD;
    outColor = inColor;
    outAge = inAge;
}
)";
    
    const char* fragmentShader = R"(
#version 430

in vec3 outNormal;
in vec3 outWorldPos;
in float outLOD;
in vec4 outColor;
in float outAge;

out vec4 fragColor;

void main() {
    fragColor = outColor;
}
)";
    
    renderer_->compileShader(vertexShader, fragmentShader);
    ASSERT_TRUE(renderer_->linkProgram());
    
    // Update grid and renderer
    grid_->updateVoxelRenderer(*voxelRenderer_);
    
    // Create test matrices
    glm::mat4 view = glm::lookAt(glm::vec3(10.0f), glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    glm::mat4 proj = glm::perspective(glm::radians(45.0f), 800.0f / 600.0f, 0.1f, 100.0f);
    
    // Render
    ASSERT_NO_THROW(voxelRenderer_->render(view, proj, glm::vec3(10.0f), 1.0f, 0, 0.0f));
}

#include <gtest/gtest.h>
#include "opengl/rendering/OpenGLRenderer.h"
#include "opengl/OpenGLContext.h"
#include "opengl/OpenGLWindowManager.h"
#include "../../../include/Logger.h"
#include <glm/glm.hpp>

using namespace OpenGLHIP;
using namespace VulkanHIP;

class RendererUnitTest : public ::testing::Test {
protected:
    void SetUp() override {
        OpenGLWindowManager::WindowConfig windowConfig;
        windowConfig.width = 800;
        windowConfig.height = 600;
        windowConfig.title = "Renderer Test";
        windowConfig.visible = false;
        
        windowManager_.init(windowConfig);
        
        OpenGLContextConfig contextConfig;
        contextConfig.majorVersion = 4;
        contextConfig.minorVersion = 3;
        
        context_.initialize(windowManager_.getWindow(), contextConfig);
        
        renderer_ = std::make_unique<OpenGLRenderer>();
        renderer_->initialize();
    }
    
    void TearDown() override {
        renderer_.reset();
        context_.cleanup();
        windowManager_.cleanup();
    }
    
    OpenGLWindowManager& windowManager_ = OpenGLWindowManager::getInstance();
    OpenGLContext& context_ = OpenGLContext::getInstance();
    std::unique_ptr<OpenGLRenderer> renderer_;
};

TEST_F(RendererUnitTest, RendererInitialization) {
    EXPECT_TRUE(renderer_->isInitialized());
}

TEST_F(RendererUnitTest, ShaderCompilation) {
    const char* vertexShader = R"(
#version 430
layout(location = 0) in vec3 inPosition;
void main() {
    gl_Position = vec4(inPosition, 1.0);
}
)";
    
    const char* fragmentShader = R"(
#version 430
out vec4 fragColor;
void main() {
    fragColor = vec4(1.0, 1.0, 1.0, 1.0);
}
)";
    
    EXPECT_TRUE(renderer_->compileShader(vertexShader, fragmentShader));
    EXPECT_TRUE(renderer_->linkProgram());
    EXPECT_NE(renderer_->getProgram(), 0);
}

TEST_F(RendererUnitTest, Viewport) {
    renderer_->setViewport(100, 200, 600, 400);
    
    GLint viewport[4];
    glGetIntegerv(GL_VIEWPORT, viewport);
    EXPECT_EQ(viewport[0], 100);
    EXPECT_EQ(viewport[1], 200);
    EXPECT_EQ(viewport[2], 600);
    EXPECT_EQ(viewport[3], 400);
}

TEST_F(RendererUnitTest, ClearOperations) {
    renderer_->clear(0.5f, 0.6f, 0.7f, 1.0f);
    
    GLenum error = glGetError();
    EXPECT_EQ(error, GL_NO_ERROR);
}

TEST_F(RendererUnitTest, UniformBuffer) {
    const char* vertexShader = R"(
#version 430
layout(location = 0) in vec3 inPosition;
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
    gl_Position = pushConstants.viewProj * vec4(inPosition, 1.0);
}
)";
    
    const char* fragmentShader = R"(
#version 430
out vec4 fragColor;
void main() {
    fragColor = vec4(1.0);
}
)";
    
    renderer_->compileShader(vertexShader, fragmentShader);
    renderer_->linkProgram();
    
    RenderPushConstants constants{};
    constants.viewProj = glm::mat4(1.0f);
    constants.cameraPos = glm::vec3(0.0f);
    constants.voxelSize = 1.0f;
    constants.time = 0.0f;
    constants.renderMode = 0;
    
    ASSERT_NO_THROW(renderer_->updateUniformBuffer(constants));
}

TEST_F(RendererUnitTest, RenderMode) {
    renderer_->setRenderMode(0);  // Solid
    renderer_->setRenderMode(1);  // Wireframe
    renderer_->setRenderMode(2);  // Points
    renderer_->setRenderMode(0);  // Back to solid
}

#include <gtest/gtest.h>
#include "opengl/rendering/OpenGLRenderer.h"
#include "opengl/OpenGLContext.h"
#include "opengl/OpenGLWindowManager.h"
#include "../../../include/Logger.h"

using namespace OpenGLHIP;
using namespace VulkanHIP;

class BasicRenderingSmokeTest : public ::testing::Test {
protected:
    void SetUp() override {
        OpenGLWindowManager::WindowConfig windowConfig;
        windowConfig.width = 800;
        windowConfig.height = 600;
        windowConfig.title = "Rendering Test";
        windowConfig.visible = false;
        
        windowManager_.init(windowConfig);
        
        OpenGLContextConfig contextConfig;
        contextConfig.majorVersion = 4;
        contextConfig.minorVersion = 3;
        contextConfig.debugContext = true;
        
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

TEST_F(BasicRenderingSmokeTest, RendererInitialization) {
    EXPECT_TRUE(renderer_->isInitialized());
}

TEST_F(BasicRenderingSmokeTest, ClearScreen) {
    renderer_->setViewport(0, 0, 800, 600);
    renderer_->clear(0.2f, 0.3f, 0.4f, 1.0f);
    
    // Check that clear succeeded (no errors)
    GLenum error = glGetError();
    EXPECT_EQ(error, GL_NO_ERROR);
}

TEST_F(BasicRenderingSmokeTest, SimpleShaderCompile) {
    // Simple vertex shader
    const char* vertexShader = R"(
#version 430

layout(location = 0) in vec3 inPosition;

out vec3 fragPosition;

void main() {
    fragPosition = inPosition;
    gl_Position = vec4(inPosition, 1.0);
}
)";
    
    // Simple fragment shader
    const char* fragmentShader = R"(
#version 430

in vec3 fragPosition;
out vec4 fragColor;

void main() {
    fragColor = vec4(fragPosition * 0.5 + 0.5, 1.0);
}
)";
    
    EXPECT_TRUE(renderer_->compileShader(vertexShader, fragmentShader));
    EXPECT_TRUE(renderer_->linkProgram());
}

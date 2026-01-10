#include <gtest/gtest.h>
#include "opengl/OpenGLContext.h"
#include "opengl/OpenGLWindowManager.h"
#include "../../../include/Logger.h"
#include <GLFW/glfw3.h>

using namespace OpenGLHIP;
using namespace VulkanHIP;

class OpenGLContextSmokeTest : public ::testing::Test {
protected:
    void SetUp() override {
        Logger::getInstance().log(LogLevel::Info, "Setting up OpenGL context smoke test");
        
        // Initialize window manager
        OpenGLWindowManager::WindowConfig config;
        config.width = 800;
        config.height = 600;
        config.title = "OpenGL Context Test";
        config.visible = false;  // Don't show window during tests
        
        windowManager_.init(config);
    }
    
    void TearDown() override {
        if (context_.isInitialized()) {
            context_.cleanup();
        }
        windowManager_.cleanup();
    }
    
    OpenGLWindowManager& windowManager_ = OpenGLWindowManager::getInstance();
    OpenGLContext& context_ = OpenGLContext::getInstance();
};

TEST_F(OpenGLContextSmokeTest, ContextCreation) {
    OpenGLContextConfig config;
    config.majorVersion = 4;
    config.minorVersion = 3;
    config.debugContext = true;
    
    ASSERT_NO_THROW(context_.initialize(windowManager_.getWindow(), config));
    EXPECT_TRUE(context_.isInitialized());
}

TEST_F(OpenGLContextSmokeTest, VersionDetection) {
    OpenGLContextConfig config;
    config.majorVersion = 4;
    config.minorVersion = 3;
    
    context_.initialize(windowManager_.getWindow(), config);
    
    auto version = context_.getVersion();
    EXPECT_GT(version.major, 0);
    EXPECT_GE(version.major, 3);  // At least OpenGL 3.3
}

TEST_F(OpenGLContextSmokeTest, ComputeShaderSupport) {
    OpenGLContextConfig config;
    config.majorVersion = 4;
    config.minorVersion = 3;
    
    context_.initialize(windowManager_.getWindow(), config);
    
    if (context_.getVersion().major >= 4 && context_.getVersion().minor >= 3) {
        EXPECT_TRUE(context_.supportsComputeShaders());
    }
}

TEST_F(OpenGLContextSmokeTest, ErrorChecking) {
    OpenGLContextConfig config;
    config.majorVersion = 4;
    config.minorVersion = 3;
    
    context_.initialize(windowManager_.getWindow(), config);
    
    // Check initial error state
    GLenum error = context_.getLastError();
    EXPECT_EQ(error, GL_NO_ERROR);
    
    // Check error checking works
    context_.checkError("Test operation");
    EXPECT_FALSE(context_.hasError());
}

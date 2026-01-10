#include <gtest/gtest.h>
#include "opengl/OpenGLWindowManager.h"
#include "../../../include/Logger.h"

using namespace OpenGLHIP;
using namespace VulkanHIP;

class WindowManagerSmokeTest : public ::testing::Test {
protected:
    void TearDown() override {
        if (windowManager_.shouldClose() == false) {
            windowManager_.cleanup();
        }
    }
    
    OpenGLWindowManager& windowManager_ = OpenGLWindowManager::getInstance();
};

TEST_F(WindowManagerSmokeTest, WindowCreation) {
    OpenGLWindowManager::WindowConfig config;
    config.width = 640;
    config.height = 480;
    config.title = "Test Window";
    config.visible = false;  // Don't show window during tests
    
    ASSERT_NO_THROW(windowManager_.init(config));
    EXPECT_FALSE(windowManager_.shouldClose());
}

TEST_F(WindowManagerSmokeTest, WindowSize) {
    OpenGLWindowManager::WindowConfig config;
    config.width = 1024;
    config.height = 768;
    config.title = "Size Test Window";
    config.visible = false;
    
    windowManager_.init(config);
    
    int width, height;
    windowManager_.getWindowSize(&width, &height);
    EXPECT_EQ(width, 1024);
    EXPECT_EQ(height, 768);
}

TEST_F(WindowManagerSmokeTest, InputHandling) {
    OpenGLWindowManager::WindowConfig config;
    config.width = 800;
    config.height = 600;
    config.title = "Input Test Window";
    config.visible = false;
    
    windowManager_.init(config);
    
    // Test key query (window not focused, so should return false)
    bool keyPressed = windowManager_.getKey(GLFW_KEY_A);
    EXPECT_FALSE(keyPressed);  // Key should not be pressed when window not focused
    
    // Test mouse button query
    bool mousePressed = windowManager_.getMouseButton(GLFW_MOUSE_BUTTON_LEFT);
    EXPECT_FALSE(mousePressed);
}

TEST_F(WindowManagerSmokeTest, WindowClose) {
    OpenGLWindowManager::WindowConfig config;
    config.width = 800;
    config.height = 600;
    config.title = "Close Test Window";
    config.visible = false;
    
    windowManager_.init(config);
    
    windowManager_.setWindowShouldClose(true);
    EXPECT_TRUE(windowManager_.shouldClose());
}

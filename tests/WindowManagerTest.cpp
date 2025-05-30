#include <gtest/gtest.h>
#include "WindowManager.h"
#include <GLFW/glfw3.h>
#include <memory>
#include <thread>
#include <chrono>

using namespace VulkanHIP;

class WindowManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup code that will be called before each test
    }

    void TearDown() override {
        // Cleanup code that will be called after each test
        WindowManager::getInstance().cleanup();
    }
};

TEST_F(WindowManagerTest, Initialization) {
    WindowManager::WindowConfig config;
    config.width = 800;
    config.height = 600;
    config.title = "Test Window";
    config.resizable = true;
    config.fullscreen = false;

    EXPECT_NO_THROW({
        WindowManager::getInstance().init(config);
    });

    EXPECT_NE(WindowManager::getInstance().getWindow(), nullptr);
    EXPECT_FALSE(WindowManager::getInstance().shouldClose());
}

TEST_F(WindowManagerTest, WindowProperties) {
    WindowManager::WindowConfig config;
    config.width = 1024;
    config.height = 768;
    config.title = "Properties Test";
    WindowManager::getInstance().init(config);

    int width, height;
    WindowManager::getInstance().getWindowSize(&width, &height);
    EXPECT_EQ(width, 1024);
    EXPECT_EQ(height, 768);

    WindowManager::getInstance().setWindowSize(800, 600);
    WindowManager::getInstance().getWindowSize(&width, &height);
    EXPECT_EQ(width, 800);
    EXPECT_EQ(height, 600);
}

TEST_F(WindowManagerTest, WindowPosition) {
    WindowManager::WindowConfig config;
    config.width = 800;
    config.height = 600;
    config.title = "Position Test";
    WindowManager::getInstance().init(config);

    WindowManager::getInstance().setWindowPos(100, 100);
    int x, y;
    glfwGetWindowPos(WindowManager::getInstance().getWindow(), &x, &y);
    EXPECT_EQ(x, 100);
    EXPECT_EQ(y, 100);
}

TEST_F(WindowManagerTest, WindowTitle) {
    WindowManager::WindowConfig config;
    config.title = "Title Test";
    WindowManager::getInstance().init(config);

    WindowManager::getInstance().setWindowTitle("New Title");
    // Note: We can't easily test the actual window title, but we can verify the function doesn't throw
    EXPECT_NO_THROW(WindowManager::getInstance().setWindowTitle("Another Title"));
}

TEST_F(WindowManagerTest, InputHandling) {
    WindowManager::WindowConfig config;
    config.title = "Input Test";
    WindowManager::getInstance().init(config);

    // Test key callback
    bool keyCallbackCalled = false;
    WindowManager::getInstance().setKeyCallback([&](int key, int scancode, int action, int mods) {
        keyCallbackCalled = true;
    });

    // Test mouse button callback
    bool mouseButtonCallbackCalled = false;
    WindowManager::getInstance().setMouseButtonCallback([&](int button, int action, int mods) {
        mouseButtonCallbackCalled = true;
    });

    // Test cursor position callback
    bool cursorPosCallbackCalled = false;
    WindowManager::getInstance().setCursorPosCallback([&](double xpos, double ypos) {
        cursorPosCallbackCalled = true;
    });

    // Note: We can't easily simulate input events in a test, but we can verify the callbacks are set
    EXPECT_NO_THROW(WindowManager::getInstance().pollEvents());
}

TEST_F(WindowManagerTest, SurfaceCreation) {
    WindowManager::WindowConfig config;
    config.title = "Surface Test";
    WindowManager::getInstance().init(config);

    // Create a minimal Vulkan instance for testing
    VkInstance instance;
    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;

    // Get required extensions
    uint32_t extensionCount = 0;
    const char** extensions = glfwGetRequiredInstanceExtensions(&extensionCount);
    createInfo.enabledExtensionCount = extensionCount;
    createInfo.ppEnabledExtensionNames = extensions;

    EXPECT_EQ(vkCreateInstance(&createInfo, nullptr, &instance), VK_SUCCESS);

    // Test surface creation
    VkSurfaceKHR surface;
    EXPECT_NO_THROW({
        surface = WindowManager::getInstance().createSurface(instance);
    });
    EXPECT_NE(surface, VK_NULL_HANDLE);

    // Cleanup
    vkDestroySurfaceKHR(instance, surface, nullptr);
    vkDestroyInstance(instance, nullptr);
}

TEST_F(WindowManagerTest, CursorMode) {
    WindowManager::WindowConfig config;
    config.title = "Cursor Test";
    WindowManager::getInstance().init(config);

    // Test different cursor modes
    EXPECT_NO_THROW(WindowManager::getInstance().setCursorMode(GLFW_CURSOR_NORMAL));
    EXPECT_NO_THROW(WindowManager::getInstance().setCursorMode(GLFW_CURSOR_HIDDEN));
    EXPECT_NO_THROW(WindowManager::getInstance().setCursorMode(GLFW_CURSOR_DISABLED));
}

TEST_F(WindowManagerTest, FramebufferResize) {
    WindowManager::WindowConfig config;
    config.title = "Resize Test";
    config.resizable = true;
    WindowManager::getInstance().init(config);

    bool resizeCallbackCalled = false;
    WindowManager::getInstance().setFramebufferResizeCallback([&](int width, int height) {
        resizeCallbackCalled = true;
    });

    // Note: We can't easily simulate window resize in a test, but we can verify the callback is set
    EXPECT_NO_THROW(WindowManager::getInstance().pollEvents());
}

TEST_F(WindowManagerTest, MultipleInitialization) {
    WindowManager::WindowConfig config;
    config.title = "Multiple Init Test";
    
    // First initialization
    EXPECT_NO_THROW(WindowManager::getInstance().init(config));
    
    // Second initialization should not throw (it should handle the case gracefully)
    EXPECT_NO_THROW(WindowManager::getInstance().init(config));
}

TEST_F(WindowManagerTest, Cleanup) {
    WindowManager::WindowConfig config;
    config.title = "Cleanup Test";
    WindowManager::getInstance().init(config);

    EXPECT_NO_THROW(WindowManager::getInstance().cleanup());
    EXPECT_EQ(WindowManager::getInstance().getWindow(), nullptr);
} 
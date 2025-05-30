#include <gtest/gtest.h>
#include "WindowManager.h"
#include <GLFW/glfw3.h>

class WindowManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        if (!glfwInit()) {
            throw std::runtime_error("Failed to initialize GLFW");
        }
    }

    void TearDown() override {
        glfwTerminate();
    }
};

TEST_F(WindowManagerTest, WindowCreation) {
    WindowManager windowManager;
    EXPECT_TRUE(windowManager.init(800, 600, "Test Window"));
    EXPECT_TRUE(windowManager.getWindow() != nullptr);
    EXPECT_FALSE(windowManager.shouldClose());
}

TEST_F(WindowManagerTest, WindowResize) {
    WindowManager windowManager;
    EXPECT_TRUE(windowManager.init(800, 600, "Test Window"));

    int width, height;
    glfwGetWindowSize(windowManager.getWindow(), &width, &height);
    EXPECT_EQ(width, 800);
    EXPECT_EQ(height, 600);

    glfwSetWindowSize(windowManager.getWindow(), 1024, 768);
    glfwGetWindowSize(windowManager.getWindow(), &width, &height);
    EXPECT_EQ(width, 1024);
    EXPECT_EQ(height, 768);
}

TEST_F(WindowManagerTest, WindowClose) {
    WindowManager windowManager;
    EXPECT_TRUE(windowManager.init(800, 600, "Test Window"));
    EXPECT_FALSE(windowManager.shouldClose());

    glfwSetWindowShouldClose(windowManager.getWindow(), GLFW_TRUE);
    EXPECT_TRUE(windowManager.shouldClose());
}

TEST_F(WindowManagerTest, RequiredExtensions) {
    WindowManager windowManager;
    EXPECT_TRUE(windowManager.init(800, 600, "Test Window"));

    uint32_t extensionCount = 0;
    const char** extensions = glfwGetRequiredInstanceExtensions(&extensionCount);
    EXPECT_GT(extensionCount, 0);
    EXPECT_NE(extensions, nullptr);
}

TEST_F(WindowManagerTest, SurfaceCreation) {
    WindowManager windowManager;
    EXPECT_TRUE(windowManager.init(800, 600, "Test Window"));

    VkInstance instance;
    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.enabledExtensionCount = 0;
    createInfo.ppEnabledExtensionNames = nullptr;

    if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create Vulkan instance");
    }

    VkSurfaceKHR surface;
    EXPECT_EQ(glfwCreateWindowSurface(instance, windowManager.getWindow(), nullptr, &surface), VK_SUCCESS);
    EXPECT_NE(surface, VK_NULL_HANDLE);

    vkDestroySurfaceKHR(instance, surface, nullptr);
    vkDestroyInstance(instance, nullptr);
} 
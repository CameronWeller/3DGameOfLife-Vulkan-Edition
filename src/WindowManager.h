#pragma once

#include <GLFW/glfw3.h>
#include <string>
#include <functional>
#include "ThreadSafe.h"

namespace VulkanHIP {

class WindowManager {
public:
    struct WindowConfig {
        int width = 800;
        int height = 600;
        std::string title = "Vulkan Window";
        bool resizable = true;
        bool fullscreen = false;
    };

    static WindowManager& getInstance() {
        static WindowManager instance;
        return instance;
    }

    WindowManager();
    ~WindowManager();

    void init(const WindowConfig& config);
    void cleanup();

    // Vulkan surface creation
    VkSurfaceKHR createSurface(VkInstance instance) const;
    void destroySurface(VkInstance instance, VkSurfaceKHR surface) const;

    // Helper for VulkanContext compatibility
    VkSurfaceKHR createWindowSurface() const;

    // Window state queries
    bool shouldClose() const;
    bool isMinimized() const;
    void getFramebufferSize(int* width, int* height) const;
    void getWindowSize(int* width, int* height) const;
    void pollEvents() const;
    void waitEvents() const;

    // Window manipulation
    void setWindowTitle(const std::string& title);
    void setWindowSize(int width, int height);
    void setWindowPos(int x, int y);
    void setWindowShouldClose(bool value);
    void setInputMode(int mode, int value);
    void setCursorMode(int mode);

    // Input queries
    bool getKey(int key) const;
    bool getMouseButton(int button) const;
    void getCursorPos(double* xpos, double* ypos) const;
    bool isKeyPressed(int key) const;

    // Callback setters
    void setFramebufferResizeCallback(std::function<void(int, int)> callback) {
        framebufferResizeCallback_.setCallback(callback);
    }

    void setKeyCallback(std::function<void(int, int, int, int)> callback) {
        keyCallback_.setCallback(callback);
    }

    void setMouseButtonCallback(std::function<void(int, int, int)> callback) {
        mouseButtonCallback_.setCallback(callback);
    }

    void setCursorPosCallback(std::function<void(double, double)> callback) {
        cursorPosCallback_.setCallback(callback);
    }

    void setScrollCallback(std::function<void(double, double)> callback) {
        scrollCallback_.setCallback(callback);
    }

    // Window access
    GLFWwindow* getWindow() const { return window_.get(); }

private:
    ThreadSafeValue<WindowConfig> config_;
    ThreadSafeValue<GLFWwindow*> window_;
    ThreadSafeValue<bool> glfwInitialized_{false};

    ThreadSafeCallback<int, int> framebufferResizeCallback_;
    ThreadSafeCallback<int, int, int, int> keyCallback_;
    ThreadSafeCallback<int, int, int> mouseButtonCallback_;
    ThreadSafeCallback<double, double> cursorPosCallback_;
    ThreadSafeCallback<double, double> scrollCallback_;

    static void framebufferResizeCallback(GLFWwindow* window, int width, int height);
    static void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
    static void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
    static void cursorPosCallback(GLFWwindow* window, double xpos, double ypos);
    static void scrollCallback(GLFWwindow* window, double xoffset, double yoffset);

    void setupCallbacks();
};

} // namespace VulkanHIP 
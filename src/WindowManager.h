#pragma once

#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>
#include <string>
#include <functional>
#include <memory>

class WindowManager {
public:
    struct WindowConfig {
        int width = 800;
        int height = 600;
        std::string title = "Vulkan HIP Engine";
        bool resizable = true;
        bool fullscreen = false;
    };

    WindowManager();
    ~WindowManager();

    WindowManager(const WindowManager&) = delete;
    WindowManager& operator=(const WindowManager&) = delete;

    void init(const WindowConfig& config = WindowConfig{});
    void cleanup();

    GLFWwindow* getWindow() const { return window_; }
    VkSurfaceKHR createSurface(VkInstance instance) const;
    void destroySurface(VkInstance instance, VkSurfaceKHR surface) const;

    // Window state queries
    bool shouldClose() const;
    bool isMinimized() const;
    void getFramebufferSize(int* width, int* height) const;
    void getWindowSize(int* width, int* height) const;
    void waitEvents() const;
    void pollEvents() const;

    // Callback setters
    using FramebufferResizeCallback = std::function<void(int, int)>;
    using KeyCallback = std::function<void(int, int, int, int)>;
    using MouseButtonCallback = std::function<void(int, int, int)>;
    using CursorPosCallback = std::function<void(double, double)>;
    using ScrollCallback = std::function<void(double, double)>;

    void setFramebufferResizeCallback(const FramebufferResizeCallback& callback);
    void setKeyCallback(const KeyCallback& callback);
    void setMouseButtonCallback(const MouseButtonCallback& callback);
    void setCursorPosCallback(const CursorPosCallback& callback);
    void setScrollCallback(const ScrollCallback& callback);

private:
    static void framebufferResizeCallback(GLFWwindow* window, int width, int height);
    static void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
    static void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
    static void cursorPosCallback(GLFWwindow* window, double xpos, double ypos);
    static void scrollCallback(GLFWwindow* window, double xoffset, double yoffset);

    GLFWwindow* window_ = nullptr;
    WindowConfig config_;
    bool glfwInitialized_ = false;
    
    // Callback storage
    FramebufferResizeCallback framebufferResizeCallback_;
    KeyCallback keyCallback_;
    MouseButtonCallback mouseButtonCallback_;
    CursorPosCallback cursorPosCallback_;
    ScrollCallback scrollCallback_;
}; 
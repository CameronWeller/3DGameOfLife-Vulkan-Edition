#pragma once

#include <GLFW/glfw3.h>
#include <string>
#include <functional>
#include <mutex>
#include <atomic>
#include <memory>
#include <vulkan/vulkan.h>

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
    VkExtent2D getWindowExtent() const {
        int width, height;
        getFramebufferSize(&width, &height);
        return VkExtent2D{static_cast<uint32_t>(width), static_cast<uint32_t>(height)};
    }
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

    // Callback setters with thread safety
    void setFramebufferResizeCallback(std::function<void(int, int)> callback) {
        std::lock_guard<std::mutex> lock(callbackMutex_);
        framebufferResizeCallback_ = callback;
    }

    void setKeyCallback(std::function<void(int, int, int, int)> callback) {
        std::lock_guard<std::mutex> lock(callbackMutex_);
        keyCallback_ = callback;
    }

    void setMouseButtonCallback(std::function<void(int, int, int)> callback) {
        std::lock_guard<std::mutex> lock(callbackMutex_);
        mouseButtonCallback_ = callback;
    }

    void setCursorPosCallback(std::function<void(double, double)> callback) {
        std::lock_guard<std::mutex> lock(callbackMutex_);
        cursorPosCallback_ = callback;
    }

    void setScrollCallback(std::function<void(double, double)> callback) {
        std::lock_guard<std::mutex> lock(callbackMutex_);
        scrollCallback_ = callback;
    }

    // Window access
    GLFWwindow* getWindow() const { return window_.load(); }

protected:
    WindowManager() = default;
    WindowManager(const WindowManager&) = delete;
    WindowManager& operator=(const WindowManager&) = delete;

private:
    std::atomic<GLFWwindow*> window_{nullptr};
    std::atomic<bool> glfwInitialized_{false};
    WindowConfig config_;
    std::mutex callbackMutex_;
    std::mutex initMutex_;

    // Thread-safe callbacks
    std::function<void(int, int)> framebufferResizeCallback_;
    std::function<void(int, int, int, int)> keyCallback_;
    std::function<void(int, int, int)> mouseButtonCallback_;
    std::function<void(double, double)> cursorPosCallback_;
    std::function<void(double, double)> scrollCallback_;

    // Static callback handlers
    static void framebufferResizeCallback(GLFWwindow* window, int width, int height);
    static void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
    static void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
    static void cursorPosCallback(GLFWwindow* window, double xpos, double ypos);
    static void scrollCallback(GLFWwindow* window, double xoffset, double yoffset);

    void setupCallbacks();
};

} // namespace VulkanHIP 
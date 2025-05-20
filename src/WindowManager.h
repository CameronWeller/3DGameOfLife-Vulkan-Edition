#pragma once

#include <GLFW/glfw3.h>
#include <string>
#include <functional>

class WindowManager {
public:
    WindowManager();
    ~WindowManager();

    bool init(int width, int height, const std::string& title);
    void cleanup();

    GLFWwindow* getWindow() const { return window_; }
    bool shouldClose() const { return glfwWindowShouldClose(window_); }
    void pollEvents() { glfwPollEvents(); }
    void waitEvents() { glfwWaitEvents(); }

    void setFramebufferSizeCallback(std::function<void(int, int)> callback);
    void setKeyCallback(std::function<void(int, int, int, int)> callback);
    void setMouseButtonCallback(std::function<void(int, int, int)> callback);
    void setCursorPosCallback(std::function<void(double, double)> callback);
    void setScrollCallback(std::function<void(double, double)> callback);

    void getFramebufferSize(int* width, int* height) const {
        glfwGetFramebufferSize(window_, width, height);
    }

    void setWindowTitle(const std::string& title) {
        glfwSetWindowTitle(window_, title.c_str());
    }

    void setWindowSize(int width, int height) {
        glfwSetWindowSize(window_, width, height);
    }

    void setWindowPos(int x, int y) {
        glfwSetWindowPos(window_, x, y);
    }

    void setWindowShouldClose(bool value) {
        glfwSetWindowShouldClose(window_, value);
    }

    void setInputMode(int mode, int value) {
        glfwSetInputMode(window_, mode, value);
    }

    bool getKey(int key) const {
        return glfwGetKey(window_, key) == GLFW_PRESS;
    }

    bool getMouseButton(int button) const {
        return glfwGetMouseButton(window_, button) == GLFW_PRESS;
    }

    void getCursorPos(double* xpos, double* ypos) const {
        glfwGetCursorPos(window_, xpos, ypos);
    }

private:
    GLFWwindow* window_ = nullptr;
    std::function<void(int, int)> framebufferSizeCallback_;
    std::function<void(int, int, int, int)> keyCallback_;
    std::function<void(int, int, int)> mouseButtonCallback_;
    std::function<void(double, double)> cursorPosCallback_;
    std::function<void(double, double)> scrollCallback_;

    static void framebufferSizeCallback(GLFWwindow* window, int width, int height);
    static void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
    static void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
    static void cursorPosCallback(GLFWwindow* window, double xpos, double ypos);
    static void scrollCallback(GLFWwindow* window, double xoffset, double yoffset);
}; 
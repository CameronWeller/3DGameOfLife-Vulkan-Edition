#include "WindowManager.h"          // must be FIRST
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <stdexcept>
#include <iostream>


WindowManager::WindowManager() {}

WindowManager::~WindowManager() {
    cleanup();
}

void WindowManager::init(const WindowConfig& config) {
    config_ = config;

    if (!glfwInit()) {
        throw std::runtime_error("Failed to initialize GLFW!");
    }
    glfwInitialized_ = true;

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, config_.resizable ? GLFW_TRUE : GLFW_FALSE);

    window_ = glfwCreateWindow(
        config_.width,
        config_.height,
        config_.title.c_str(),
        config_.fullscreen ? glfwGetPrimaryMonitor() : nullptr,
        nullptr
    );

    if (!window_) {
        glfwTerminate();
        throw std::runtime_error("Failed to create GLFW window!");
    }

    // Set user pointer for callbacks
    glfwSetWindowUserPointer(window_, this);

    // Set up callbacks
    glfwSetFramebufferSizeCallback(window_, framebufferResizeCallback);
    glfwSetKeyCallback(window_, keyCallback);
    glfwSetMouseButtonCallback(window_, mouseButtonCallback);
    glfwSetCursorPosCallback(window_, cursorPosCallback);
    glfwSetScrollCallback(window_, scrollCallback);
}

void WindowManager::cleanup() {
    if (window_) {
        glfwDestroyWindow(window_);
        window_ = nullptr;
    }
    if (glfwInitialized_) {
        glfwTerminate();
        glfwInitialized_ = false;
    }
}

VkSurfaceKHR WindowManager::createSurface(VkInstance instance) const {
    if (!window_) {
        throw std::runtime_error("Cannot create surface: Window is null!");
    }

    VkSurfaceKHR surface;
    if (glfwCreateWindowSurface(instance, window_, nullptr, &surface) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create window surface!");
    }
    return surface;
}

void WindowManager::destroySurface(VkInstance instance, VkSurfaceKHR surface) const {
    if (surface != VK_NULL_HANDLE) {
        vkDestroySurfaceKHR(instance, surface, nullptr);
    }
}

bool WindowManager::shouldClose() const {
    return glfwWindowShouldClose(window_);
}

bool WindowManager::isMinimized() const {
    int width = 0, height = 0;
    glfwGetFramebufferSize(window_, &width, &height);
    return width == 0 || height == 0;
}

void WindowManager::getFramebufferSize(int* width, int* height) const {
    glfwGetFramebufferSize(window_, width, height);
}

void WindowManager::getWindowSize(int* width, int* height) const {
    glfwGetWindowSize(window_, width, height);
}

void WindowManager::waitEvents() const {
    glfwWaitEvents();
}

void WindowManager::pollEvents() const {
    glfwPollEvents();
}

void WindowManager::setFramebufferResizeCallback(const FramebufferResizeCallback& callback) {
    framebufferResizeCallback_ = callback;
}

void WindowManager::setKeyCallback(const KeyCallback& callback) {
    keyCallback_ = callback;
}

void WindowManager::setMouseButtonCallback(const MouseButtonCallback& callback) {
    mouseButtonCallback_ = callback;
}

void WindowManager::setCursorPosCallback(const CursorPosCallback& callback) {
    cursorPosCallback_ = callback;
}

void WindowManager::setScrollCallback(const ScrollCallback& callback) {
    scrollCallback_ = callback;
}

void WindowManager::framebufferResizeCallback(GLFWwindow* window, int width, int height) {
    auto windowManager = reinterpret_cast<WindowManager*>(glfwGetWindowUserPointer(window));
    if (windowManager && windowManager->framebufferResizeCallback_) {
        windowManager->framebufferResizeCallback_(width, height);
    }
}

void WindowManager::keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    auto windowManager = reinterpret_cast<WindowManager*>(glfwGetWindowUserPointer(window));
    if (windowManager && windowManager->keyCallback_) {
        windowManager->keyCallback_(key, scancode, action, mods);
    }
}

void WindowManager::mouseButtonCallback(GLFWwindow* window, int button, int action, int mods) {
    auto windowManager = reinterpret_cast<WindowManager*>(glfwGetWindowUserPointer(window));
    if (windowManager && windowManager->mouseButtonCallback_) {
        windowManager->mouseButtonCallback_(button, action, mods);
    }
}

void WindowManager::cursorPosCallback(GLFWwindow* window, double xpos, double ypos) {
    auto windowManager = reinterpret_cast<WindowManager*>(glfwGetWindowUserPointer(window));
    if (windowManager && windowManager->cursorPosCallback_) {
        windowManager->cursorPosCallback_(xpos, ypos);
    }
}

void WindowManager::scrollCallback(GLFWwindow* window, double xoffset, double yoffset) {
    auto windowManager = reinterpret_cast<WindowManager*>(glfwGetWindowUserPointer(window));
    if (windowManager && windowManager->scrollCallback_) {
        windowManager->scrollCallback_(xoffset, yoffset);
    }
} 
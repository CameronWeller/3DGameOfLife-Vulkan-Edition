#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <stdexcept>
#include "WindowManager.h"
#include "Logger.h"

WindowManager::WindowManager() {}

WindowManager::~WindowManager() {
    cleanup();
}

void WindowManager::init(const WindowConfig& config) {
    config_.set(config);

    if (!glfwInit()) {
        Logger::getInstance().log(LogLevel::ERROR, LogCategory::WINDOW, "Failed to initialize GLFW!");
        throw std::runtime_error("Failed to initialize GLFW!");
    }
    glfwInitialized_.set(true);

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, config.resizable ? GLFW_TRUE : GLFW_FALSE);

    GLFWwindow* newWindow = glfwCreateWindow(
        config.width,
        config.height,
        config.title.c_str(),
        config.fullscreen ? glfwGetPrimaryMonitor() : nullptr,
        nullptr
    );

    if (!newWindow) {
        Logger::getInstance().log(LogLevel::ERROR, LogCategory::WINDOW, "Failed to create GLFW window!");
        glfwTerminate();
        glfwInitialized_.set(false);
        throw std::runtime_error("Failed to create GLFW window!");
    }

    window_.set(newWindow);

    // Set user pointer for callbacks
    glfwSetWindowUserPointer(newWindow, this);

    // Set up callbacks
    glfwSetFramebufferSizeCallback(newWindow, framebufferResizeCallback);
    glfwSetKeyCallback(newWindow, keyCallback);
    glfwSetMouseButtonCallback(newWindow, mouseButtonCallback);
    glfwSetCursorPosCallback(newWindow, cursorPosCallback);
    glfwSetScrollCallback(newWindow, scrollCallback);

    Logger::getInstance().log(LogLevel::INFO, LogCategory::WINDOW, "Window created successfully");
}

void WindowManager::cleanup() {
    if (window_.get()) {
        glfwDestroyWindow(window_.get());
        window_.set(nullptr);
    }
    if (glfwInitialized_.get()) {
        glfwTerminate();
        glfwInitialized_.set(false);
    }
}

VkSurfaceKHR WindowManager::createSurface(VkInstance instance) const {
    GLFWwindow* window = window_.get();
    if (!window) {
        Logger::getInstance().log(LogLevel::ERROR, LogCategory::WINDOW, "Cannot create surface: Window is null!");
        throw std::runtime_error("Cannot create surface: Window is null!");
    }

    VkSurfaceKHR surface;
    if (glfwCreateWindowSurface(instance, window, nullptr, &surface) != VK_SUCCESS) {
        Logger::getInstance().log(LogLevel::ERROR, LogCategory::WINDOW, "Failed to create window surface!");
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
    return glfwWindowShouldClose(window_.get());
}

bool WindowManager::isMinimized() const {
    int width = 0, height = 0;
    glfwGetFramebufferSize(window_.get(), &width, &height);
    return width == 0 || height == 0;
}

void WindowManager::getFramebufferSize(int* width, int* height) const {
    glfwGetFramebufferSize(window_.get(), width, height);
}

void WindowManager::getWindowSize(int* width, int* height) const {
    glfwGetWindowSize(window_.get(), width, height);
}

void WindowManager::pollEvents() const {
    glfwPollEvents();
}

void WindowManager::framebufferResizeCallback(GLFWwindow* window, int width, int height) {
    auto windowManager = reinterpret_cast<WindowManager*>(glfwGetWindowUserPointer(window));
    if (windowManager) {
        windowManager->framebufferResizeCallback_.invoke(width, height);
    }
}

void WindowManager::keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    auto windowManager = reinterpret_cast<WindowManager*>(glfwGetWindowUserPointer(window));
    if (windowManager) {
        windowManager->keyCallback_.invoke(key, scancode, action, mods);
    }
}

void WindowManager::mouseButtonCallback(GLFWwindow* window, int button, int action, int mods) {
    auto windowManager = reinterpret_cast<WindowManager*>(glfwGetWindowUserPointer(window));
    if (windowManager) {
        windowManager->mouseButtonCallback_.invoke(button, action, mods);
    }
}

void WindowManager::cursorPosCallback(GLFWwindow* window, double xpos, double ypos) {
    auto windowManager = reinterpret_cast<WindowManager*>(glfwGetWindowUserPointer(window));
    if (windowManager) {
        windowManager->cursorPosCallback_.invoke(xpos, ypos);
    }
}

void WindowManager::scrollCallback(GLFWwindow* window, double xoffset, double yoffset) {
    auto windowManager = reinterpret_cast<WindowManager*>(glfwGetWindowUserPointer(window));
    if (windowManager) {
        windowManager->scrollCallback_.invoke(xoffset, yoffset);
    }
} 
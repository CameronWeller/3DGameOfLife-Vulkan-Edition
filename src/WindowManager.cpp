#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <stdexcept>
#include "WindowManager.h"
#include "Logger.h"
#include "VulkanContext.h"

namespace VulkanHIP {

static Logger& logger = Logger::getInstance();

WindowManager::WindowManager() {
    if (!glfwInit()) {
        throw std::runtime_error("Failed to initialize GLFW!");
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
}

WindowManager::~WindowManager() {
    cleanup();
}

void WindowManager::init(const WindowConfig& config) {
    config_ = config;

    if (!glfwInit()) {
        logger.log(Logger::LogLevel::Error, "Failed to initialize GLFW!");
        throw std::runtime_error("Failed to initialize GLFW!");
    }
    glfwInitialized_ = true;

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
        logger.log(Logger::LogLevel::Error, "Failed to create GLFW window!");
        glfwTerminate();
        glfwInitialized_ = false;
        throw std::runtime_error("Failed to create GLFW window!");
    }

    window_.store(newWindow);

    // Set user pointer for callbacks
    glfwSetWindowUserPointer(newWindow, this);

    // Set up callbacks
    setupCallbacks();

    logger.log(Logger::LogLevel::Info, "Window created successfully");
}

void WindowManager::cleanup() {
    if (window_.load()) {
        glfwDestroyWindow(window_.load());
        window_.store(nullptr);
    }
    if (glfwInitialized_.load()) {
        glfwTerminate();
        glfwInitialized_.store(false);
    }
}

VkSurfaceKHR WindowManager::createSurface(VkInstance instance) const {
    GLFWwindow* window = window_.load();
    if (!window) {
        logger.log(Logger::LogLevel::Error, "Cannot create surface: Window is null!");
        throw std::runtime_error("Cannot create surface: Window is null!");
    }

    VkSurfaceKHR surface;
    if (glfwCreateWindowSurface(instance, window, nullptr, &surface) != VK_SUCCESS) {
        logger.log(Logger::LogLevel::Error, "Failed to create window surface!");
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
    return glfwWindowShouldClose(window_.load());
}

bool WindowManager::isMinimized() const {
    int width = 0, height = 0;
    glfwGetFramebufferSize(window_.load(), &width, &height);
    return width == 0 || height == 0;
}

void WindowManager::getFramebufferSize(int* width, int* height) const {
    glfwGetFramebufferSize(window_.load(), width, height);
}

void WindowManager::getWindowSize(int* width, int* height) const {
    glfwGetWindowSize(window_.load(), width, height);
}

void WindowManager::pollEvents() const {
    glfwPollEvents();
}

void WindowManager::waitEvents() const {
    glfwWaitEvents();
}

void WindowManager::setWindowTitle(const std::string& title) {
    glfwSetWindowTitle(window_.load(), title.c_str());
}

void WindowManager::setWindowSize(int width, int height) {
    glfwSetWindowSize(window_.load(), width, height);
}

void WindowManager::setWindowPos(int x, int y) {
    glfwSetWindowPos(window_.load(), x, y);
}

void WindowManager::setWindowShouldClose(bool value) {
    glfwSetWindowShouldClose(window_.load(), value);
}

void WindowManager::setInputMode(int mode, int value) {
    glfwSetInputMode(window_.load(), mode, value);
}

bool WindowManager::getKey(int key) const {
    return glfwGetKey(window_.load(), key) == GLFW_PRESS;
}

bool WindowManager::getMouseButton(int button) const {
    return glfwGetMouseButton(window_.load(), button) == GLFW_PRESS;
}

void WindowManager::getCursorPos(double* xpos, double* ypos) const {
    glfwGetCursorPos(window_.load(), xpos, ypos);
}

void WindowManager::framebufferResizeCallback(GLFWwindow* window, int width, int height) {
    auto windowManager = reinterpret_cast<WindowManager*>(glfwGetWindowUserPointer(window));
    if (windowManager) {
        std::lock_guard<std::mutex> lock(windowManager->callbackMutex_);
        if (windowManager->framebufferResizeCallback_) {
            windowManager->framebufferResizeCallback_(width, height);
        }
    }
}

void WindowManager::keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    auto windowManager = reinterpret_cast<WindowManager*>(glfwGetWindowUserPointer(window));
    if (windowManager) {
        std::lock_guard<std::mutex> lock(windowManager->callbackMutex_);
        if (windowManager->keyCallback_) {
            windowManager->keyCallback_(key, scancode, action, mods);
        }
    }
}

void WindowManager::mouseButtonCallback(GLFWwindow* window, int button, int action, int mods) {
    auto windowManager = reinterpret_cast<WindowManager*>(glfwGetWindowUserPointer(window));
    if (windowManager) {
        std::lock_guard<std::mutex> lock(windowManager->callbackMutex_);
        if (windowManager->mouseButtonCallback_) {
            windowManager->mouseButtonCallback_(button, action, mods);
        }
    }
}

void WindowManager::cursorPosCallback(GLFWwindow* window, double xpos, double ypos) {
    auto windowManager = reinterpret_cast<WindowManager*>(glfwGetWindowUserPointer(window));
    if (windowManager) {
        std::lock_guard<std::mutex> lock(windowManager->callbackMutex_);
        if (windowManager->cursorPosCallback_) {
            windowManager->cursorPosCallback_(xpos, ypos);
        }
    }
}

void WindowManager::scrollCallback(GLFWwindow* window, double xoffset, double yoffset) {
    auto windowManager = reinterpret_cast<WindowManager*>(glfwGetWindowUserPointer(window));
    if (windowManager) {
        std::lock_guard<std::mutex> lock(windowManager->callbackMutex_);
        if (windowManager->scrollCallback_) {
            windowManager->scrollCallback_(xoffset, yoffset);
        }
    }
}

void WindowManager::setupCallbacks() {
    GLFWwindow* window = window_.load();
    if (!window) return;

    glfwSetFramebufferSizeCallback(window, framebufferResizeCallback);
    glfwSetKeyCallback(window, keyCallback);
    glfwSetMouseButtonCallback(window, mouseButtonCallback);
    glfwSetCursorPosCallback(window, cursorPosCallback);
    glfwSetScrollCallback(window, scrollCallback);
}

VkSurfaceKHR WindowManager::createWindowSurface() const {
    VkInstance instance = VulkanHIP::VulkanContext::getInstance().getVkInstance();
    return createSurface(instance);
}

bool WindowManager::isKeyPressed(int key) const {
    return glfwGetKey(window_.load(), key) == GLFW_PRESS;
}

void WindowManager::setCursorMode(int mode) {
    glfwSetInputMode(window_.load(), GLFW_CURSOR, mode);
}

} // namespace VulkanHIP 
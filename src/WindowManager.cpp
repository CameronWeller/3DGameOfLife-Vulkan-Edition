#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <stdexcept>
#include "WindowManager.h"
#include "Logger.h"
#include "VulkanContext.h"

namespace VulkanHIP {

static Logger logger;

// getInstance is already defined in the header

WindowManager::WindowManager() : window_(nullptr) {
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
    config_.set(config);

    if (!glfwInit()) {
        logger.log(Logger::LogLevel::Error, "Failed to initialize GLFW!");
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
        logger.log(Logger::LogLevel::Error, "Failed to create GLFW window!");
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

    logger.log(Logger::LogLevel::Info, "Window created successfully");
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

void WindowManager::setWindowTitle(const std::string& title) {
    glfwSetWindowTitle(window_.get(), title.c_str());
}

void WindowManager::setWindowSize(int width, int height) {
    glfwSetWindowSize(window_.get(), width, height);
}

void WindowManager::setWindowPos(int x, int y) {
    glfwSetWindowPos(window_.get(), x, y);
}

void WindowManager::setWindowShouldClose(bool value) {
    glfwSetWindowShouldClose(window_.get(), value);
}

void WindowManager::setInputMode(int mode, int value) {
    glfwSetInputMode(window_.get(), mode, value);
}

bool WindowManager::getKey(int key) const {
    return glfwGetKey(window_.get(), key) == GLFW_PRESS;
}

bool WindowManager::getMouseButton(int button) const {
    return glfwGetMouseButton(window_.get(), button) == GLFW_PRESS;
}

void WindowManager::getCursorPos(double* xpos, double* ypos) const {
    glfwGetCursorPos(window_.get(), xpos, ypos);
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

void WindowManager::setupCallbacks() {
    glfwSetFramebufferSizeCallback(window_, [](GLFWwindow* window, int width, int height) {
        auto windowManager = static_cast<WindowManager*>(glfwGetWindowUserPointer(window));
        if (windowManager->framebufferResizeCallback_) {
            windowManager->framebufferResizeCallback_(width, height);
        }
    });

    glfwSetKeyCallback(window_, [](GLFWwindow* window, int key, int scancode, int action, int mods) {
        auto windowManager = static_cast<WindowManager*>(glfwGetWindowUserPointer(window));
        if (windowManager->keyCallback_) {
            windowManager->keyCallback_(key, scancode, action, mods);
        }
    });

    glfwSetMouseButtonCallback(window_, [](GLFWwindow* window, int button, int action, int mods) {
        auto windowManager = static_cast<WindowManager*>(glfwGetWindowUserPointer(window));
        if (windowManager->mouseButtonCallback_) {
            windowManager->mouseButtonCallback_(button, action, mods);
        }
    });

    glfwSetCursorPosCallback(window_, [](GLFWwindow* window, double xpos, double ypos) {
        auto windowManager = static_cast<WindowManager*>(glfwGetWindowUserPointer(window));
        if (windowManager->cursorPosCallback_) {
            windowManager->cursorPosCallback_(xpos, ypos);
        }
    });

    glfwSetScrollCallback(window_, [](GLFWwindow* window, double xoffset, double yoffset) {
        auto windowManager = static_cast<WindowManager*>(glfwGetWindowUserPointer(window));
        if (windowManager->scrollCallback_) {
            windowManager->scrollCallback_(xoffset, yoffset);
        }
    });
}

void WindowManager::setFramebufferResizeCallback(FramebufferResizeCallback callback) {
    framebufferResizeCallback_ = callback;
}

void WindowManager::setKeyCallback(KeyCallback callback) {
    keyCallback_ = callback;
}

void WindowManager::setMouseButtonCallback(MouseButtonCallback callback) {
    mouseButtonCallback_ = callback;
}

void WindowManager::setCursorPosCallback(CursorPosCallback callback) {
    cursorPosCallback_ = callback;
}

void WindowManager::setScrollCallback(ScrollCallback callback) {
    scrollCallback_ = callback;
}

void WindowManager::waitEvents() {
    glfwWaitEvents();
}

VkSurfaceKHR WindowManager::createWindowSurface() const {
    // Use VulkanContext to get the current instance
    VkInstance instance = VulkanHIP::VulkanContext::getInstance().getVkInstance();
    return createSurface(instance);
}

bool WindowManager::isKeyPressed(int key) const {
    return glfwGetKey(window_, key) == GLFW_PRESS;
}

void WindowManager::setCursorMode(int mode) {
    glfwSetInputMode(window_, GLFW_CURSOR, mode);
}

} // namespace VulkanHIP 
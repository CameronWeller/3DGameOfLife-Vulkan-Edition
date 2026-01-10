#include "opengl/OpenGLWindowManager.h"
#include <stdexcept>

namespace OpenGLHIP {

static VulkanHIP::Logger& logger = VulkanHIP::Logger::getInstance();

OpenGLWindowManager::~OpenGLWindowManager() {
    cleanup();
}

void OpenGLWindowManager::init(const WindowConfig& config) {
    std::lock_guard<std::mutex> lock(initMutex_);
    
    if (glfwInitialized_.load()) {
        logger.log(VulkanHIP::Logger::LogLevel::Warning, "GLFW already initialized!");
        return;
    }

    config_ = config;

    if (!glfwInit()) {
        logger.log(VulkanHIP::Logger::LogLevel::Error, "Failed to initialize GLFW!");
        throw std::runtime_error("Failed to initialize GLFW!");
    }
    glfwInitialized_.store(true);

    // Set OpenGL context hints
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GLFW_TRUE);  // Enable debug context for diagnostics
    glfwWindowHint(GLFW_RESIZABLE, config.resizable ? GLFW_TRUE : GLFW_FALSE);
    glfwWindowHint(GLFW_VISIBLE, config.visible ? GLFW_TRUE : GLFW_FALSE);
    glfwWindowHint(GLFW_DECORATED, config.decorated ? GLFW_TRUE : GLFW_FALSE);
    glfwWindowHint(GLFW_FLOATING, config.floating ? GLFW_TRUE : GLFW_FALSE);
    
    if (config.maximized) {
        glfwWindowHint(GLFW_MAXIMIZED, GLFW_TRUE);
    }

    GLFWwindow* newWindow = glfwCreateWindow(
        config.width,
        config.height,
        config.title.c_str(),
        config.fullscreen ? glfwGetPrimaryMonitor() : nullptr,
        nullptr
    );

    if (!newWindow) {
        logger.log(VulkanHIP::Logger::LogLevel::Error, "Failed to create GLFW window!");
        glfwTerminate();
        glfwInitialized_.store(false);
        throw std::runtime_error("Failed to create GLFW window!");
    }

    window_.store(newWindow);

    // Set user pointer for callbacks
    glfwSetWindowUserPointer(newWindow, this);

    // Set up callbacks
    setupCallbacks();

    logger.log(VulkanHIP::Logger::LogLevel::Info, "OpenGL window created successfully");
}

void OpenGLWindowManager::cleanup() {
    std::lock_guard<std::mutex> lock(initMutex_);
    
    if (window_.load()) {
        glfwDestroyWindow(window_.load());
        window_.store(nullptr);
    }
    if (glfwInitialized_.load()) {
        glfwTerminate();
        glfwInitialized_.store(false);
    }
}

bool OpenGLWindowManager::shouldClose() const {
    return glfwWindowShouldClose(window_.load());
}

bool OpenGLWindowManager::isMinimized() const {
    int width = 0, height = 0;
    glfwGetFramebufferSize(window_.load(), &width, &height);
    return width == 0 || height == 0;
}

void OpenGLWindowManager::getFramebufferSize(int* width, int* height) const {
    glfwGetFramebufferSize(window_.load(), width, height);
}

void OpenGLWindowManager::getWindowSize(int* width, int* height) const {
    glfwGetWindowSize(window_.load(), width, height);
}

void OpenGLWindowManager::pollEvents() const {
    glfwPollEvents();
}

void OpenGLWindowManager::waitEvents() const {
    glfwWaitEvents();
}

void OpenGLWindowManager::setWindowTitle(const std::string& title) {
    glfwSetWindowTitle(window_.load(), title.c_str());
}

void OpenGLWindowManager::setWindowSize(int width, int height) {
    glfwSetWindowSize(window_.load(), width, height);
}

void OpenGLWindowManager::setWindowPos(int x, int y) {
    glfwSetWindowPos(window_.load(), x, y);
}

void OpenGLWindowManager::setWindowShouldClose(bool value) {
    glfwSetWindowShouldClose(window_.load(), value);
}

void OpenGLWindowManager::setInputMode(int mode, int value) {
    glfwSetInputMode(window_.load(), mode, value);
}

bool OpenGLWindowManager::getKey(int key) const {
    return glfwGetKey(window_.load(), key) == GLFW_PRESS;
}

bool OpenGLWindowManager::getMouseButton(int button) const {
    return glfwGetMouseButton(window_.load(), button) == GLFW_PRESS;
}

void OpenGLWindowManager::getCursorPos(double* xpos, double* ypos) const {
    glfwGetCursorPos(window_.load(), xpos, ypos);
}

void OpenGLWindowManager::framebufferResizeCallback(GLFWwindow* window, int width, int height) {
    auto windowManager = reinterpret_cast<OpenGLWindowManager*>(glfwGetWindowUserPointer(window));
    if (windowManager) {
        std::lock_guard<std::mutex> lock(windowManager->callbackMutex_);
        if (windowManager->framebufferResizeCallback_) {
            windowManager->framebufferResizeCallback_(width, height);
        }
    }
}

void OpenGLWindowManager::keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    auto windowManager = reinterpret_cast<OpenGLWindowManager*>(glfwGetWindowUserPointer(window));
    if (windowManager) {
        std::lock_guard<std::mutex> lock(windowManager->callbackMutex_);
        if (windowManager->keyCallback_) {
            windowManager->keyCallback_(key, scancode, action, mods);
        }
    }
}

void OpenGLWindowManager::mouseButtonCallback(GLFWwindow* window, int button, int action, int mods) {
    auto windowManager = reinterpret_cast<OpenGLWindowManager*>(glfwGetWindowUserPointer(window));
    if (windowManager) {
        std::lock_guard<std::mutex> lock(windowManager->callbackMutex_);
        if (windowManager->mouseButtonCallback_) {
            windowManager->mouseButtonCallback_(button, action, mods);
        }
    }
}

void OpenGLWindowManager::cursorPosCallback(GLFWwindow* window, double xpos, double ypos) {
    auto windowManager = reinterpret_cast<OpenGLWindowManager*>(glfwGetWindowUserPointer(window));
    if (windowManager) {
        std::lock_guard<std::mutex> lock(windowManager->callbackMutex_);
        if (windowManager->cursorPosCallback_) {
            windowManager->cursorPosCallback_(xpos, ypos);
        }
    }
}

void OpenGLWindowManager::scrollCallback(GLFWwindow* window, double xoffset, double yoffset) {
    auto windowManager = reinterpret_cast<OpenGLWindowManager*>(glfwGetWindowUserPointer(window));
    if (windowManager) {
        std::lock_guard<std::mutex> lock(windowManager->callbackMutex_);
        if (windowManager->scrollCallback_) {
            windowManager->scrollCallback_(xoffset, yoffset);
        }
    }
}

void OpenGLWindowManager::setupCallbacks() {
    GLFWwindow* window = window_.load();
    if (!window) {
        logger.log(VulkanHIP::Logger::LogLevel::Error, "Cannot setup callbacks: Window is null!");
        return;
    }

    try {
        glfwSetFramebufferSizeCallback(window, framebufferResizeCallback);
        glfwSetKeyCallback(window, keyCallback);
        glfwSetMouseButtonCallback(window, mouseButtonCallback);
        glfwSetCursorPosCallback(window, cursorPosCallback);
        glfwSetScrollCallback(window, scrollCallback);
    } catch (const std::exception& e) {
        logger.log(VulkanHIP::Logger::LogLevel::Error, std::string("Failed to setup callbacks: ") + e.what());
        throw;
    }
}

bool OpenGLWindowManager::isKeyPressed(int key) const {
    return glfwGetKey(window_.load(), key) == GLFW_PRESS;
}

void OpenGLWindowManager::setCursorMode(int mode) {
    glfwSetInputMode(window_.load(), GLFW_CURSOR, mode);
}

} // namespace OpenGLHIP

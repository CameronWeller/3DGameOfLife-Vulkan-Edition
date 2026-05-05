#include "opengl/OpenGLContext.h"
#include <glad/glad.h>
#include <sstream>
#include <stdexcept>
#include <algorithm>
#include <cstring>

namespace OpenGLHIP {

void OpenGLContext::initialize(GLFWwindow* window, const OpenGLContextConfig& config) {
    std::lock_guard<std::mutex> lock(contextMutex_);
    
    if (initialized_) {
        VulkanHIP::Logger::getInstance().log(VulkanHIP::Logger::LogLevel::Warning, 
            "OpenGLContext already initialized");
        return;
    }

    window_ = window;
    config_ = config;

    if (!window_) {
        throw std::runtime_error("OpenGLContext::initialize: Window is null");
    }

    // Make context current before GLAD initialization
    glfwMakeContextCurrent(window_);
    
    // Initialize GLAD
    if (!initializeGLAD()) {
        throw std::runtime_error("Failed to initialize GLAD");
    }

    // Detect OpenGL version
    if (!detectVersion()) {
        throw std::runtime_error("Failed to detect OpenGL version");
    }

    // Check if requested version is supported
    if (!isVersionSupported(config_.majorVersion, config_.minorVersion)) {
        std::stringstream ss;
        ss << "Requested OpenGL version " << config_.majorVersion << "." << config_.minorVersion
           << " not supported. Detected: " << version_.major << "." << version_.minor;
        VulkanHIP::Logger::getInstance().log(VulkanHIP::Logger::LogLevel::Warning, ss.str());
        
        // Try to continue with detected version if compute shaders are supported
        if (!version_.supportsComputeShaders) {
            throw std::runtime_error("OpenGL version does not support compute shaders (requires 4.3+)");
        }
    }

    // Setup debug context if available and requested
    if (config_.debugContext && version_.supportsDebugContext) {
        setupDebugContext();
    }

    // Set VSync
    setVSync(vsyncEnabled_);

    // Check required extensions
    checkExtensions(config_.requiredExtensions);

    // Log context information
    logContextInfo();

    // Initialize diagnostics
    OpenGLDebugCallback::getInstance().initialize();
    OpenGLProfiler::getInstance().initialize();
    HealthMonitor::getInstance().initialize();

    initialized_ = true;
    VulkanHIP::Logger::getInstance().log(VulkanHIP::Logger::LogLevel::Info, 
        "OpenGLContext initialized successfully");
}

void OpenGLContext::cleanup() {
    std::lock_guard<std::mutex> lock(contextMutex_);
    
    if (!initialized_) {
        return;
    }

    // Shutdown diagnostics
    HealthMonitor::getInstance().shutdown();
    OpenGLProfiler::getInstance().shutdown();
    OpenGLDebugCallback::getInstance().shutdown();

    // Clear context
    if (window_) {
        glfwMakeContextCurrent(nullptr);
    }

    initialized_ = false;
    window_ = nullptr;
    VulkanHIP::Logger::getInstance().log(VulkanHIP::Logger::LogLevel::Info, 
        "OpenGLContext cleaned up");
}

OpenGLContext::~OpenGLContext() {
    cleanup();
}

bool OpenGLContext::initializeGLAD() {
    // The vcpkg glad package used here loads through the current context.
    if (!gladLoadGL()) {
        VulkanHIP::Logger::getInstance().log(VulkanHIP::Logger::LogLevel::Error,
            "Failed to load OpenGL functions via GLAD");
        return false;
    }

    VulkanHIP::Logger::getInstance().log(VulkanHIP::Logger::LogLevel::Info,
        "GLAD initialized successfully");
    
    return true;
}

bool OpenGLContext::detectVersion() {
    const char* versionStr = reinterpret_cast<const char*>(glGetString(GL_VERSION));
    if (!versionStr) {
        VulkanHIP::Logger::getInstance().log(VulkanHIP::Logger::LogLevel::Error,
            "Failed to get OpenGL version string");
        return false;
    }

    version_.versionString = std::string(versionStr);
    
    // Get version via glGetInteger (most reliable method)
    glGetIntegerv(GL_MAJOR_VERSION, &version_.major);
    GLenum error = glGetError();
    if (error == GL_INVALID_ENUM) {
        // Fallback: Parse version string if glGetIntegerv not available
        std::istringstream iss(version_.versionString);
        std::string token;
        if (iss >> token) {
            if (token == "OpenGL") {
                iss >> token;  // Skip "version" or "ES"
                if (token == "version") {
                    iss >> token;  // Get version number
                }
            }
            // Now token should be "major.minor"
            size_t dotPos = token.find('.');
            if (dotPos != std::string::npos && dotPos < token.length() - 1) {
                try {
                    version_.major = std::stoi(token.substr(0, dotPos));
                    version_.minor = std::stoi(token.substr(dotPos + 1));
                } catch (...) {
                    // Parsing failed, use defaults
                    version_.major = 3;
                    version_.minor = 3;
                }
            }
        }
    } else {
        // glGetIntegerv worked, now get minor version
        glGetIntegerv(GL_MINOR_VERSION, &version_.minor);
    }

    // Check compute shader support (requires OpenGL 4.3+ or extension)
    version_.supportsComputeShaders = (version_.major > 4) || (version_.major == 4 && version_.minor >= 3);
    if (!version_.supportsComputeShaders) {
        // Check for compute shader extension
        const char* extensionsStr = reinterpret_cast<const char*>(glGetString(GL_EXTENSIONS));
        std::string extensions = extensionsStr ? extensionsStr : "";
        if (extensions.find("GL_ARB_compute_shader") != std::string::npos) {
            version_.supportsComputeShaders = true;
        }
    }

    // Check debug context support (requires OpenGL 4.3+ or extension)
    version_.supportsDebugContext = (version_.major > 4) || (version_.major == 4 && version_.minor >= 3);
    if (!version_.supportsDebugContext) {
        const char* extensionsStr = reinterpret_cast<const char*>(glGetString(GL_EXTENSIONS));
        std::string extensions = extensionsStr ? extensionsStr : "";
        if (extensions.find("GL_KHR_debug") != std::string::npos ||
            extensions.find("GL_ARB_debug_output") != std::string::npos) {
            version_.supportsDebugContext = true;
        }
    }

    std::stringstream ss;
    ss << "Detected OpenGL version: " << version_.major << "." << version_.minor
       << " (" << version_.versionString << ")";
    VulkanHIP::Logger::getInstance().log(VulkanHIP::Logger::LogLevel::Info, ss.str());

    return true;
}

bool OpenGLContext::isVersionSupported(int major, int minor) const {
    if (version_.major > major) return true;
    if (version_.major == major && version_.minor >= minor) return true;
    return false;
}

void OpenGLContext::setupDebugContext() {
    GLint flags = 0;
    glGetIntegerv(GL_CONTEXT_FLAGS, &flags);
    
    if (flags & GL_CONTEXT_FLAG_DEBUG_BIT) {
        VulkanHIP::Logger::getInstance().log(VulkanHIP::Logger::LogLevel::Info,
            "OpenGL debug context enabled");
        
        // Enable debug output
        glEnable(GL_DEBUG_OUTPUT);
        glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
        
        // Set up debug callback
        auto& debugCallback = OpenGLDebugCallback::getInstance();
        debugCallback.initialize();
        glDebugMessageCallback(OpenGLDebugCallback::debugCallback, &debugCallback);
        glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, nullptr, GL_TRUE);
        
        VulkanHIP::Logger::getInstance().log(VulkanHIP::Logger::LogLevel::Info,
            "OpenGL debug callback installed");
    } else {
        VulkanHIP::Logger::getInstance().log(VulkanHIP::Logger::LogLevel::Warning,
            "OpenGL debug context requested but not available");
    }
}

void OpenGLContext::checkExtensions(const std::vector<int>& requiredExtensions) {
    if (requiredExtensions.empty()) {
        return;
    }

    const char* rawExtensions = reinterpret_cast<const char*>(glGetString(GL_EXTENSIONS));
    if (!rawExtensions) {
        VulkanHIP::Logger::getInstance().log(VulkanHIP::Logger::LogLevel::Warning,
            "Skipping extension string check; glGetString(GL_EXTENSIONS) is unavailable in this context");
        return;
    }

    std::string extensionsStr = rawExtensions;
    
    for (const auto& extName : requiredExtensions) {
        const char* extCStr = reinterpret_cast<const char*>(extName);
        if (extensionsStr.find(extCStr) == std::string::npos) {
            std::stringstream ss;
            ss << "Required OpenGL extension not available: " << extCStr;
            VulkanHIP::Logger::getInstance().log(VulkanHIP::Logger::LogLevel::Warning, ss.str());
        }
    }
}

void OpenGLContext::logContextInfo() {
    std::stringstream ss;
    ss << "=== OpenGL Context Information ===" << "\n"
       << "Version: " << version_.versionString << "\n"
       << "Major: " << version_.major << ", Minor: " << version_.minor << "\n"
       << "Compute Shaders: " << (version_.supportsComputeShaders ? "Yes" : "No") << "\n"
       << "Debug Context: " << (version_.supportsDebugContext ? "Yes" : "No");
    
    auto vendor = getGLString(GL_VENDOR);
    auto renderer = getGLString(GL_RENDERER);
    auto glslVersion = getGLString(GL_SHADING_LANGUAGE_VERSION);
    
    if (vendor.has_value()) {
        ss << "\nVendor: " << vendor.value();
    }
    if (renderer.has_value()) {
        ss << "\nRenderer: " << renderer.value();
    }
    if (glslVersion.has_value()) {
        ss << "\nGLSL Version: " << glslVersion.value();
    }
    
    VulkanHIP::Logger::getInstance().log(VulkanHIP::Logger::LogLevel::Info, ss.str());
}

std::optional<std::string> OpenGLContext::getGLString(GLenum name) const {
    const GLubyte* str = glGetString(name);
    if (str) {
        return std::string(reinterpret_cast<const char*>(str));
    }
    return std::nullopt;
}

void OpenGLContext::makeCurrent() {
    if (window_) {
        glfwMakeContextCurrent(window_);
    }
}

void OpenGLContext::swapBuffers() {
    if (window_) {
        glfwSwapBuffers(window_);
    }
}

void OpenGLContext::setVSync(bool enabled) {
    vsyncEnabled_ = enabled;
    glfwSwapInterval(enabled ? 1 : 0);
    
    std::stringstream ss;
    ss << "VSync " << (enabled ? "enabled" : "disabled");
    VulkanHIP::Logger::getInstance().log(VulkanHIP::Logger::LogLevel::Info, ss.str());
}

void OpenGLContext::checkError(const std::string& operation) const {
    GLenum error = glGetError();
    if (error != GL_NO_ERROR) {
        std::stringstream ss;
        ss << "OpenGL error in " << operation << ": " << error;
        VulkanHIP::Logger::getInstance().log(VulkanHIP::Logger::LogLevel::Error, ss.str());
    }
}

GLenum OpenGLContext::getLastError() const {
    return glGetError();
}

bool OpenGLContext::hasError() const {
    return glGetError() != GL_NO_ERROR;
}

} // namespace OpenGLHIP

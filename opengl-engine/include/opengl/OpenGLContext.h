#pragma once

#include <GLFW/glfw3.h>
// GLAD must be included before other OpenGL headers
// Note: glad/glad.h will be included via vcpkg
#ifdef __has_include
    #if __has_include(<glad/glad.h>)
        #include <glad/glad.h>
    #else
        // Fallback: assume GLAD is generated inline or use system OpenGL
        #define GLAD_GL_IMPLEMENTATION
        #include <glad/glad.h>
    #endif
#else
    #include <glad/glad.h>
#endif
#include <string>
#include <mutex>
#include <vector>
#include <optional>
#include "../../../include/Logger.h"
#include "diagnostics/OpenGLDebugCallback.h"
#include "diagnostics/OpenGLProfiler.h"
#include "diagnostics/HealthMonitor.h"

namespace OpenGLHIP {

struct OpenGLVersion {
    int major = 0;
    int minor = 0;
    std::string versionString;
    bool supportsComputeShaders = false;
    bool supportsDebugContext = false;
};

struct OpenGLContextConfig {
    int majorVersion = 4;
    int minorVersion = 3;
    bool debugContext = true;
    bool forwardCompatible = false;
    bool coreProfile = true;
    bool requestDebugContext = true;
    std::vector<int> requiredExtensions;
};

class OpenGLContext {
public:
    static OpenGLContext& getInstance() {
        static OpenGLContext instance;
        return instance;
    }

    void initialize(GLFWwindow* window, const OpenGLContextConfig& config = OpenGLContextConfig{});
    void cleanup();
    
    bool isInitialized() const { return initialized_; }
    const OpenGLVersion& getVersion() const { return version_; }
    GLFWwindow* getWindow() const { return window_; }
    
    // Version detection
    bool detectVersion();
    bool isVersionSupported(int major, int minor) const;
    bool supportsComputeShaders() const { return version_.supportsComputeShaders; }
    bool supportsDebugContext() const { return version_.supportsDebugContext; }
    
    // Error checking
    void checkError(const std::string& operation) const;
    GLenum getLastError() const;
    bool hasError() const;
    
    // Context management
    void makeCurrent();
    void swapBuffers();
    void setVSync(bool enabled);
    bool getVSync() const { return vsyncEnabled_; }
    
    // Diagnostic integration
    OpenGLDebugCallback& getDebugCallback() { return OpenGLDebugCallback::getInstance(); }
    OpenGLProfiler& getProfiler() { return OpenGLProfiler::getInstance(); }
    HealthMonitor& getHealthMonitor() { return HealthMonitor::getInstance(); }

private:
    OpenGLContext() = default;
    ~OpenGLContext();
    OpenGLContext(const OpenGLContext&) = delete;
    OpenGLContext& operator=(const OpenGLContext&) = delete;

    bool initializeGLAD();
    bool createContext(const OpenGLContextConfig& config);
    void setupDebugContext();
    void logContextInfo();
    void checkExtensions(const std::vector<int>& requiredExtensions);
    std::optional<std::string> getGLString(GLenum name) const;

    mutable std::mutex contextMutex_;
    GLFWwindow* window_ = nullptr;
    OpenGLVersion version_;
    bool initialized_ = false;
    bool vsyncEnabled_ = true;
    OpenGLContextConfig config_;
};

} // namespace OpenGLHIP

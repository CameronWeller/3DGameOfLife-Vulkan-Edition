#pragma once

#include <glad/glad.h>
#include <string>
#include <functional>
#include <mutex>
#include <vector>
#include <chrono>
#include <fstream>
#include <sstream>
#include <iomanip>
#include "../../../../include/Logger.h"

namespace OpenGLHIP {

enum class GLDebugSeverity {
    Low,
    Medium,
    High,
    Critical,
    Notification
};

struct GLDebugMessage {
    GLenum source;
    GLenum type;
    GLuint id;
    GLDebugSeverity severity;
    std::string message;
    std::chrono::system_clock::time_point timestamp;
    std::vector<std::string> suggestions;
};

class OpenGLDebugCallback {
public:
    static OpenGLDebugCallback& getInstance() {
        static OpenGLDebugCallback instance;
        return instance;
    }

    void initialize();
    void shutdown();
    
    void setLogFile(const std::string& filename);
    void setSeverityFilter(GLDebugSeverity minSeverity);
    
    void enableAutoDiagnostics(bool enable) { autoDiagnostics_ = enable; }
    void addSuggestionProvider(std::function<std::vector<std::string>(const GLDebugMessage&)> provider);
    
    static void APIENTRY debugCallback(
        GLenum source,
        GLenum type,
        GLuint id,
        GLenum severity,
        GLsizei length,
        const GLchar* message,
        const void* userParam);

private:
    OpenGLDebugCallback() = default;
    ~OpenGLDebugCallback();
    OpenGLDebugCallback(const OpenGLDebugCallback&) = delete;
    OpenGLDebugCallback& operator=(const OpenGLDebugCallback&) = delete;

    void logMessage(const GLDebugMessage& msg);
    void analyzeMessage(const GLDebugMessage& msg);
    std::vector<std::string> generateSuggestions(const GLDebugMessage& msg);
    
    static GLDebugSeverity convertSeverity(GLenum glSeverity);
    static std::string severityToString(GLDebugSeverity severity);
    static std::string sourceToString(GLenum source);
    static std::string typeToString(GLenum type);

    std::mutex mutex_;
    std::ofstream logFile_;
    std::string logFileName_ = "logs/opengl_debug.log";
    GLDebugSeverity minSeverity_ = GLDebugSeverity::Low;
    bool autoDiagnostics_ = true;
    bool initialized_ = false;
    std::vector<GLDebugMessage> messageHistory_;
    size_t maxHistorySize_ = 1000;
    std::vector<std::function<std::vector<std::string>(const GLDebugMessage&)>> suggestionProviders_;
    
    // Statistics
    size_t totalMessages_ = 0;
    size_t errorCount_ = 0;
    size_t warningCount_ = 0;
    
    friend void APIENTRY debugCallback(GLenum, GLenum, GLuint, GLenum, GLsizei, const GLchar*, const void*);
};

} // namespace OpenGLHIP

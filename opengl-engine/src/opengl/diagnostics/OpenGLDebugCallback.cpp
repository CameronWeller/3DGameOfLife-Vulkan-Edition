#include "opengl/diagnostics/OpenGLDebugCallback.h"
#include "../../../include/Logger.h"
#include <GLFW/glfw3.h>
#include <filesystem>
#include <algorithm>
#include <stdexcept>
#include <cstring>

namespace OpenGLHIP {

void OpenGLDebugCallback::initialize() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (initialized_) {
        return;
    }

    // Create logs directory if it doesn't exist
    std::filesystem::path logPath(logFileName_);
    std::filesystem::create_directories(logPath.parent_path());

    logFile_.open(logFileName_, std::ios::app);
    if (!logFile_.is_open()) {
        throw std::runtime_error("Failed to open OpenGL debug log file: " + logFileName_);
    }

    // Enable debug output if available
    GLint flags = 0;
    glGetIntegerv(GL_CONTEXT_FLAGS, &flags);
    if (flags & GL_CONTEXT_FLAG_DEBUG_BIT) {
        glEnable(GL_DEBUG_OUTPUT);
        glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
        glDebugMessageCallback(debugCallback, this);
        glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, nullptr, GL_TRUE);
        
        logFile_ << "=== OpenGL Debug Callback Initialized ===" << std::endl;
        VulkanHIP::Logger::getInstance().log(VulkanHIP::Logger::LogLevel::Info, "OpenGL debug callback initialized");
    } else {
        VulkanHIP::Logger::getInstance().log(VulkanHIP::Logger::LogLevel::Warning, "OpenGL debug context not available");
    }

    initialized_ = true;
}

void OpenGLDebugCallback::shutdown() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (initialized_ && logFile_.is_open()) {
        logFile_ << "=== OpenGL Debug Callback Shutdown ===" << std::endl;
        logFile_ << "Total messages: " << totalMessages_ << std::endl;
        logFile_ << "Errors: " << errorCount_ << std::endl;
        logFile_ << "Warnings: " << warningCount_ << std::endl;
        logFile_.close();
    }
    
    initialized_ = false;
}

OpenGLDebugCallback::~OpenGLDebugCallback() {
    shutdown();
}

void OpenGLDebugCallback::setLogFile(const std::string& filename) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (logFile_.is_open()) {
        logFile_.close();
    }
    
    logFileName_ = filename;
    
    if (initialized_) {
        std::filesystem::path logPath(logFileName_);
        std::filesystem::create_directories(logPath.parent_path());
        logFile_.open(logFileName_, std::ios::app);
        if (!logFile_.is_open()) {
            throw std::runtime_error("Failed to open OpenGL debug log file: " + logFileName_);
        }
    }
}

void OpenGLDebugCallback::setSeverityFilter(GLDebugSeverity minSeverity) {
    std::lock_guard<std::mutex> lock(mutex_);
    minSeverity_ = minSeverity;
}

void OpenGLDebugCallback::addSuggestionProvider(
    std::function<std::vector<std::string>(const GLDebugMessage&)> provider) {
    std::lock_guard<std::mutex> lock(mutex_);
    suggestionProviders_.push_back(provider);
}

void OpenGLDebugCallback::logMessage(const GLDebugMessage& msg) {
    if (!logFile_.is_open()) {
        return;
    }

    auto time_t = std::chrono::system_clock::to_time_t(msg.timestamp);
    std::tm tm;
#ifdef _WIN32
    localtime_s(&tm, &time_t);
#else
    localtime_r(&time_t, &tm);
#endif

    logFile_ << std::put_time(&tm, "%Y-%m-%d %H:%M:%S") << " ["
             << severityToString(msg.severity) << "] "
             << "[" << sourceToString(msg.source) << "] "
             << "[" << typeToString(msg.type) << "] "
             << "ID: " << msg.id << " - "
             << msg.message << std::endl;

    if (!msg.suggestions.empty()) {
        logFile_ << "  Suggestions:" << std::endl;
        for (const auto& suggestion : msg.suggestions) {
            logFile_ << "    - " << suggestion << std::endl;
        }
    }

    logFile_.flush();
}

void OpenGLDebugCallback::analyzeMessage(const GLDebugMessage& msg) {
    if (!autoDiagnostics_) {
        return;
    }

    // Update statistics
    totalMessages_++;
    if (msg.severity == GLDebugSeverity::High || msg.severity == GLDebugSeverity::Critical) {
        errorCount_++;
    } else if (msg.severity == GLDebugSeverity::Medium) {
        warningCount_++;
    }

    // Generate suggestions
    auto suggestions = generateSuggestions(msg);
    const_cast<GLDebugMessage&>(msg).suggestions = suggestions;

    // Store in history
    messageHistory_.push_back(msg);
    if (messageHistory_.size() > maxHistorySize_) {
        messageHistory_.erase(messageHistory_.begin());
    }

    // Log to console for critical errors
    if (msg.severity == GLDebugSeverity::Critical || msg.severity == GLDebugSeverity::High) {
        std::string logMsg = "OpenGL Error: " + msg.message;
        if (!suggestions.empty()) {
            logMsg += " Suggestions: ";
            for (const auto& s : suggestions) {
                logMsg += s + "; ";
            }
        }
        VulkanHIP::Logger::getInstance().log(VulkanHIP::Logger::LogLevel::Error, logMsg);
    }
}

std::vector<std::string> OpenGLDebugCallback::generateSuggestions(const GLDebugMessage& msg) {
    std::vector<std::string> suggestions;

    // Call custom suggestion providers
    for (const auto& provider : suggestionProviders_) {
        auto customSuggestions = provider(msg);
        suggestions.insert(suggestions.end(), customSuggestions.begin(), customSuggestions.end());
    }

    // Built-in suggestions based on error type
    if (msg.type == GL_DEBUG_TYPE_ERROR) {
        if (msg.id == GL_INVALID_ENUM) {
            suggestions.push_back("Check that all OpenGL enums are valid for your OpenGL version");
            suggestions.push_back("Verify GLAD initialization completed successfully");
        } else if (msg.id == GL_INVALID_VALUE) {
            suggestions.push_back("Check that all parameter values are within valid ranges");
            suggestions.push_back("Verify buffer sizes and offsets are correct");
        } else if (msg.id == GL_INVALID_OPERATION) {
            suggestions.push_back("Check that required state is set before operation");
            suggestions.push_back("Verify objects are bound and valid");
        } else if (msg.id == GL_OUT_OF_MEMORY) {
            suggestions.push_back("Reduce grid size or optimize memory usage");
            suggestions.push_back("Check for memory leaks in buffer/texture allocations");
        }
    }

    return suggestions;
}

GLDebugSeverity OpenGLDebugCallback::convertSeverity(GLenum glSeverity) {
    switch (glSeverity) {
        case GL_DEBUG_SEVERITY_HIGH:
            return GLDebugSeverity::High;
        case GL_DEBUG_SEVERITY_MEDIUM:
            return GLDebugSeverity::Medium;
        case GL_DEBUG_SEVERITY_LOW:
            return GLDebugSeverity::Low;
        case GL_DEBUG_SEVERITY_NOTIFICATION:
            return GLDebugSeverity::Notification;
        default:
            return GLDebugSeverity::Low;
    }
}

std::string OpenGLDebugCallback::severityToString(GLDebugSeverity severity) {
    switch (severity) {
        case GLDebugSeverity::Low: return "LOW";
        case GLDebugSeverity::Medium: return "MEDIUM";
        case GLDebugSeverity::High: return "HIGH";
        case GLDebugSeverity::Critical: return "CRITICAL";
        case GLDebugSeverity::Notification: return "NOTIFICATION";
        default: return "UNKNOWN";
    }
}

std::string OpenGLDebugCallback::sourceToString(GLenum source) {
    switch (source) {
        case GL_DEBUG_SOURCE_API: return "API";
        case GL_DEBUG_SOURCE_WINDOW_SYSTEM: return "WINDOW_SYSTEM";
        case GL_DEBUG_SOURCE_SHADER_COMPILER: return "SHADER_COMPILER";
        case GL_DEBUG_SOURCE_THIRD_PARTY: return "THIRD_PARTY";
        case GL_DEBUG_SOURCE_APPLICATION: return "APPLICATION";
        case GL_DEBUG_SOURCE_OTHER: return "OTHER";
        default: return "UNKNOWN";
    }
}

std::string OpenGLDebugCallback::typeToString(GLenum type) {
    switch (type) {
        case GL_DEBUG_TYPE_ERROR: return "ERROR";
        case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR: return "DEPRECATED";
        case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR: return "UNDEFINED_BEHAVIOR";
        case GL_DEBUG_TYPE_PORTABILITY: return "PORTABILITY";
        case GL_DEBUG_TYPE_PERFORMANCE: return "PERFORMANCE";
        case GL_DEBUG_TYPE_MARKER: return "MARKER";
        case GL_DEBUG_TYPE_PUSH_GROUP: return "PUSH_GROUP";
        case GL_DEBUG_TYPE_POP_GROUP: return "POP_GROUP";
        case GL_DEBUG_TYPE_OTHER: return "OTHER";
        default: return "UNKNOWN";
    }
}

void OpenGLDebugCallback::APIENTRY debugCallback(
    GLenum source,
    GLenum type,
    GLuint id,
    GLenum severity,
    GLsizei length,
    const GLchar* message,
    const void* userParam) {
    
    auto* callback = const_cast<OpenGLDebugCallback*>(static_cast<const OpenGLDebugCallback*>(userParam));
    if (!callback || !callback->initialized_) {
        return;
    }

    GLDebugMessage msg;
    msg.source = source;
    msg.type = type;
    msg.id = id;
    msg.severity = convertSeverity(severity);
    msg.message = std::string(message, length > 0 ? length : strlen(message));
    msg.timestamp = std::chrono::system_clock::now();

    callback->analyzeMessage(msg);
    callback->logMessage(msg);
}

} // namespace OpenGLHIP

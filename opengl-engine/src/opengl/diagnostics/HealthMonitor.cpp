#include "opengl/diagnostics/HealthMonitor.h"
#include "opengl/diagnostics/OpenGLProfiler.h"
#include <filesystem>
#include <sstream>
#include <algorithm>
#include <fstream>
#include <iomanip>
#include <ctime>

namespace OpenGLHIP {

void HealthMonitor::initialize() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (initialized_) {
        return;
    }

    resourceCounts_ = ResourceCounts{};
    resourceCreationCount_.clear();
    resourceDeletionCount_.clear();
    healthChecks_.clear();
    lastHealthCheck_ = std::chrono::system_clock::now();
    lastRecordedFPS_ = 0.0f;
    
    initialized_ = true;
    VulkanHIP::Logger::getInstance().log(VulkanHIP::Logger::LogLevel::Info, "Health Monitor initialized");
}

void HealthMonitor::shutdown() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (initialized_) {
        performHealthCheck();
        exportHealthReport("logs/opengl_health_final.json");
    }
    
    initialized_ = false;
}

HealthMonitor::~HealthMonitor() {
    shutdown();
}

void HealthMonitor::performHealthCheck() {
    if (!initialized_) return;
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto now = std::chrono::system_clock::now();
    auto timeSinceLastCheck = std::chrono::duration_cast<std::chrono::seconds>(now - lastHealthCheck_);
    
    if (timeSinceLastCheck < healthCheckInterval_) {
        return;  // Don't check too frequently
    }
    
    healthChecks_.clear();
    
    // Update metrics from profiler
    auto& profiler = OpenGLProfiler::getInstance();
    auto lastFrame = profiler.getLastFrameMetrics();
    lastRecordedFPS_ = lastFrame.fps;
    lastRecordedFrameTime_ = lastFrame.frameTime;
    
    // Perform all checks (no locking needed - we already have the lock)
    checkContextValidity();
    checkMemoryUsage();
    checkFrameRate();
    checkResourceLeaks();
    checkShaderErrors();
    checkOpenGLErrors();
    
    lastHealthCheck_ = now;
}

HealthStatus HealthMonitor::getOverallHealth() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (healthChecks_.empty()) {
        return HealthStatus::Unknown;
    }
    
    bool hasCritical = false;
    bool hasWarning = false;
    
    for (const auto& check : healthChecks_) {
        if (check.status == HealthStatus::Critical) {
            hasCritical = true;
            break;
        } else if (check.status == HealthStatus::Warning) {
            hasWarning = true;
        }
    }
    
    if (hasCritical) return HealthStatus::Critical;
    if (hasWarning) return HealthStatus::Warning;
    return HealthStatus::Healthy;
}

std::vector<HealthCheck> HealthMonitor::getHealthChecks() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return healthChecks_;
}

void HealthMonitor::recordResourceCreation(const std::string& type) {
    if (!initialized_) return;
    
    std::lock_guard<std::mutex> lock(mutex_);
    resourceCreationCount_[type]++;
    
    if (type == "buffer") resourceCounts_.buffers++;
    else if (type == "texture") resourceCounts_.textures++;
    else if (type == "shader") resourceCounts_.shaders++;
    else if (type == "program") resourceCounts_.programs++;
    else if (type == "framebuffer") resourceCounts_.framebuffers++;
    else if (type == "vertexarray") resourceCounts_.vertexArrays++;
}

void HealthMonitor::recordResourceDeletion(const std::string& type) {
    if (!initialized_) return;
    
    std::lock_guard<std::mutex> lock(mutex_);
    resourceDeletionCount_[type]++;
    
    if (type == "buffer") { if (resourceCounts_.buffers > 0) resourceCounts_.buffers--; }
    else if (type == "texture") { if (resourceCounts_.textures > 0) resourceCounts_.textures--; }
    else if (type == "shader") { if (resourceCounts_.shaders > 0) resourceCounts_.shaders--; }
    else if (type == "program") { if (resourceCounts_.programs > 0) resourceCounts_.programs--; }
    else if (type == "framebuffer") { if (resourceCounts_.framebuffers > 0) resourceCounts_.framebuffers--; }
    else if (type == "vertexarray") { if (resourceCounts_.vertexArrays > 0) resourceCounts_.vertexArrays--; }
}

ResourceCounts HealthMonitor::getResourceCounts() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return resourceCounts_;
}

void HealthMonitor::checkContextValidity() {
    // Called from performHealthCheck which already has lock
    GLenum error = glGetError();
    if (error != GL_NO_ERROR) {
        std::stringstream msg;
        msg << "OpenGL context error detected: " << error;
        healthChecks_.push_back(createCheck(
            "Context Validity",
            HealthStatus::Critical,
            msg.str(),
            {"Check that OpenGL context is valid and not destroyed", "Verify GLAD initialization completed"}
        ));
    } else {
        healthChecks_.push_back(createCheck(
            "Context Validity",
            HealthStatus::Healthy,
            "OpenGL context is valid"
        ));
    }
}

void HealthMonitor::checkMemoryUsage() {
    // Called from performHealthCheck which already has lock
    // Get GPU memory info if available (requires extension)
    GLint totalMemoryMB = 0;
    GLint availableMemoryMB = 0;
    
    // Try to get memory info (vendor-specific)
    #ifdef GL_NVX_gpu_memory_info
    if (glGetError() == GL_NO_ERROR) {
        glGetIntegerv(GL_GPU_MEMORY_INFO_TOTAL_AVAILABLE_MEMORY_NVX, &totalMemoryMB);
        glGetIntegerv(GL_GPU_MEMORY_INFO_CURRENT_AVAILABLE_VIDMEM_NVX, &availableMemoryMB);
        glGetError();  // Clear any error if extension not available
    }
    #endif
    
    if (totalMemoryMB > 0 && availableMemoryMB > 0) {
        int usedMemoryMB = totalMemoryMB - availableMemoryMB;
        float usagePercent = (static_cast<float>(usedMemoryMB) / static_cast<float>(totalMemoryMB)) * 100.0f;
        
        if (usagePercent > 90.0f) {
            std::stringstream msg;
            msg << "High GPU memory usage: " << usagePercent << "% (" << usedMemoryMB << "MB / " << totalMemoryMB << "MB)";
            healthChecks_.push_back(createCheck(
                "Memory Usage",
                HealthStatus::Critical,
                msg.str(),
                {"Reduce grid size", "Free unused resources", "Optimize memory allocations"}
            ));
        } else if (usagePercent > 75.0f) {
            std::stringstream msg;
            msg << "Moderate GPU memory usage: " << usagePercent << "%";
            healthChecks_.push_back(createCheck(
                "Memory Usage",
                HealthStatus::Warning,
                msg.str()
            ));
        } else {
            std::stringstream msg;
            msg << "GPU memory usage: " << usagePercent << "%";
            healthChecks_.push_back(createCheck(
                "Memory Usage",
                HealthStatus::Healthy,
                msg.str()
            ));
        }
    } else {
        healthChecks_.push_back(createCheck(
            "Memory Usage",
            HealthStatus::Unknown,
            "GPU memory information not available (requires vendor extension)"
        ));
    }
}

void HealthMonitor::checkFrameRate() {
    // Called from performHealthCheck which already has lock
    float frameTimeMs = lastRecordedFrameTime_.count() / 1000.0f;
    
    if (lastRecordedFPS_ < fpsThreshold_) {
        std::stringstream msg;
        msg << "Low FPS: " << lastRecordedFPS_ << " FPS (threshold: " << fpsThreshold_ << " FPS)";
        healthChecks_.push_back(createCheck(
            "Frame Rate",
            HealthStatus::Critical,
            msg.str(),
            {"Reduce grid size", "Disable LOD or reduce quality", "Check for CPU bottlenecks", "Profile GPU usage"}
        ));
    } else if (frameTimeMs > frameTimeThresholdMs_) {
        std::stringstream msg;
        msg << "High frame time: " << frameTimeMs << "ms (threshold: " << frameTimeThresholdMs_ << "ms)";
        healthChecks_.push_back(createCheck(
            "Frame Rate",
            HealthStatus::Warning,
            msg.str()
        ));
    } else {
        std::stringstream msg;
        msg << "FPS: " << lastRecordedFPS_ << ", Frame Time: " << frameTimeMs << "ms";
        healthChecks_.push_back(createCheck(
            "Frame Rate",
            HealthStatus::Healthy,
            msg.str()
        ));
    }
}

void HealthMonitor::checkResourceLeaks() {
    // Called from performHealthCheck which already has lock
    bool hasLeak = false;
    std::stringstream leakMsg;
    std::vector<std::string> suggestions;
    
    for (const auto& [type, created] : resourceCreationCount_) {
        size_t deleted = resourceDeletionCount_[type];
        if (created > deleted) {
            size_t leaked = created - deleted;
            if (!hasLeak) {
                leakMsg << "Resource leaks detected: ";
                hasLeak = true;
            }
            leakMsg << type << ": " << leaked << " leaked, ";
            suggestions.push_back("Ensure " + type + " resources are properly deleted");
        }
    }
    
    if (hasLeak) {
        healthChecks_.push_back(createCheck(
            "Resource Leaks",
            HealthStatus::Warning,
            leakMsg.str(),
            suggestions
        ));
    } else {
        healthChecks_.push_back(createCheck(
            "Resource Leaks",
            HealthStatus::Healthy,
            "No resource leaks detected"
        ));
    }
}

void HealthMonitor::checkShaderErrors() {
    // Called from performHealthCheck which already has lock
    // This would be checked by the debug callback, but we can verify here too
    GLenum error = glGetError();
    if (error != GL_NO_ERROR) {
        std::stringstream msg;
        msg << "OpenGL error after shader operations: " << error;
        healthChecks_.push_back(createCheck(
            "Shader Errors",
            HealthStatus::Warning,
            msg.str(),
            {"Check shader compilation logs", "Verify shader source code", "Check OpenGL debug output"}
        ));
    } else {
        healthChecks_.push_back(createCheck(
            "Shader Errors",
            HealthStatus::Healthy,
            "No shader errors detected"
        ));
    }
}

void HealthMonitor::checkOpenGLErrors() {
    // Called from performHealthCheck which already has lock
    GLenum error = glGetError();
    std::vector<GLenum> errors;
    
    while (error != GL_NO_ERROR) {
        errors.push_back(error);
        error = glGetError();
    }
    
    if (!errors.empty()) {
        std::stringstream msg;
        msg << "OpenGL errors detected: ";
        for (size_t i = 0; i < errors.size(); ++i) {
            msg << errors[i];
            if (i < errors.size() - 1) msg << ", ";
        }
        healthChecks_.push_back(createCheck(
            "OpenGL Errors",
            HealthStatus::Warning,
            msg.str(),
            {"Check OpenGL debug callback output", "Verify all OpenGL calls are valid", "Check for invalid object handles"}
        ));
    } else {
        healthChecks_.push_back(createCheck(
            "OpenGL Errors",
            HealthStatus::Healthy,
            "No OpenGL errors detected"
        ));
    }
}

HealthCheck HealthMonitor::createCheck(const std::string& name, HealthStatus status,
                                       const std::string& message, const std::vector<std::string>& suggestions) {
    HealthCheck check;
    check.name = name;
    check.status = status;
    check.message = message;
    check.suggestions = suggestions;
    check.timestamp = std::chrono::system_clock::now();
    return check;
}

void HealthMonitor::exportHealthReport(const std::string& filename) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    nlohmann::json json;
    auto overallStatus = getOverallHealth();
    switch (overallStatus) {
        case HealthStatus::Healthy: json["overallStatus"] = "Healthy"; break;
        case HealthStatus::Warning: json["overallStatus"] = "Warning"; break;
        case HealthStatus::Critical: json["overallStatus"] = "Critical"; break;
        default: json["overallStatus"] = "Unknown"; break;
    }
    
    json["resourceCounts"] = {
        {"buffers", resourceCounts_.buffers},
        {"textures", resourceCounts_.textures},
        {"shaders", resourceCounts_.shaders},
        {"programs", resourceCounts_.programs},
        {"framebuffers", resourceCounts_.framebuffers},
        {"vertexArrays", resourceCounts_.vertexArrays}
    };
    
    json["healthChecks"] = nlohmann::json::array();
    for (const auto& check : healthChecks_) {
        nlohmann::json checkJson;
        checkJson["name"] = check.name;
        switch (check.status) {
            case HealthStatus::Healthy: checkJson["status"] = "Healthy"; break;
            case HealthStatus::Warning: checkJson["status"] = "Warning"; break;
            case HealthStatus::Critical: checkJson["status"] = "Critical"; break;
            default: checkJson["status"] = "Unknown"; break;
        }
        checkJson["message"] = check.message;
        checkJson["suggestions"] = check.suggestions;
        auto time_t = std::chrono::system_clock::to_time_t(check.timestamp);
        checkJson["timestamp"] = std::to_string(time_t);
        json["healthChecks"].push_back(checkJson);
    }
    
    std::filesystem::create_directories(std::filesystem::path(filename).parent_path());
    std::ofstream file(filename);
    if (file.is_open()) {
        file << json.dump(2);
        file.close();
    }
}

std::string HealthMonitor::generateHealthReport() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::stringstream report;
    report << "=== OpenGL Health Report ===\n";
    report << "Overall Status: ";
    
    auto overallStatus = getOverallHealth();
    switch (overallStatus) {
        case HealthStatus::Healthy: report << "Healthy\n"; break;
        case HealthStatus::Warning: report << "Warning\n"; break;
        case HealthStatus::Critical: report << "Critical\n"; break;
        default: report << "Unknown\n"; break;
    }
    
    report << "\nResource Counts:\n";
    report << "  Buffers: " << resourceCounts_.buffers << "\n";
    report << "  Textures: " << resourceCounts_.textures << "\n";
    report << "  Shaders: " << resourceCounts_.shaders << "\n";
    report << "  Programs: " << resourceCounts_.programs << "\n";
    report << "  Framebuffers: " << resourceCounts_.framebuffers << "\n";
    report << "  Vertex Arrays: " << resourceCounts_.vertexArrays << "\n";
    
    report << "\nHealth Checks:\n";
    for (const auto& check : healthChecks_) {
        std::string statusStr;
        switch (check.status) {
            case HealthStatus::Healthy: statusStr = "OK"; break;
            case HealthStatus::Warning: statusStr = "WARN"; break;
            case HealthStatus::Critical: statusStr = "CRITICAL"; break;
            default: statusStr = "UNKNOWN"; break;
        }
        report << "  " << check.name << " [" << statusStr << "]: " << check.message << "\n";
        if (!check.suggestions.empty()) {
            for (const auto& suggestion : check.suggestions) {
                report << "    - " << suggestion << "\n";
            }
        }
    }
    
    return report.str();
}

} // namespace OpenGLHIP

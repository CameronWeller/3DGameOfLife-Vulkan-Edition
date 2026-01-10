#pragma once

#include <glad/glad.h>
#include <string>
#include <mutex>
#include <vector>
#include <unordered_map>
#include <chrono>
#include <nlohmann/json.hpp>
#include "../../../../include/Logger.h"

namespace OpenGLHIP {

enum class HealthStatus {
    Healthy,
    Warning,
    Critical,
    Unknown
};

struct HealthCheck {
    std::string name;
    HealthStatus status;
    std::string message;
    std::vector<std::string> suggestions;
    std::chrono::system_clock::time_point timestamp;
};

struct ResourceCounts {
    size_t buffers = 0;
    size_t textures = 0;
    size_t shaders = 0;
    size_t programs = 0;
    size_t framebuffers = 0;
    size_t vertexArrays = 0;
};

class HealthMonitor {
public:
    static HealthMonitor& getInstance() {
        static HealthMonitor instance;
        return instance;
    }

    void initialize();
    void shutdown();

    void performHealthCheck();
    HealthStatus getOverallHealth() const;
    std::vector<HealthCheck> getHealthChecks() const;
    
    void recordResourceCreation(const std::string& type);
    void recordResourceDeletion(const std::string& type);
    ResourceCounts getResourceCounts() const;

    void setMemoryThreshold(uint64_t thresholdMB) { memoryThresholdMB_ = thresholdMB; }
    void setFPSThreshold(float threshold) { fpsThreshold_ = threshold; }
    void setFrameTimeThreshold(float thresholdMs) { frameTimeThresholdMs_ = thresholdMs; }

    void exportHealthReport(const std::string& filename) const;
    std::string generateHealthReport() const;

private:
    HealthMonitor() = default;
    ~HealthMonitor();
    HealthMonitor(const HealthMonitor&) = delete;
    HealthMonitor& operator=(const HealthMonitor&) = delete;

    void checkContextValidity();
    void checkMemoryUsage();
    void checkFrameRate();
    void checkResourceLeaks();
    void checkShaderErrors();
    void checkOpenGLErrors();

    HealthCheck createCheck(const std::string& name, HealthStatus status, 
                           const std::string& message, const std::vector<std::string>& suggestions = {});

    mutable std::mutex mutex_;
    std::vector<HealthCheck> healthChecks_;
    ResourceCounts resourceCounts_;
    std::unordered_map<std::string, size_t> resourceCreationCount_;
    std::unordered_map<std::string, size_t> resourceDeletionCount_;
    
    uint64_t memoryThresholdMB_ = 2048;  // 2GB default
    float fpsThreshold_ = 30.0f;  // Minimum acceptable FPS
    float frameTimeThresholdMs_ = 33.33f;  // Maximum acceptable frame time (30 FPS)
    
    bool initialized_ = false;
    std::chrono::system_clock::time_point lastHealthCheck_;
    std::chrono::seconds healthCheckInterval_ = std::chrono::seconds(5);
    
    float lastRecordedFPS_ = 0.0f;
    std::chrono::microseconds lastRecordedFrameTime_{0};
};

} // namespace OpenGLHIP

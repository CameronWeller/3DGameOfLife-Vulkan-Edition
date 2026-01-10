#pragma once

#include <glad/glad.h>
#include <chrono>
#include <string>
#include <mutex>
#include <vector>
#include <unordered_map>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <nlohmann/json.hpp>
#include "../../../../include/Logger.h"

namespace OpenGLHIP {

struct FrameMetrics {
    std::chrono::high_resolution_clock::time_point frameStart;
    std::chrono::microseconds frameTime;
    std::chrono::microseconds computeTime;
    std::chrono::microseconds renderTime;
    uint32_t drawCalls = 0;
    uint32_t vertexCount = 0;
    uint64_t memoryUsed = 0;
    float fps = 0.0f;
};

struct PerformanceStats {
    std::chrono::microseconds minFrameTime;
    std::chrono::microseconds maxFrameTime;
    std::chrono::microseconds avgFrameTime;
    std::chrono::microseconds minComputeTime;
    std::chrono::microseconds maxComputeTime;
    std::chrono::microseconds avgComputeTime;
    std::chrono::microseconds minRenderTime;
    std::chrono::microseconds maxRenderTime;
    std::chrono::microseconds avgRenderTime;
    float avgFPS = 0.0f;
    float minFPS = 0.0f;
    float maxFPS = 0.0f;
    uint32_t totalFrames = 0;
    uint64_t totalMemory = 0;
};

class OpenGLProfiler {
public:
    static OpenGLProfiler& getInstance() {
        static OpenGLProfiler instance;
        return instance;
    }

    void initialize();
    void shutdown();

    void beginFrame();
    void endFrame();
    void beginCompute();
    void endCompute();
    void beginRender();
    void endRender();

    void recordDrawCall(uint32_t vertexCount = 0);
    void recordMemoryUsage(uint64_t bytes);

    PerformanceStats getStats() const;
    FrameMetrics getLastFrameMetrics() const;
    std::vector<FrameMetrics> getFrameHistory(size_t count = 100) const;

    void exportToJSON(const std::string& filename) const;
    void exportToCSV(const std::string& filename) const;
    void clearHistory();

    void setLoggingEnabled(bool enabled) { loggingEnabled_ = enabled; }
    void setMaxHistorySize(size_t size) { maxHistorySize_ = size; }

private:
    OpenGLProfiler() = default;
    ~OpenGLProfiler();
    OpenGLProfiler(const OpenGLProfiler&) = delete;
    OpenGLProfiler& operator=(const OpenGLProfiler&) = delete;

    mutable std::mutex mutex_;
    std::vector<FrameMetrics> frameHistory_;
    FrameMetrics currentFrame_;
    PerformanceStats stats_;
    size_t maxHistorySize_ = 1000;
    bool loggingEnabled_ = true;
    bool initialized_ = false;

    std::chrono::high_resolution_clock::time_point frameStartTime_;
    std::chrono::high_resolution_clock::time_point computeStartTime_;
    std::chrono::high_resolution_clock::time_point renderStartTime_;

    void updateStats();
    void logMetrics(const FrameMetrics& metrics);
};

} // namespace OpenGLHIP

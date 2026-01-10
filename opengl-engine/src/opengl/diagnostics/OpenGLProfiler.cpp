#include "opengl/diagnostics/OpenGLProfiler.h"
#include <algorithm>
#include <numeric>
#include <filesystem>
#include <fstream>

namespace OpenGLHIP {

void OpenGLProfiler::initialize() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (initialized_) {
        return;
    }

    // Initialize stats
    stats_ = PerformanceStats{};
    stats_.minFrameTime = std::chrono::microseconds::max();
    stats_.minComputeTime = std::chrono::microseconds::max();
    stats_.minRenderTime = std::chrono::microseconds::max();
    
    frameHistory_.clear();
    frameHistory_.reserve(maxHistorySize_);
    
    initialized_ = true;
    VulkanHIP::Logger::getInstance().log(VulkanHIP::Logger::LogLevel::Info, "OpenGL Profiler initialized");
}

void OpenGLProfiler::shutdown() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (initialized_ && loggingEnabled_) {
        exportToJSON("logs/opengl_profiler_final.json");
    }
    
    initialized_ = false;
}

OpenGLProfiler::~OpenGLProfiler() {
    shutdown();
}

void OpenGLProfiler::beginFrame() {
    if (!initialized_) return;
    
    std::lock_guard<std::mutex> lock(mutex_);
    frameStartTime_ = std::chrono::high_resolution_clock::now();
    currentFrame_ = FrameMetrics{};
    currentFrame_.frameStart = frameStartTime_;
    currentFrame_.drawCalls = 0;
    currentFrame_.vertexCount = 0;
    currentFrame_.memoryUsed = 0;
}

void OpenGLProfiler::endFrame() {
    if (!initialized_) return;
    
    std::lock_guard<std::mutex> lock(mutex_);
    auto frameEnd = std::chrono::high_resolution_clock::now();
    currentFrame_.frameTime = std::chrono::duration_cast<std::chrono::microseconds>(frameEnd - frameStartTime_);
    
    if (currentFrame_.frameTime.count() > 0) {
        currentFrame_.fps = 1000000.0f / static_cast<float>(currentFrame_.frameTime.count());
    }
    
    frameHistory_.push_back(currentFrame_);
    if (frameHistory_.size() > maxHistorySize_) {
        frameHistory_.erase(frameHistory_.begin());
    }
    
    updateStats();
    
    if (loggingEnabled_ && frameHistory_.size() % 60 == 0) {  // Log every 60 frames
        logMetrics(currentFrame_);
    }
}

void OpenGLProfiler::beginCompute() {
    if (!initialized_) return;
    computeStartTime_ = std::chrono::high_resolution_clock::now();
}

void OpenGLProfiler::endCompute() {
    if (!initialized_) return;
    
    std::lock_guard<std::mutex> lock(mutex_);
    auto computeEnd = std::chrono::high_resolution_clock::now();
    currentFrame_.computeTime = std::chrono::duration_cast<std::chrono::microseconds>(computeEnd - computeStartTime_);
}

void OpenGLProfiler::beginRender() {
    if (!initialized_) return;
    renderStartTime_ = std::chrono::high_resolution_clock::now();
}

void OpenGLProfiler::endRender() {
    if (!initialized_) return;
    
    std::lock_guard<std::mutex> lock(mutex_);
    auto renderEnd = std::chrono::high_resolution_clock::now();
    currentFrame_.renderTime = std::chrono::duration_cast<std::chrono::microseconds>(renderEnd - renderStartTime_);
}

void OpenGLProfiler::recordDrawCall(uint32_t vertexCount) {
    if (!initialized_) return;
    
    std::lock_guard<std::mutex> lock(mutex_);
    currentFrame_.drawCalls++;
    currentFrame_.vertexCount += vertexCount;
}

void OpenGLProfiler::recordMemoryUsage(uint64_t bytes) {
    if (!initialized_) return;
    
    std::lock_guard<std::mutex> lock(mutex_);
    currentFrame_.memoryUsed = bytes;
}

PerformanceStats OpenGLProfiler::getStats() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return stats_;
}

FrameMetrics OpenGLProfiler::getLastFrameMetrics() const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (frameHistory_.empty()) {
        return FrameMetrics{};
    }
    return frameHistory_.back();
}

std::vector<FrameMetrics> OpenGLProfiler::getFrameHistory(size_t count) const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (frameHistory_.empty()) {
        return {};
    }
    
    size_t startIdx = frameHistory_.size() > count ? frameHistory_.size() - count : 0;
    return std::vector<FrameMetrics>(frameHistory_.begin() + startIdx, frameHistory_.end());
}

void OpenGLProfiler::updateStats() {
    if (frameHistory_.empty()) {
        return;
    }
    
    stats_.totalFrames = static_cast<uint32_t>(frameHistory_.size());
    
    // Calculate frame time stats
    std::vector<int64_t> frameTimes;
    std::vector<int64_t> computeTimes;
    std::vector<int64_t> renderTimes;
    std::vector<float> fpsValues;
    
    for (const auto& frame : frameHistory_) {
        frameTimes.push_back(frame.frameTime.count());
        computeTimes.push_back(frame.computeTime.count());
        renderTimes.push_back(frame.renderTime.count());
        fpsValues.push_back(frame.fps);
        stats_.totalMemory += frame.memoryUsed;
    }
    
    if (!frameTimes.empty()) {
        auto [minIt, maxIt] = std::minmax_element(frameTimes.begin(), frameTimes.end());
        stats_.minFrameTime = std::chrono::microseconds(*minIt);
        stats_.maxFrameTime = std::chrono::microseconds(*maxIt);
        int64_t avgFrameTime = std::accumulate(frameTimes.begin(), frameTimes.end(), 0LL) / frameTimes.size();
        stats_.avgFrameTime = std::chrono::microseconds(avgFrameTime);
    }
    
    if (!computeTimes.empty()) {
        auto [minIt, maxIt] = std::minmax_element(computeTimes.begin(), computeTimes.end());
        stats_.minComputeTime = std::chrono::microseconds(*minIt);
        stats_.maxComputeTime = std::chrono::microseconds(*maxIt);
        int64_t avgComputeTime = std::accumulate(computeTimes.begin(), computeTimes.end(), 0LL) / computeTimes.size();
        stats_.avgComputeTime = std::chrono::microseconds(avgComputeTime);
    }
    
    if (!renderTimes.empty()) {
        auto [minIt, maxIt] = std::minmax_element(renderTimes.begin(), renderTimes.end());
        stats_.minRenderTime = std::chrono::microseconds(*minIt);
        stats_.maxRenderTime = std::chrono::microseconds(*maxIt);
        int64_t avgRenderTime = std::accumulate(renderTimes.begin(), renderTimes.end(), 0LL) / renderTimes.size();
        stats_.avgRenderTime = std::chrono::microseconds(avgRenderTime);
    }
    
    if (!fpsValues.empty()) {
        stats_.avgFPS = std::accumulate(fpsValues.begin(), fpsValues.end(), 0.0f) / fpsValues.size();
        stats_.minFPS = *std::min_element(fpsValues.begin(), fpsValues.end());
        stats_.maxFPS = *std::max_element(fpsValues.begin(), fpsValues.end());
    }
}

void OpenGLProfiler::exportToJSON(const std::string& filename) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    nlohmann::json json;
    json["stats"] = {
        {"totalFrames", stats_.totalFrames},
        {"avgFPS", stats_.avgFPS},
        {"minFPS", stats_.minFPS},
        {"maxFPS", stats_.maxFPS},
        {"avgFrameTime_us", stats_.avgFrameTime.count()},
        {"minFrameTime_us", stats_.minFrameTime.count()},
        {"maxFrameTime_us", stats_.maxFrameTime.count()},
        {"avgComputeTime_us", stats_.avgComputeTime.count()},
        {"avgRenderTime_us", stats_.avgRenderTime.count()},
        {"totalMemory_bytes", stats_.totalMemory}
    };
    
    json["frames"] = nlohmann::json::array();
    for (const auto& frame : frameHistory_) {
        json["frames"].push_back({
            {"frameTime_us", frame.frameTime.count()},
            {"computeTime_us", frame.computeTime.count()},
            {"renderTime_us", frame.renderTime.count()},
            {"fps", frame.fps},
            {"drawCalls", frame.drawCalls},
            {"vertexCount", frame.vertexCount},
            {"memoryUsed_bytes", frame.memoryUsed}
        });
    }
    
    std::filesystem::create_directories(std::filesystem::path(filename).parent_path());
    std::ofstream file(filename);
    if (file.is_open()) {
        file << json.dump(2);
        file.close();
    }
}

void OpenGLProfiler::exportToCSV(const std::string& filename) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::filesystem::create_directories(std::filesystem::path(filename).parent_path());
    std::ofstream file(filename);
    if (!file.is_open()) {
        return;
    }
    
    file << "FrameTime_us,ComputeTime_us,RenderTime_us,FPS,DrawCalls,VertexCount,MemoryUsed_bytes\n";
    
    for (const auto& frame : frameHistory_) {
        file << frame.frameTime.count() << ","
             << frame.computeTime.count() << ","
             << frame.renderTime.count() << ","
             << frame.fps << ","
             << frame.drawCalls << ","
             << frame.vertexCount << ","
             << frame.memoryUsed << "\n";
    }
    
    file.close();
}

void OpenGLProfiler::clearHistory() {
    std::lock_guard<std::mutex> lock(mutex_);
    frameHistory_.clear();
    stats_ = PerformanceStats{};
}

void OpenGLProfiler::logMetrics(const FrameMetrics& metrics) {
    std::stringstream ss;
    ss << "Frame Metrics - FPS: " << std::fixed << std::setprecision(2) << metrics.fps
       << ", Frame Time: " << metrics.frameTime.count() << "us"
       << ", Compute: " << metrics.computeTime.count() << "us"
       << ", Render: " << metrics.renderTime.count() << "us"
       << ", Draw Calls: " << metrics.drawCalls
       << ", Vertices: " << metrics.vertexCount
       << ", Memory: " << (metrics.memoryUsed / 1024 / 1024) << "MB";
    
    VulkanHIP::Logger::getInstance().log(VulkanHIP::Logger::LogLevel::Debug, ss.str());
}

} // namespace OpenGLHIP

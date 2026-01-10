#include "opengl/ui/OpenGLImGui.h"
#include "../../../../include/Logger.h"
#include "opengl/diagnostics/OpenGLProfiler.h"
#include "opengl/diagnostics/HealthMonitor.h"
#include <stdexcept>

namespace OpenGLHIP {

void OpenGLImGui::initialize(GLFWwindow* window) {
    if (initialized_) {
        VulkanHIP::Logger::getInstance().log(VulkanHIP::Logger::LogLevel::Warning,
            "ImGui already initialized");
        return;
    }

    window_ = window;

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
    
    // Enable docking (optional)
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    
    // Setup Dear ImGui style
    ImGui::StyleColorsDark();

    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForOpenGL(window_, true);
    ImGui_ImplOpenGL3_Init("#version 430");  // Use OpenGL 4.3 core profile
    
    initialized_ = true;
    VulkanHIP::Logger::getInstance().log(VulkanHIP::Logger::LogLevel::Info,
        "ImGui initialized with OpenGL backend");
}

void OpenGLImGui::cleanup() {
    if (!initialized_) {
        return;
    }
    
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    
    initialized_ = false;
    window_ = nullptr;
}

OpenGLImGui::~OpenGLImGui() {
    cleanup();
}

void OpenGLImGui::shutdown() {
    cleanup();
}

void OpenGLImGui::beginFrame() {
    if (!initialized_) return;
    
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void OpenGLImGui::endFrame() {
    if (!initialized_) return;
    
    ImGui::EndFrame();
}

void OpenGLImGui::render() {
    if (!initialized_) return;
    
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void OpenGLImGui::renderDebugPanel() {
    if (!showDebugPanel_) return;
    
    if (ImGui::Begin("Debug Panel", &showDebugPanel_)) {
        auto& profiler = OpenGLProfiler::getInstance();
        auto stats = profiler.getStats();
        auto lastFrame = profiler.getLastFrameMetrics();
        
        ImGui::Text("Performance Metrics");
        ImGui::Separator();
        ImGui::Text("FPS: %.2f (Min: %.2f, Max: %.2f, Avg: %.2f)", 
                    lastFrame.fps, stats.minFPS, stats.maxFPS, stats.avgFPS);
        ImGui::Text("Frame Time: %.2f ms", lastFrame.frameTime.count() / 1000.0f);
        ImGui::Text("Compute Time: %.2f ms", lastFrame.computeTime.count() / 1000.0f);
        ImGui::Text("Render Time: %.2f ms", lastFrame.renderTime.count() / 1000.0f);
        ImGui::Text("Draw Calls: %u", lastFrame.drawCalls);
        ImGui::Text("Vertices: %u", lastFrame.vertexCount);
        ImGui::Text("Memory Used: %.2f MB", lastFrame.memoryUsed / 1024.0f / 1024.0f);
        ImGui::Text("Total Frames: %u", stats.totalFrames);
    }
    ImGui::End();
}

void OpenGLImGui::renderSettingsPanel() {
    if (!showSettingsPanel_) return;
    
    if (ImGui::Begin("Settings", &showSettingsPanel_)) {
        ImGui::Text("Simulation Settings");
        ImGui::Separator();
        // Settings will be implemented based on game state
    }
    ImGui::End();
}

void OpenGLImGui::renderPerformancePanel() {
    if (!showPerformancePanel_) return;
    
    if (ImGui::Begin("Performance", &showPerformancePanel_)) {
        auto& healthMonitor = HealthMonitor::getInstance();
        auto overallHealth = healthMonitor.getOverallHealth();
        
        ImGui::Text("System Health");
        ImGui::Separator();
        
        const char* healthStatus = "Unknown";
        switch (overallHealth) {
            case HealthStatus::Healthy: healthStatus = "Healthy"; break;
            case HealthStatus::Warning: healthStatus = "Warning"; break;
            case HealthStatus::Critical: healthStatus = "Critical"; break;
            default: break;
        }
        ImGui::Text("Overall Status: %s", healthStatus);
        
        auto checks = healthMonitor.getHealthChecks();
        for (const auto& check : checks) {
            const char* statusStr = "Unknown";
            switch (check.status) {
                case HealthStatus::Healthy: statusStr = "OK"; break;
                case HealthStatus::Warning: statusStr = "WARN"; break;
                case HealthStatus::Critical: statusStr = "CRITICAL"; break;
                default: break;
            }
            ImGui::Text("[%s] %s: %s", statusStr, check.name.c_str(), check.message.c_str());
            if (!check.suggestions.empty()) {
                for (const auto& suggestion : check.suggestions) {
                    ImGui::BulletText("%s", suggestion.c_str());
                }
            }
        }
    }
    ImGui::End();
}

void OpenGLImGui::renderSimulationControls() {
    // This will be implemented with actual game state
    // For now, just a placeholder
}

} // namespace OpenGLHIP

#pragma once

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <string>
#include <memory>
#include "../../../../include/Logger.h"

namespace OpenGLHIP {

class OpenGLImGui {
public:
    static OpenGLImGui& getInstance() {
        static OpenGLImGui instance;
        return instance;
    }

    void initialize(GLFWwindow* window);
    void cleanup();
    void shutdown();
    
    void beginFrame();
    void endFrame();
    void render();
    
    // UI rendering methods
    void renderDebugPanel();
    void renderSettingsPanel();
    void renderPerformancePanel();
    void renderSimulationControls();
    
    // State
    bool isInitialized() const { return initialized_; }
    
private:
    OpenGLImGui() = default;
    ~OpenGLImGui();
    OpenGLImGui(const OpenGLImGui&) = delete;
    OpenGLImGui& operator=(const OpenGLImGui&) = delete;

    GLFWwindow* window_ = nullptr;
    bool initialized_ = false;
    bool showDebugPanel_ = true;
    bool showSettingsPanel_ = true;
    bool showPerformancePanel_ = true;
};

} // namespace OpenGLHIP

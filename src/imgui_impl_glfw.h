#pragma once

#include <imgui.h>
#include <GLFW/glfw3.h>

namespace ImGui {
    bool InitForGLFW(GLFWwindow* window, bool install_callbacks = true);
    void ShutdownForGLFW();
    void NewFrameForGLFW();
    void RenderForGLFW();
    void UpdateMousePosAndButtons();
    void UpdateMouseCursor();
    void UpdateGamepads();
} 
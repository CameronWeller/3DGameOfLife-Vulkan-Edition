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

// Initialize GLFW for ImGui
IMGUI_IMPL_API bool     ImGui_ImplGlfw_InitForVulkan(GLFWwindow* window, bool install_callbacks);
IMGUI_IMPL_API void     ImGui_ImplGlfw_Shutdown();
IMGUI_IMPL_API void     ImGui_ImplGlfw_NewFrame();
IMGUI_IMPL_API void     ImGui_ImplGlfw_RenderDrawData(ImDrawData* draw_data);

// GLFW callbacks (installed by default if you enable 'install_callbacks' during initialization)
// You can also handle inputs yourself and use those as a reference.
IMGUI_IMPL_API void     ImGui_ImplGlfw_MouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
IMGUI_IMPL_API void     ImGui_ImplGlfw_ScrollCallback(GLFWwindow* window, double xoffset, double yoffset);
IMGUI_IMPL_API void     ImGui_ImplGlfw_KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
IMGUI_IMPL_API void     ImGui_ImplGlfw_CharCallback(GLFWwindow* window, unsigned int c);
IMGUI_IMPL_API void     ImGui_ImplGlfw_MonitorCallback(GLFWmonitor* monitor, int event); 
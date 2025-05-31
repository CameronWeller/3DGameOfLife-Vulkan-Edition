#include "UI.h"
#include "VulkanEngine.h"
#include "AppState.h"
#include <imgui.h>
#include "imgui_impl_glfw.h"
#include "imgui_impl_vulkan.h"
#include <sstream>
#include <iomanip>
#include <thread>
#include <filesystem>

UI::UI(VulkanEngine* engine)
    : engine_(engine), isPaused_(true), tickRate(1.0f),
      placementMode(false), placementPos(0.0f),
      gridMin(0.0f), gridMax(250.0f), voxelSize(1.0f),
      population(0), generation(0) {
    if (engine_) {
        window = engine_->getWindowManager()->getWindow();
    }
}

UI::~UI() {
    cleanupPreviewTextures();
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
}

// Global variable for window close state
bool g_shouldClose = false;

void UI::init() {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    
    // Set up modern theme
    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    
    // Modern theme settings
    style.WindowRounding = 6.0f;
    style.FrameRounding = 6.0f;
    style.PopupRounding = 6.0f;
    style.ScrollbarRounding = 6.0f;
    style.GrabRounding = 6.0f;
    style.TabRounding = 6.0f;
    style.WindowBorderSize = 0.0f;
    style.FrameBorderSize = 0.0f;
    style.PopupBorderSize = 0.0f;
    style.ScrollbarSize = 12.0f;
    style.GrabMinSize = 20.0f;
    style.WindowTitleAlign = ImVec2(0.5f, 0.5f);
    style.ButtonTextAlign = ImVec2(0.5f, 0.5f);
    style.DisplayWindowPadding = ImVec2(15, 15);
    style.DisplaySafeAreaPadding = ImVec2(15, 15);
    style.ItemSpacing = ImVec2(8, 4);
    style.ItemInnerSpacing = ImVec2(4, 4);
    style.TouchExtraPadding = ImVec2(0, 0);
    style.IndentSpacing = 21.0f;
    style.ColumnsMinSpacing = 6.0f;
    style.ScrollbarSize = 12.0f;
    style.ScrollbarRounding = 6.0f;
    style.GrabMinSize = 20.0f;
    style.GrabRounding = 6.0f;
    style.WindowTitleAlign = ImVec2(0.5f, 0.5f);
    style.ButtonTextAlign = ImVec2(0.5f, 0.5f);
    style.SelectableTextAlign = ImVec2(0.0f, 0.5f);
    
    // Colors
    ImVec4* colors = style.Colors;
    colors[ImGuiCol_Text] = ImVec4(0.95f, 0.95f, 0.95f, 1.00f);
    colors[ImGuiCol_TextDisabled] = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
    colors[ImGuiCol_WindowBg] = ImVec4(0.06f, 0.06f, 0.06f, 0.94f);
    colors[ImGuiCol_ChildBg] = ImVec4(0.08f, 0.08f, 0.08f, 0.00f);
    colors[ImGuiCol_PopupBg] = ImVec4(0.08f, 0.08f, 0.08f, 0.94f);
    colors[ImGuiCol_Border] = ImVec4(0.43f, 0.43f, 0.50f, 0.50f);
    colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_FrameBg] = ImVec4(0.16f, 0.16f, 0.16f, 1.00f);
    colors[ImGuiCol_FrameBgHovered] = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
    colors[ImGuiCol_FrameBgActive] = ImVec4(0.24f, 0.24f, 0.24f, 1.00f);
    colors[ImGuiCol_TitleBg] = ImVec4(0.04f, 0.04f, 0.04f, 1.00f);
    colors[ImGuiCol_TitleBgActive] = ImVec4(0.16f, 0.16f, 0.16f, 1.00f);
    colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.00f, 0.00f, 0.00f, 0.51f);
    colors[ImGuiCol_MenuBarBg] = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
    colors[ImGuiCol_ScrollbarBg] = ImVec4(0.02f, 0.02f, 0.02f, 0.53f);
    colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.31f, 0.31f, 0.31f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.41f, 0.41f, 0.41f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.51f, 0.51f, 0.51f, 1.00f);
    colors[ImGuiCol_CheckMark] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
    colors[ImGuiCol_SliderGrab] = ImVec4(0.24f, 0.52f, 0.88f, 1.00f);
    colors[ImGuiCol_SliderGrabActive] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
    colors[ImGuiCol_Button] = ImVec4(0.26f, 0.59f, 0.98f, 0.40f);
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
    colors[ImGuiCol_ButtonActive] = ImVec4(0.06f, 0.53f, 0.98f, 1.00f);
    colors[ImGuiCol_Header] = ImVec4(0.26f, 0.59f, 0.98f, 0.31f);
    colors[ImGuiCol_HeaderHovered] = ImVec4(0.26f, 0.59f, 0.98f, 0.80f);
    colors[ImGuiCol_HeaderActive] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
    colors[ImGuiCol_Separator] = ImVec4(0.43f, 0.43f, 0.50f, 0.50f);
    colors[ImGuiCol_SeparatorHovered] = ImVec4(0.41f, 0.42f, 0.44f, 1.00f);
    colors[ImGuiCol_SeparatorActive] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
    colors[ImGuiCol_ResizeGrip] = ImVec4(0.26f, 0.59f, 0.98f, 0.20f);
    colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.26f, 0.59f, 0.98f, 0.67f);
    colors[ImGuiCol_ResizeGripActive] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
    colors[ImGuiCol_Tab] = ImVec4(0.18f, 0.18f, 0.18f, 0.86f);
    colors[ImGuiCol_TabHovered] = ImVec4(0.26f, 0.59f, 0.98f, 0.80f);
    colors[ImGuiCol_TabActive] = ImVec4(0.20f, 0.41f, 0.68f, 1.00f);
    colors[ImGuiCol_TabUnfocused] = ImVec4(0.07f, 0.10f, 0.15f, 0.97f);
    colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.14f, 0.26f, 0.42f, 1.00f);
    colors[ImGuiCol_PlotLines] = ImVec4(0.61f, 0.61f, 0.61f, 1.00f);
    colors[ImGuiCol_PlotLinesHovered] = ImVec4(1.00f, 0.43f, 0.35f, 1.00f);
    colors[ImGuiCol_PlotHistogram] = ImVec4(0.90f, 0.70f, 0.00f, 1.00f);
    colors[ImGuiCol_PlotHistogramHovered] = ImVec4(1.00f, 0.60f, 0.00f, 1.00f);
    colors[ImGuiCol_TextSelectedBg] = ImVec4(0.26f, 0.59f, 0.98f, 0.35f);
    colors[ImGuiCol_DragDropTarget] = ImVec4(0.26f, 0.59f, 0.98f, 0.95f);
    colors[ImGuiCol_NavHighlight] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
    colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
    colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0.80f, 0.80f, 0.80f, 0.20f);
    colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.80f, 0.80f, 0.80f, 0.35f);
    
    ImGui_ImplGlfw_InitForVulkan(window, true);
    // Note: ImGui::InitForVulkan will be called after Vulkan device creation
}

void UI::render() {
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    
    handleInput();
    
    // Main menu bar
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("Save Pattern", "Ctrl+S")) {
                showSavePatternDialog_ = true;
            }
            if (ImGui::MenuItem("Load Pattern", "Ctrl+O")) {
                showLoadPatternDialog_ = true;
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Exit", "Alt+F4")) {
                g_shouldClose = true;
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("View")) {
            ImGui::MenuItem("Statistics", nullptr, &showStatsWindow_);
            ImGui::MenuItem("Controls", nullptr, &showControlsWindow_);
            ImGui::MenuItem("Settings", nullptr, &showSettingsWindow_);
            ImGui::MenuItem("Performance", nullptr, &showPerformanceWindow_);
            ImGui::MenuItem("Pattern Browser", nullptr, &showPatternBrowser_);
            ImGui::MenuItem("Rule Analysis", nullptr, &showRuleAnalysisWindow_);
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Help")) {
            ImGui::MenuItem("About", nullptr, &showAboutWindow_);
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }
    
    // Camera mode indicator overlay
    {
        ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDecoration | 
                                      ImGuiWindowFlags_AlwaysAutoResize | 
                                      ImGuiWindowFlags_NoSavedSettings |
                                      ImGuiWindowFlags_NoFocusOnAppearing |
                                      ImGuiWindowFlags_NoNav |
                                      ImGuiWindowFlags_NoMove;
        
        const float PAD = 10.0f;
        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImVec2 work_pos = viewport->WorkPos;
        ImVec2 work_size = viewport->WorkSize;
        ImVec2 window_pos = ImVec2(work_pos.x + work_size.x - PAD, work_pos.y + PAD);
        ImVec2 window_pos_pivot = ImVec2(1.0f, 0.0f);
        ImGui::SetNextWindowPos(window_pos, ImGuiCond_Always, window_pos_pivot);
        ImGui::SetNextWindowBgAlpha(0.35f);
        
        if (ImGui::Begin("Camera Mode", nullptr, window_flags)) {
            CameraMode mode = engine_->getCamera()->getMode();
            const char* modeText = "";
            ImVec4 modeColor = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
            
            switch (mode) {
                case CameraMode::Fly:
                    modeText = "Fly Mode";
                    modeColor = ImVec4(0.0f, 0.8f, 0.0f, 1.0f);
                    break;
                case CameraMode::Orbit:
                    modeText = "Orbit Mode";
                    modeColor = ImVec4(0.0f, 0.6f, 1.0f, 1.0f);
                    break;
                case CameraMode::Pan:
                    modeText = "Pan Mode";
                    modeColor = ImVec4(1.0f, 0.6f, 0.0f, 1.0f);
                    break;
                case CameraMode::FirstPerson:
                    modeText = "First Person Mode";
                    modeColor = ImVec4(1.0f, 0.0f, 0.0f, 1.0f);
                    break;
            }
            
            ImGui::PushStyleColor(ImGuiCol_Text, modeColor);
            ImGui::Text("%s", modeText);
            ImGui::PopStyleColor();
            
            // Add mode-specific controls
            switch (mode) {
                case CameraMode::Fly:
                    ImGui::Text("WASD: Move");
                    ImGui::Text("Space/Ctrl: Up/Down");
                    ImGui::Text("Mouse: Look");
                    break;
                case CameraMode::Orbit:
                    ImGui::Text("Right Mouse: Orbit");
                    ImGui::Text("Scroll: Distance");
                    break;
                case CameraMode::Pan:
                    ImGui::Text("Middle Mouse: Pan");
                    ImGui::Text("Scroll: Zoom");
                    break;
                case CameraMode::FirstPerson:
                    ImGui::Text("WASD: Move");
                    ImGui::Text("Space/Ctrl: Up/Down");
                    ImGui::Text("Mouse: Look");
                    break;
            }
        }
        ImGui::End();
    }
    
    // Render all windows
    if (showStatsWindow_) renderStats();
    if (showControlsWindow_) renderControls();
    if (showSettingsWindow_) renderSettings();
    if (showPerformanceWindow_) renderPerformance();
    if (showAboutWindow_) renderAbout();
    if (showPatternBrowser_) renderPatternBrowser();
    if (showSavePatternDialog_) renderSavePatternDialog();
    if (showLoadPatternDialog_) renderLoadPatternDialog();
    if (showRuleAnalysisWindow_) renderRuleAnalysis();
    
    ImGui::Render();
}

void UI::handleInput() {
    // Toggle pause with Space
    if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) {
        isPaused_ = !isPaused_;
    }
    
    // Toggle placement mode with E
    if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS) {
        placementMode = !placementMode;
    }
    
    // Camera mode shortcuts
    static bool keyPressed = false;
    if (!keyPressed) {
        if (glfwGetKey(window, GLFW_KEY_1) == GLFW_PRESS) {
            engine_->getCamera()->setMode(CameraMode::Fly);
            keyPressed = true;
        }
        else if (glfwGetKey(window, GLFW_KEY_2) == GLFW_PRESS) {
            engine_->getCamera()->setMode(CameraMode::Orbit);
            keyPressed = true;
        }
        else if (glfwGetKey(window, GLFW_KEY_3) == GLFW_PRESS) {
            engine_->getCamera()->setMode(CameraMode::Pan);
            keyPressed = true;
        }
        else if (glfwGetKey(window, GLFW_KEY_4) == GLFW_PRESS) {
            engine_->getCamera()->setMode(CameraMode::FirstPerson);
            keyPressed = true;
        }
    }
    else if (glfwGetKey(window, GLFW_KEY_1) == GLFW_RELEASE &&
             glfwGetKey(window, GLFW_KEY_2) == GLFW_RELEASE &&
             glfwGetKey(window, GLFW_KEY_3) == GLFW_RELEASE &&
             glfwGetKey(window, GLFW_KEY_4) == GLFW_RELEASE) {
        keyPressed = false;
    }
    
    // Update placement position
    if (placementMode) {
        updatePlacementPosition();
        
        // Place/remove voxel with left/right mouse button
        if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS) {
            if (onPlaceVoxel) onPlaceVoxel(placementPos);
        }
        if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS) {
            if (onRemoveVoxel) onRemoveVoxel(placementPos);
        }
    }
}

glm::vec3 UI::getMouseRayDirection() {
    double xpos, ypos;
    glfwGetCursorPos(window, &xpos, &ypos);
    
    int width, height;
    glfwGetFramebufferSize(window, &width, &height);
    
    // Convert to normalized device coordinates
    float x = (2.0f * xpos) / width - 1.0f;
    float y = 1.0f - (2.0f * ypos) / height;
    
    // Convert to clip space
    glm::vec4 clipCoords(x, y, -1.0f, 1.0f);
    
    // Convert to eye space
    glm::mat4 invProj = glm::inverse(camera->getProjectionMatrix());
    glm::vec4 eyeCoords = invProj * clipCoords;
    eyeCoords.z = -1.0f;
    eyeCoords.w = 0.0f;
    
    // Convert to world space
    glm::mat4 invView = glm::inverse(camera->getViewMatrix());
    glm::vec4 worldCoords = invView * eyeCoords;
    
    return glm::normalize(glm::vec3(worldCoords));
}

void UI::updatePlacementPosition() {
    RayCaster::Ray ray;
    ray.origin = camera->getPosition();
    ray.direction = getMouseRayDirection();
    
    RayCaster::HitResult hit = RayCaster::castRay(ray, gridMin, gridMax);
    
    if (hit.hit) {
        // Move slightly along the normal to place voxel on the surface
        glm::vec3 worldPos = hit.position + hit.normal * 0.01f;
        placementPos = RayCaster::getGridPosition(worldPos, voxelSize);
    }
}

void UI::renderStats() {
    ImGui::Begin("Statistics", &showStatsWindow_);
    
    // Population and generation
    ImGui::Text("Population: %u", population);
    ImGui::Text("Generation: %u", generation);
    
    // Grid information
    ImGui::Separator();
    ImGui::Text("Grid Information");
    ImGui::Text("Size: %.0f x %.0f x %.0f", gridMax.x, gridMax.y, gridMax.z);
    ImGui::Text("Voxel Size: %.2f", voxelSize);
    
    // Camera information
    ImGui::Separator();
    ImGui::Text("Camera Information");
    glm::vec3 pos = engine_->getCamera()->getPosition();
    ImGui::Text("Position: (%.1f, %.1f, %.1f)", pos.x, pos.y, pos.z);
    
    // Performance metrics
    ImGui::Separator();
    ImGui::Text("Performance");
    ImGui::Text("FPS: %.1f", fps_);
    ImGui::Text("Frame Time: %.2f ms", frameTime_);
    ImGui::Text("Update Time: %.2f ms", updateTime_);
    ImGui::Text("Memory Usage: %.2f MB / %.2f MB", 
                usedMemory_ / (1024.0f * 1024.0f),
                totalMemory_ / (1024.0f * 1024.0f));
    
    ImGui::End();
}

void UI::renderControls() {
    ImGui::Begin("Controls", &showControlsWindow_);
    
    // Simulation controls
    if (ImGui::CollapsingHeader("Simulation")) {
        // Pause/Resume button
        if (ImGui::Button(isPaused_ ? "Resume" : "Pause")) {
            isPaused_ = !isPaused_;
        }
        
        // Tick rate slider
        ImGui::SliderFloat("Tick Rate", &tickRate, 0.1f, 10.0f, "%.1f Hz");
    }
    
    // Camera controls
    if (ImGui::CollapsingHeader("Camera")) {
        static int cameraMode = static_cast<int>(engine_->getCamera()->getMode());
        const char* modes[] = { "Fly", "Orbit", "Pan", "First Person" };
        
        // Camera mode selector with visual indicator
        ImGui::Text("Mode: ");
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 0.8f, 0.0f, 1.0f));
        ImGui::Text("%s", modes[cameraMode]);
        ImGui::PopStyleColor();
        
        // Mode selection buttons
        ImGui::BeginGroup();
        for (int i = 0; i < IM_ARRAYSIZE(modes); i++) {
            if (i > 0) ImGui::SameLine();
            if (ImGui::Button(modes[i], ImVec2(100, 0))) {
                cameraMode = i;
                engine_->getCamera()->setMode(static_cast<CameraMode>(cameraMode));
            }
            if (cameraMode == i) {
                ImGui::GetWindowDrawList()->AddRect(
                    ImGui::GetItemRectMin(),
                    ImGui::GetItemRectMax(),
                    ImGui::ColorConvertFloat4ToU32(ImVec4(0.0f, 0.8f, 0.0f, 1.0f)),
                    4.0f,
                    0,
                    2.0f
                );
            }
        }
        ImGui::EndGroup();
        
        // Camera settings
        static float moveSpeed = engine_->getCamera()->getMovementSpeed();
        if (ImGui::SliderFloat("Movement Speed", &moveSpeed, 1.0f, 200.0f)) {
            engine_->getCamera()->setMovementSpeed(moveSpeed);
        }
        
        static float mouseSensitivity = engine_->getCamera()->getMouseSensitivity();
        if (ImGui::SliderFloat("Mouse Sensitivity", &mouseSensitivity, 0.01f, 1.0f)) {
            engine_->getCamera()->setMouseSensitivity(mouseSensitivity);
        }
        
        // Orbit mode settings
        if (cameraMode == static_cast<int>(CameraMode::Orbit)) {
            static float orbitDistance = engine_->getCamera()->getOrbitDistance();
            if (ImGui::SliderFloat("Orbit Distance", &orbitDistance, 1.0f, 500.0f)) {
                engine_->getCamera()->setOrbitDistance(orbitDistance);
            }
            
            static glm::vec3 target = engine_->getCamera()->getTarget();
            if (ImGui::SliderFloat3("Target Point", &target.x, 0.0f, 250.0f)) {
                engine_->getCamera()->setTarget(target);
            }
        }
    }
    
    // Placement controls
    if (ImGui::CollapsingHeader("Placement")) {
        ImGui::Checkbox("Placement Mode (E)", &placementMode);
        
        if (placementMode) {
            ImGui::Text("Left Click: Place Voxel");
            ImGui::Text("Right Click: Remove Voxel");
            
            // Display current placement position
            std::stringstream ss;
            ss << "Position: (" << std::fixed << std::setprecision(1)
               << placementPos.x << ", " << placementPos.y << ", " << placementPos.z << ")";
            ImGui::Text("%s", ss.str().c_str());
        }
    }
    
    ImGui::End();
}

void UI::renderSettings() {
    ImGui::Begin("Settings", &showSettingsWindow_);
    
    if (ImGui::CollapsingHeader("Grid Settings")) {
        ImGui::SliderFloat3("Grid Size", &gridMax.x, 10.0f, 500.0f);
        ImGui::SliderFloat("Voxel Size", &voxelSize, 0.1f, 2.0f);
    }
    
    if (ImGui::CollapsingHeader("Rendering Settings")) {
        static bool wireframe = false;
        static bool showGrid = true;
        static float transparency = 1.0f;
        
        if (ImGui::Checkbox("Wireframe Mode", &wireframe)) {
            engine_->setWireframeMode(wireframe);
        }
        if (ImGui::Checkbox("Show Grid", &showGrid)) {
            engine_->setShowGrid(showGrid);
        }
        if (ImGui::SliderFloat("Transparency", &transparency, 0.0f, 1.0f)) {
            engine_->setTransparency(transparency);
        }
    }
    
    if (ImGui::CollapsingHeader("Rule Settings")) {
        static int selectedRule = 0;
        const char* rules[] = { "5766", "4555", "Custom" };
        
        if (ImGui::Combo("Rule Set", &selectedRule, rules, IM_ARRAYSIZE(rules))) {
            // Apply rule set changes immediately
            engine_->setRenderMode(selectedRule);
        }
        
        if (selectedRule == 2) { // Custom rules
            static int birthMin = 5, birthMax = 7;
            static int survivalMin = 6, survivalMax = 6;
            
            bool customRulesChanged = false;
            customRulesChanged |= ImGui::SliderInt2("Birth Range", &birthMin, 0, 26);
            customRulesChanged |= ImGui::SliderInt2("Survival Range", &survivalMin, 0, 26);
            
            if (customRulesChanged) {
                engine_->setCustomRules(birthMin, birthMax, survivalMin, survivalMax);
            }
        }
    }
    
    ImGui::End();
}

void UI::renderPerformance() {
    ImGui::Begin("Performance", &showPerformanceWindow_);
    
    // FPS counter - get real values from engine
    float fps = engine_->getCurrentFPS();
    float frameTime = engine_->getFrameTime();
    float updateTime = engine_->getUpdateTime();
    
    ImGui::Text("FPS: %.1f", fps);
    ImGui::Text("Frame Time: %.2f ms", frameTime);
    ImGui::Text("Update Time: %.2f ms", updateTime);
    
    // Memory usage - get real values from engine
    size_t totalMemory = engine_->getTotalMemory();
    size_t usedMemory = engine_->getUsedMemory();
    
    ImGui::Text("Memory Usage: %.2f MB / %.2f MB", 
                usedMemory / (1024.0f * 1024.0f),
                totalMemory / (1024.0f * 1024.0f));
    
    // Progress bars for visual feedback
    if (totalMemory > 0) {
        float memoryUsagePercent = static_cast<float>(usedMemory) / static_cast<float>(totalMemory);
        ImGui::ProgressBar(memoryUsagePercent, ImVec2(0.0f, 0.0f), 
                          (std::to_string(static_cast<int>(memoryUsagePercent * 100)) + "%").c_str());
    }
    
    ImGui::End();
}

void UI::renderAbout() {
    ImGui::Begin("About", &showAboutWindow_);
    
    ImGui::Text("3D Game of Life - Vulkan Edition");
    ImGui::Text("Version 1.0.0");
    ImGui::Separator();
    ImGui::Text("A 3D implementation of Conway's Game of Life");
    ImGui::Text("using the Vulkan graphics API.");
    ImGui::Separator();
    
    ImGui::Text("Controls:");
    ImGui::BulletText("Space: Pause/Resume");
    ImGui::BulletText("E: Toggle placement mode");
    ImGui::BulletText("Left Click: Place voxel");
    ImGui::BulletText("Right Click: Remove voxel");
    
    ImGui::Text("Camera Controls:");
    ImGui::BulletText("1: Fly mode");
    ImGui::BulletText("2: Orbit mode");
    ImGui::BulletText("3: Pan mode");
    ImGui::BulletText("4: First Person mode");
    ImGui::BulletText("WASD: Move camera (Fly/First Person)");
    ImGui::BulletText("Right Mouse: Orbit camera (Orbit mode)");
    ImGui::BulletText("Middle Mouse: Pan camera (Pan mode)");
    ImGui::BulletText("Mouse: Rotate camera (Fly/First Person)");
    ImGui::BulletText("Scroll: Zoom/Change orbit distance");
    
    ImGui::End();
}

void UI::renderPatternBrowser() {
    ImGui::Begin("Pattern Browser", &showPatternBrowser_);
    
    // Search filter
    static char searchFilter[256] = "";
    ImGui::InputText("Search", searchFilter, IM_ARRAYSIZE(searchFilter));
    
    // Pattern list
    static int selectedPattern = -1;
    static std::vector<App::SaveInfo> patterns = engine_->getSaveManager()->getPatternFiles();
    
    // Filter patterns based on search
    std::vector<App::SaveInfo> filteredPatterns;
    std::string searchLower = searchFilter;
    std::transform(searchLower.begin(), searchLower.end(), searchLower.begin(), ::tolower);
    
    for (const auto& pattern : patterns) {
        std::string nameLower = pattern.name;
        std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::tolower);
        std::string descLower = pattern.description;
        std::transform(descLower.begin(), descLower.end(), descLower.begin(), ::tolower);
        
        if (searchLower.empty() || 
            nameLower.find(searchLower) != std::string::npos ||
            descLower.find(searchLower) != std::string::npos) {
            filteredPatterns.push_back(pattern);
        }
    }
    
    ImGui::BeginChild("PatternList", ImVec2(200, 0), true);
    for (int i = 0; i < filteredPatterns.size(); i++) {
        if (ImGui::Selectable(filteredPatterns[i].name.c_str(), selectedPattern == i)) {
            selectedPattern = i;
        }
    }
    ImGui::EndChild();
    
    ImGui::SameLine();
    
    // Pattern details
    ImGui::BeginChild("PatternDetails", ImVec2(0, 0), true);
    if (selectedPattern >= 0 && selectedPattern < filteredPatterns.size()) {
        const auto& pattern = filteredPatterns[selectedPattern];
        
        ImGui::Text("Name: %s", pattern.name.c_str());
        ImGui::Text("Author: %s", pattern.author.c_str());
        ImGui::Text("Version: %s", pattern.version.c_str());
        ImGui::Text("Description: %s", pattern.description.c_str());
        
        // Preview image with improved layout
        if (engine_->getSaveManager()->hasPreview(pattern.filename)) {
            std::string previewPath = engine_->getSaveManager()->getPreviewPath(pattern.filename);
            
            // Load preview texture if not already loaded
            if (previewTextures_.find(previewPath) == previewTextures_.end()) {
                // Create texture from image file
                VkImage image;
                VkDeviceMemory imageMemory;
                VkImageView imageView;
                VkSampler sampler;
                
                if (engine_->createTextureFromFile(previewPath, image, imageMemory, imageView, sampler)) {
                    // Create descriptor set for the texture
                    VkDescriptorSet descriptorSet;
                    if (engine_->createDescriptorSetForTexture(imageView, sampler, descriptorSet)) {
                        previewTextures_[previewPath] = descriptorSet;
                        previewImages_[previewPath] = image;
                        previewImageMemory_[previewPath] = imageMemory;
                        previewImageViews_[previewPath] = imageView;
                        previewSamplers_[previewPath] = sampler;
                    }
                }
            }
            
            // Display preview image with border and caption
            if (previewTextures_.find(previewPath) != previewTextures_.end()) {
                ImGui::Separator();
                ImGui::Text("Preview:");
                ImGui::BeginChild("PreviewFrame", ImVec2(256, 256), true);
                ImGui::Image(previewTextures_[previewPath], ImVec2(256, 256));
                ImGui::EndChild();
            }
        }
        
        // Load and Delete buttons with improved layout
        ImGui::Separator();
        ImGui::BeginGroup();
        if (ImGui::Button("Load Pattern", ImVec2(120, 0))) {
            VoxelData voxelData;
            PatternMetadata metadata;
            if (engine_->getSaveManager()->loadPattern(pattern.filename, voxelData, metadata)) {
                // Apply pattern to simulation
                engine_->setVoxelData(voxelData);
                engine_->setGridSize(metadata.gridSize);
                engine_->setVoxelSize(metadata.voxelSize);
                engine_->setRuleSet(metadata.ruleSet);
                engine_->resetSimulation();
                showPatternBrowser_ = false;
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Delete Pattern", ImVec2(120, 0))) {
            ImGui::OpenPopup("Delete Pattern");
        }
        ImGui::EndGroup();
        
        if (ImGui::BeginPopupModal("Delete Pattern", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("Are you sure you want to delete this pattern?");
            ImGui::Text("This action cannot be undone.");
            ImGui::Separator();
            
            if (ImGui::Button("Yes", ImVec2(120, 0))) {
                engine_->getSaveManager()->deletePattern(pattern.filename);
                patterns = engine_->getSaveManager()->getPatternFiles();
                selectedPattern = -1;
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("No", ImVec2(120, 0))) {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
    }
    ImGui::EndChild();
    
    ImGui::End();
}

void UI::renderSavePatternDialog() {
    ImGui::Begin("Save Pattern", &showSavePatternDialog_, ImGuiWindowFlags_AlwaysAutoResize);
    
    static char name[256] = "";
    static char description[1024] = "";
    static char author[256] = "";
    static std::vector<std::string> tags;
    static char newTag[64] = "";
    
    ImGui::InputText("Name", name, IM_ARRAYSIZE(name));
    ImGui::InputText("Description", description, IM_ARRAYSIZE(description));
    ImGui::InputText("Author", author, IM_ARRAYSIZE(author));
    
    // Tags
    ImGui::Text("Tags");
    ImGui::SameLine();
    ImGui::InputText("##NewTag", newTag, IM_ARRAYSIZE(newTag));
    ImGui::SameLine();
    if (ImGui::Button("Add")) {
        if (strlen(newTag) > 0) {
            tags.push_back(newTag);
            newTag[0] = '\0';
        }
    }
    
    for (size_t i = 0; i < tags.size(); i++) {
        ImGui::Text("%s", tags[i].c_str());
        ImGui::SameLine();
        if (ImGui::Button(("X##" + std::to_string(i)).c_str())) {
            tags.erase(tags.begin() + i);
            i--;
        }
    }
    
    if (ImGui::Button("Save")) {
        PatternMetadata metadata;
        metadata.name = name;
        metadata.description = description;
        metadata.author = author;
        metadata.version = "1.0";
        metadata.ruleSet = engine_->getRuleSet();
        metadata.gridSize = gridMax;
        metadata.voxelSize = voxelSize;
        metadata.creationTime = std::time(nullptr);
        metadata.modificationTime = std::time(nullptr);
        metadata.population = population;
        metadata.generation = generation;
        metadata.tags = tags;
        
        std::string filename = engine_->getSaveManager()->generatePatternFileName();
        if (engine_->getSaveManager()->savePattern(filename, engine_->getVoxelData(), metadata)) {
            // Generate preview
            std::string previewPath = engine_->getSaveManager()->getPreviewPath(filename);
            engine_->getSaveManager()->generatePreview(filename, previewPath);
            showSavePatternDialog_ = false;
        }
    }
    
    ImGui::SameLine();
    if (ImGui::Button("Cancel")) {
        showSavePatternDialog_ = false;
    }
    
    ImGui::End();
}

void UI::renderLoadPatternDialog() {
    ImGui::Begin("Load Pattern", &showLoadPatternDialog_, ImGuiWindowFlags_AlwaysAutoResize);
    
    static int selectedPattern = -1;
    static std::vector<App::SaveInfo> patterns = engine_->getSaveManager()->getPatternFiles();
    
    ImGui::BeginChild("PatternList", ImVec2(300, 200), true);
    for (int i = 0; i < patterns.size(); i++) {
        if (ImGui::Selectable(patterns[i].name.c_str(), selectedPattern == i)) {
            selectedPattern = i;
        }
    }
    ImGui::EndChild();
    
    if (selectedPattern >= 0 && selectedPattern < patterns.size()) {
        const auto& pattern = patterns[selectedPattern];
        ImGui::Text("Author: %s", pattern.author.c_str());
        ImGui::Text("Description: %s", pattern.description.c_str());
    }
    
    if (ImGui::Button("Load")) {
        if (selectedPattern >= 0 && selectedPattern < patterns.size()) {
            VoxelData voxelData;
            PatternMetadata metadata;
            if (engine_->getSaveManager()->loadPattern(patterns[selectedPattern].filename, voxelData, metadata)) {
                // Apply pattern to simulation
                engine_->setVoxelData(voxelData);
                engine_->setGridSize(metadata.gridSize);
                engine_->setVoxelSize(metadata.voxelSize);
                engine_->setRuleSet(metadata.ruleSet);
                engine_->resetSimulation();
                showLoadPatternDialog_ = false;
            }
        }
    }
    
    ImGui::SameLine();
    if (ImGui::Button("Cancel")) {
        showLoadPatternDialog_ = false;
    }
    
    ImGui::End();
}

void UI::renderRuleAnalysis() {
    if (!showRuleAnalysisWindow_) {
        return;
    }

    ImGui::Begin("Rule Analysis", &showRuleAnalysisWindow_);

    if (!ruleAnalyzer_) {
        ruleAnalyzer_ = std::make_unique<RuleAnalyzer>();
    }

    // Analysis controls
    if (!isAnalyzing_) {
        if (ImGui::Button("Analyze Rules")) {
            startRuleAnalysis();
        }
    } else {
        ImGui::ProgressBar(analysisProgress_, ImVec2(-1, 0));
    }

    // Display results
    if (!analysisResults_.empty()) {
        ImGui::Separator();
        ImGui::Text("Analysis Results");

        for (const auto& result : analysisResults_) {
            if (ImGui::CollapsingHeader(result.ruleName.c_str())) {
                ImGui::Text("Stability: %.2f", result.stability);
                ImGui::Text("Growth Rate: %.2f", result.growthRate);
                ImGui::Text("Complexity: %.2f", result.complexity);

                // Pattern frequencies
                ImGui::Text("Observed Patterns:");
                for (const auto& [pattern, frequency] : result.patternFrequencies) {
                    ImGui::BulletText("%s: %d", pattern.c_str(), frequency);
                }

                // Population history chart
                if (!result.populationHistory.empty()) {
                    ImGui::PlotLines("Population History", 
                        result.populationHistory.data(), 
                        static_cast<int>(result.populationHistory.size()),
                        0, nullptr, 0.0f, 
                        static_cast<float>(engine_->getGridWidth() * 
                                         engine_->getGridHeight() * 
                                         engine_->getGridDepth()),
                        ImVec2(-1, 100));
                }
            }
        }

        if (ImGui::Button("Generate Reports")) {
            generateAnalysisReports();
        }
    }

    ImGui::End();
}

void UI::startRuleAnalysis() {
    isAnalyzing_ = true;
    analysisProgress_ = 0.0f;
    analysisResults_.clear();

    // Get all rule sets
    auto ruleSets = GameRules::getAllRuleSets();
    float progressStep = 1.0f / static_cast<float>(ruleSets.size());

    // Start analysis in a separate thread
    std::thread([this, ruleSets, progressStep]() {
        for (const auto& rule : ruleSets) {
            auto result = ruleAnalyzer_->analyzeRule(
                rule,
                engine_->getGridWidth(),
                engine_->getGridHeight(),
                engine_->getGridDepth()
            );
            analysisResults_.push_back(result);
            analysisProgress_ += progressStep;
        }
        isAnalyzing_ = false;
    }).detach();
}

void UI::updateRuleAnalysis() {
    if (isAnalyzing_) {
        // Update progress bar
        ImGui::ProgressBar(analysisProgress_, ImVec2(-1, 0));
    }
}

void UI::generateAnalysisReports() {
    if (analysisResults_.empty()) {
        return;
    }

    // Create reports directory if it doesn't exist
    std::filesystem::create_directories("reports");

    // Generate individual rule reports
    for (const auto& result : analysisResults_) {
        std::string filename = "reports/rule_" + result.ruleName + "_analysis.html";
        ruleAnalyzer_->generateRuleReport(result, filename);
    }

    // Generate comparison report
    ruleAnalyzer_->generateComparisonReport(analysisResults_, "reports/rule_comparison.html");
}

void UI::cleanupPreviewTextures() {
    for (const auto& [path, descriptorSet] : previewTextures_) {
        // Free descriptor set
        vkFreeDescriptorSets(engine_->getDevice(), engine_->getDescriptorPool(), 1, &descriptorSet);
        
        // Clean up associated resources
        if (previewImages_.find(path) != previewImages_.end()) {
            vkDestroyImage(engine_->getDevice(), previewImages_[path], nullptr);
        }
        if (previewImageMemory_.find(path) != previewImageMemory_.end()) {
            vkFreeMemory(engine_->getDevice(), previewImageMemory_[path], nullptr);
        }
        if (previewImageViews_.find(path) != previewImageViews_.end()) {
            vkDestroyImageView(engine_->getDevice(), previewImageViews_[path], nullptr);
        }
        if (previewSamplers_.find(path) != previewSamplers_.end()) {
            vkDestroySampler(engine_->getDevice(), previewSamplers_[path], nullptr);
        }
    }
    
    // Clear all maps
    previewTextures_.clear();
    previewImages_.clear();
    previewImageMemory_.clear();
    previewImageViews_.clear();
    previewSamplers_.clear();
}
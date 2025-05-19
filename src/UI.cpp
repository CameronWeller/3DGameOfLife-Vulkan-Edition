#include "UI.h"
#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_vulkan.h>
#include <sstream>
#include <iomanip>

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
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
}

void UI::init() {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    
    ImGui::StyleColorsDark();
    
    ImGui_ImplGlfw_InitForVulkan(window, true);
    // Note: ImGui_ImplVulkan_Init will be called after Vulkan device creation
}

void UI::render() {
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    
    handleInput();
    renderStats();
    renderControls();
    
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
    ImGui::Begin("Statistics");
    
    std::stringstream ss;
    ss << "Population: " << population;
    ImGui::Text("%s", ss.str().c_str());
    
    ss.str("");
    ss << "Generation: " << generation;
    ImGui::Text("%s", ss.str().c_str());
    
    ImGui::End();
}

void UI::renderControls() {
    ImGui::Begin("Controls");
    
    // Pause/Resume button
    if (ImGui::Button(isPaused_ ? "Resume" : "Pause")) {
        isPaused_ = !isPaused_;
    }
    
    // Tick rate slider
    ImGui::SliderFloat("Tick Rate", &tickRate, 0.1f, 10.0f, "%.1f Hz");
    
    // Placement mode toggle
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
    
    ImGui::End();
} 
#pragma once

#include <imgui.h>
#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>
#include <memory>
#include <string>
#include <functional>
#include <glm/glm.hpp>
#include "Camera.h"
#include "RayCaster.h"

class VulkanEngine;

class UI {
public:
    UI(VulkanEngine* engine);
    ~UI();

    void init();
    void cleanup();
    void render();
    void update();
    void setPaused(bool paused) { isPaused_ = paused; }
    bool isPaused() const { return isPaused_; }
    
    // Simulation controls
    void setTickRate(float rate) { tickRate = rate; }
    float getTickRate() const { return tickRate; }
    
    // Voxel placement
    void setPlacementMode(bool enabled) { placementMode = enabled; }
    bool isPlacementMode() const { return placementMode; }
    void setPlacementPosition(const glm::vec3& pos) { placementPos = pos; }
    glm::vec3 getPlacementPosition() const { return placementPos; }
    void setGridBounds(const glm::vec3& min, const glm::vec3& max) { gridMin = min; gridMax = max; }
    void setVoxelSize(float size) { voxelSize = size; }
    
    // Callbacks
    void setOnPlaceVoxel(std::function<void(const glm::vec3&)> callback) { onPlaceVoxel = callback; }
    void setOnRemoveVoxel(std::function<void(const glm::vec3&)> callback) { onRemoveVoxel = callback; }
    
    // Statistics
    void setPopulation(uint32_t count) { population = count; }
    void setGeneration(uint32_t count) { generation = count; }
    
private:
    VulkanEngine* engine_;
    bool isPaused_;
    bool showDemoWindow_;
    bool showMetricsWindow_;
    bool showAboutWindow_;
    bool showSettingsWindow_;
    bool showDebugWindow_;
    bool showPerformanceWindow_;
    bool showMemoryWindow_;
    bool showShaderWindow_;
    bool showTextureWindow_;
    bool showModelWindow_;
    bool showCameraWindow_;
    bool showLightWindow_;
    bool showEnvironmentWindow_;
    bool showPostProcessWindow_;
    bool showRenderingWindow_;
    bool showComputeWindow_;
    bool showRayTracingWindow_;
    bool showAnimationWindow_;
    bool showPhysicsWindow_;
    bool showAudioWindow_;
    bool showNetworkWindow_;
    bool showInputWindow_;
    bool showSystemWindow_;
    bool showLogWindow_;
    bool showConsoleWindow_;
    bool showProfilerWindow_;
    bool showDebuggerWindow_;
    bool showInspectorWindow_;
    bool showHierarchyWindow_;
    bool showProjectWindow_;
    bool showAssetWindow_;
    bool showSceneWindow_;
    bool showGameWindow_;
    bool showPreviewWindow_;
    bool showTimelineWindow_;
    bool showAnimationEditorWindow_;
    bool showMaterialEditorWindow_;
    bool showShaderEditorWindow_;
    bool showTextureEditorWindow_;
    bool showModelEditorWindow_;
    bool showCameraEditorWindow_;
    bool showLightEditorWindow_;
    bool showEnvironmentEditorWindow_;
    bool showPostProcessEditorWindow_;
    bool showRenderingEditorWindow_;
    bool showComputeEditorWindow_;
    bool showRayTracingEditorWindow_;
    bool showPhysicsEditorWindow_;
    bool showAudioEditorWindow_;
    bool showNetworkEditorWindow_;
    bool showInputEditorWindow_;
    bool showSystemEditorWindow_;
    bool showLogEditorWindow_;
    bool showConsoleEditorWindow_;
    bool showProfilerEditorWindow_;
    bool showDebuggerEditorWindow_;
    bool showInspectorEditorWindow_;
    bool showHierarchyEditorWindow_;
    bool showProjectEditorWindow_;
    bool showAssetEditorWindow_;
    bool showSceneEditorWindow_;
    bool showGameEditorWindow_;
    bool showPreviewEditorWindow_;
    bool showTimelineEditorWindow_;
    
    float tickRate;
    bool placementMode;
    glm::vec3 placementPos;
    glm::vec3 gridMin;
    glm::vec3 gridMax;
    float voxelSize;
    
    uint32_t population;
    uint32_t generation;
    
    std::function<void(const glm::vec3&)> onPlaceVoxel;
    std::function<void(const glm::vec3&)> onRemoveVoxel;
    
    void handleInput();
    void renderStats();
    void renderControls();
    glm::vec3 getMouseRayDirection();
    void updatePlacementPosition();
}; 
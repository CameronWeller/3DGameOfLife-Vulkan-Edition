#pragma once

#include <imgui.h>
#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>
#include <memory>
#include <string>
#include <functional>
#include <unordered_map>
#include <glm/glm.hpp>
#include "Camera.h"
#include "RayCaster.h"
#include "VoxelData.h"
#include "PatternMetadata.h"
#include "RuleAnalyzer.h"

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
    
    // Performance metrics
    void setPerformanceMetrics(float fps, float frameTime, float updateTime) {
        fps_ = fps;
        frameTime_ = frameTime;
        updateTime_ = updateTime;
    }
    
    void setMemoryUsage(size_t total, size_t used) {
        totalMemory_ = total;
        usedMemory_ = used;
    }
    
    // Rule analysis
    bool showRuleAnalysisWindow_ = false;
    std::unique_ptr<RuleAnalyzer> ruleAnalyzer_;
    std::vector<RuleAnalyzer::AnalysisResult> analysisResults_;
    bool isAnalyzing_ = false;
    float analysisProgress_ = 0.0f;

    void renderRuleAnalysis();
    void startRuleAnalysis();
    void updateRuleAnalysis();
    void generateAnalysisReports();

private:
    VulkanEngine* engine_;
    GLFWwindow* window;
    bool isPaused_;
    
    // Window visibility flags
    bool showStatsWindow_ = true;
    bool showControlsWindow_ = true;
    bool showSettingsWindow_ = false;
    bool showPerformanceWindow_ = false;
    bool showAboutWindow_ = false;
    
    // Pattern management
    bool showPatternBrowser_ = false;
    bool showSavePatternDialog_ = false;
    bool showLoadPatternDialog_ = false;
    
    // Preview textures
    std::unordered_map<std::string, VkDescriptorSet> previewTextures_;
    std::unordered_map<std::string, VkImage> previewImages_;
    std::unordered_map<std::string, VkDeviceMemory> previewImageMemory_;
    std::unordered_map<std::string, VkImageView> previewImageViews_;
    std::unordered_map<std::string, VkSampler> previewSamplers_;
    
    // Performance metrics
    float fps_ = 0.0f;
    float frameTime_ = 0.0f;
    float updateTime_ = 0.0f;
    size_t totalMemory_ = 0;
    size_t usedMemory_ = 0;
    
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
    void renderSettings();
    void renderPerformance();
    void renderAbout();
    glm::vec3 getMouseRayDirection();
    void updatePlacementPosition();
    
    void renderPatternBrowser();
    void renderSavePatternDialog();
    void renderLoadPatternDialog();
    
    void cleanupPreviewTextures();
}; 
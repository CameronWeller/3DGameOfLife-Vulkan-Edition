#include "UI.h"
#include "VulkanEngine.h"
#include "ui/MenuSystem.h"
#include "AppState.h"
#include "Logger.h"
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>
#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>
#include <memory>
#include <string>

namespace VulkanHIP {

UI::UI(VulkanEngine* engine) 
    : engine_(engine), 
      window(nullptr),
      isPaused_(false),
      tickRate(1.0f),
      placementMode(false),
      placementPos(0.0f),
      gridMin(-10.0f),
      gridMax(10.0f),
      voxelSize(1.0f),
      population(0),
      generation(0) {
    
    ruleAnalyzer_ = std::make_unique<RuleAnalyzer>();
}

UI::~UI() {
    cleanup();
}

void UI::init() {
    // Initialize ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
    
    ImGui::StyleColorsDark();
    
    ImGuiStyle& style = ImGui::GetStyle();
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
        style.WindowRounding = 0.0f;
        style.Colors[ImGuiCol_WindowBg].w = 1.0f;
    }
    
    // Get window from engine
    if (engine_) {
        auto* windowManager = engine_->getWindowManager();
        if (windowManager) {
            window = windowManager->getWindow();
            if (window) {
                ImGui_ImplGlfw_InitForVulkan(window, true);
            }
        }
    }
    
    // Initialize Vulkan ImGui
    // Note: If VulkanImGui is already initialized, use its descriptor pool
    // Otherwise, UI needs to set up its own ImGui context
    if (engine_) {
        auto* imgui = engine_->getImGui();
        if (imgui) {
            // VulkanImGui is already initialized, use its descriptor pool
            auto* context = engine_->getVulkanContext();
            auto queueFamilyIndices = context->getQueueFamilyIndices();
            
            ImGui_ImplVulkan_InitInfo init_info = {};
            init_info.Instance = context->getVkInstance();
            init_info.PhysicalDevice = context->getPhysicalDevice();
            init_info.Device = context->getDevice();
            init_info.QueueFamily = queueFamilyIndices.graphicsFamily.value();
            init_info.Queue = context->getGraphicsQueue();
            init_info.PipelineCache = VK_NULL_HANDLE;
            init_info.DescriptorPool = imgui->getDescriptorPool();
            init_info.Subpass = 0;
            init_info.MinImageCount = 2;
            init_info.ImageCount = 2;
            init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
            init_info.Allocator = nullptr;
            init_info.CheckVkResultFn = nullptr;
            
            ImGui_ImplVulkan_Init(&init_info);
        }
        // If VulkanImGui is not available, UI will need to create its own descriptor pool
        // For now, we assume VulkanImGui should be initialized first
    }
}

void UI::cleanup() {
    cleanupPreviewTextures();
    
    if (engine_) {
        ImGui_ImplVulkan_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
    }
}

void UI::render() {
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    
    // Render main menu
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("New Game", "Ctrl+N")) {
                if (engine_) {
                    engine_->resetSimulation();
                    // Reset statistics
                    population = 0;
                    generation = 0;
                }
            }
            if (ImGui::MenuItem("Load Pattern", "Ctrl+O")) {
                showLoadPatternDialog_ = true;
            }
            if (ImGui::MenuItem("Save Pattern", "Ctrl+S")) {
                showSavePatternDialog_ = true;
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Exit", "Alt+F4")) {
                if (window) {
                    glfwSetWindowShouldClose(window, true);
                }
            }
            ImGui::EndMenu();
        }
        
        if (ImGui::BeginMenu("View")) {
            ImGui::MenuItem("Stats", nullptr, &showStatsWindow_);
            ImGui::MenuItem("Controls", nullptr, &showControlsWindow_);
            ImGui::MenuItem("Settings", nullptr, &showSettingsWindow_);
            ImGui::MenuItem("Performance", nullptr, &showPerformanceWindow_);
            ImGui::MenuItem("Rule Analysis", nullptr, &showRuleAnalysisWindow_);
            ImGui::EndMenu();
        }
        
        if (ImGui::BeginMenu("Help")) {
            if (ImGui::MenuItem("About")) {
                showAboutWindow_ = true;
            }
            ImGui::EndMenu();
        }
        
        ImGui::EndMainMenuBar();
    }
    
    // Render windows
    if (showStatsWindow_) renderStats();
    if (showControlsWindow_) renderControls();
    if (showSettingsWindow_) renderSettings();
    if (showPerformanceWindow_) renderPerformance();
    if (showAboutWindow_) renderAbout();
    if (showRuleAnalysisWindow_) renderRuleAnalysis();
    if (showPatternBrowser_) renderPatternBrowser();
    if (showSavePatternDialog_) renderSavePatternDialog();
    if (showLoadPatternDialog_) renderLoadPatternDialog();
    
    ImGui::Render();
}

void UI::update() {
    handleInput();
    updatePlacementPosition();
    
    if (isAnalyzing_) {
        updateRuleAnalysis();
    }
}

void UI::handleInput() {
    // Handle keyboard input
    if (window) {
        if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) {
            isPaused_ = !isPaused_;
        }
        
        if (glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS) {
            if (engine_) {
                engine_->resetSimulation();
                // Reset statistics
                population = 0;
                generation = 0;
            }
        }
    }
}

void UI::renderStats() {
    ImGui::Begin("Statistics", &showStatsWindow_);
    ImGui::Text("Population: %u", population);
    ImGui::Text("Generation: %u", generation);
    ImGui::Text("FPS: %.1f", fps_);
    ImGui::Text("Frame Time: %.2f ms", frameTime_);
    ImGui::Text("Update Time: %.2f ms", updateTime_);
    ImGui::Text("Memory: %zu / %zu MB", usedMemory_ / (1024*1024), totalMemory_ / (1024*1024));
    ImGui::End();
}

void UI::renderControls() {
    ImGui::Begin("Controls", &showControlsWindow_);
    
    if (ImGui::Button(isPaused_ ? "Resume" : "Pause")) {
        isPaused_ = !isPaused_;
    }
    
    ImGui::SameLine();
    if (ImGui::Button("Reset")) {
        if (engine_) {
            engine_->resetSimulation();
            // Reset statistics
            population = 0;
            generation = 0;
        }
    }
    
    ImGui::SliderFloat("Tick Rate", &tickRate, 0.1f, 10.0f);
    
    ImGui::Checkbox("Placement Mode", &placementMode);
    if (placementMode) {
        ImGui::Text("Placement Position: (%.1f, %.1f, %.1f)", 
                   placementPos.x, placementPos.y, placementPos.z);
    }
    
    ImGui::End();
}

void UI::renderSettings() {
    ImGui::Begin("Settings", &showSettingsWindow_);
    
    ImGui::SliderFloat("Voxel Size", &voxelSize, 0.1f, 2.0f);
    
    ImGui::Text("Grid Bounds:");
    ImGui::SliderFloat3("Min", &gridMin.x, -20.0f, 0.0f);
    ImGui::SliderFloat3("Max", &gridMax.x, 0.0f, 20.0f);
    
    ImGui::End();
}

void UI::renderPerformance() {
    ImGui::Begin("Performance", &showPerformanceWindow_);
    
    ImGui::Text("FPS: %.1f", fps_);
    ImGui::Text("Frame Time: %.2f ms", frameTime_);
    ImGui::Text("Update Time: %.2f ms", updateTime_);
    
    ImGui::Separator();
    
    ImGui::Text("Memory Usage:");
    ImGui::Text("Total: %zu MB", totalMemory_ / (1024*1024));
    ImGui::Text("Used: %zu MB", usedMemory_ / (1024*1024));
    ImGui::Text("Free: %zu MB", (totalMemory_ - usedMemory_) / (1024*1024));
    
    float usagePercent = totalMemory_ > 0 ? (float)usedMemory_ / totalMemory_ * 100.0f : 0.0f;
    ImGui::ProgressBar(usagePercent / 100.0f, ImVec2(0, 0), "%.1f%%");
    
    ImGui::End();
}

void UI::renderAbout() {
    ImGui::Begin("About 3D Game of Life", &showAboutWindow_, ImGuiWindowFlags_AlwaysAutoResize);
    ImGui::Text("3D Game of Life - Vulkan Edition");
    ImGui::Text("Version 1.0.0");
    ImGui::Text("Built with Vulkan and ImGui");
    ImGui::Separator();
    ImGui::Text("Controls:");
    ImGui::BulletText("Mouse: Rotate camera");
    ImGui::BulletText("WASD: Move camera");
    ImGui::BulletText("Space: Pause/Resume simulation");
    ImGui::BulletText("R: Reset simulation");
    ImGui::End();
}

void UI::renderRuleAnalysis() {
    ImGui::Begin("Rule Analysis", &showRuleAnalysisWindow_);
    
    if (!isAnalyzing_) {
        if (ImGui::Button("Start Analysis")) {
            startRuleAnalysis();
        }
        
        if (!analysisResults_.empty()) {
            ImGui::Separator();
            ImGui::Text("Analysis Results:");
            
            for (const auto& result : analysisResults_) {
                ImGui::Text("Rule: %s", result.ruleName.c_str());
                ImGui::Text("Stability: %.2f", result.stability);
                ImGui::Text("Complexity: %.2f", result.complexity);
                ImGui::Separator();
            }
        }
    } else {
        ImGui::Text("Analyzing rules...");
        ImGui::ProgressBar(analysisProgress_, ImVec2(0, 0), "%.1f%%");
        
        if (ImGui::Button("Cancel")) {
            isAnalyzing_ = false;
            analysisProgress_ = 0.0f;
        }
    }
    
    ImGui::End();
}

void UI::startRuleAnalysis() {
    isAnalyzing_ = true;
    analysisProgress_ = 0.0f;
    analysisResults_.clear();
}

void UI::updateRuleAnalysis() {
    if (!isAnalyzing_) return;
    
    // Simulate analysis progress
    analysisProgress_ += 0.1f;
    if (analysisProgress_ >= 100.0f) {
        isAnalyzing_ = false;
        analysisProgress_ = 100.0f;
        generateAnalysisReports();
    }
}

void UI::generateAnalysisReports() {
    // TODO: Implement actual rule analysis
    RuleAnalyzer::AnalysisResult result;
    result.ruleName = "Conway's Game of Life";
    result.stability = 0.75f;
    result.complexity = 0.6f;
    analysisResults_.push_back(result);
}

void UI::renderPatternBrowser() {
    ImGui::Begin("Pattern Browser", &showPatternBrowser_);
    ImGui::Text("Pattern browser not implemented yet");
    ImGui::End();
}

void UI::renderSavePatternDialog() {
    ImGui::Begin("Save Pattern", &showSavePatternDialog_);
    ImGui::Text("Save pattern dialog not implemented yet");
    ImGui::End();
}

void UI::renderLoadPatternDialog() {
    ImGui::Begin("Load Pattern", &showLoadPatternDialog_);
    ImGui::Text("Load pattern dialog not implemented yet");
    ImGui::End();
}

glm::vec3 UI::getMouseRayDirection() {
    // TODO: Implement mouse ray casting
    return glm::vec3(0.0f, 0.0f, -1.0f);
}

void UI::updatePlacementPosition() {
    if (!placementMode) return;
    
    // TODO: Implement placement position calculation
    // This would involve ray casting from mouse position to grid
}

void UI::cleanupPreviewTextures() {
    if (!engine_) return;
    
    VkDevice device = engine_->getVulkanContext()->getDevice();
    
    for (auto& [name, sampler] : previewSamplers_) {
        vkDestroySampler(device, sampler, nullptr);
    }
    previewSamplers_.clear();
    
    for (auto& [name, imageView] : previewImageViews_) {
        vkDestroyImageView(device, imageView, nullptr);
    }
    previewImageViews_.clear();
    
    for (auto& [name, memory] : previewImageMemory_) {
        vmaFreeMemory(engine_->getMemoryManager().getAllocator(), memory);
    }
    previewImageMemory_.clear();
    
    for (auto& [name, image] : previewImages_) {
        vmaDestroyImage(engine_->getMemoryManager().getAllocator(), image, VK_NULL_HANDLE);
    }
    previewImages_.clear();
    
    previewTextures_.clear();
}

} // namespace VulkanHIP 
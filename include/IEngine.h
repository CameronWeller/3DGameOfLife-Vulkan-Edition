#pragma once

#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <cstdint>
#include <string>
#include <vector>
#include "GameRules.h"

// Forward declarations - these are complete types when IEngine is used
class WindowManager;
class Camera;
class VoxelData;
struct PatternMetadata;

namespace VulkanHIP {
    class SaveManager;
}

namespace VulkanHIP {
    enum class CameraMode;
}

/**
 * @brief Lightweight interface for engine functionality needed by UI
 * 
 * This interface breaks the monolithic dependency between UI and VulkanEngine,
 * allowing UI to work without needing the complete VulkanEngine definition.
 */
class IEngine {
public:
    virtual ~IEngine() = default;
    
    // Window access
    virtual WindowManager* getWindowManager() const = 0;
    
    // Camera access
    virtual Camera* getCamera() const = 0;
    
    // Save manager access
    virtual VulkanHIP::SaveManager* getSaveManager() const = 0;
    
    // Grid information
    virtual uint32_t getGridWidth() const = 0;
    virtual uint32_t getGridHeight() const = 0;
    virtual uint32_t getGridDepth() const = 0;
    virtual GameRules::RuleSet getRuleSet() const = 0;
    
    // Performance metrics
    virtual float getCurrentFPS() const = 0;
    virtual float getFrameTime() const = 0;
    virtual float getUpdateTime() const = 0;
    virtual size_t getTotalMemory() const = 0;
    virtual size_t getUsedMemory() const = 0;
    
    // Vulkan handles
    virtual VkDevice getDevice() const = 0;
    virtual VkDescriptorPool getDescriptorPool() const = 0;
    
    // Setters
    virtual void setWireframeMode(bool enabled) = 0;
    virtual void setShowGrid(bool enabled) = 0;
    virtual void setTransparency(float transparency) = 0;
    virtual void setRenderMode(int mode) = 0;
    virtual void setCustomRules(int birthMin, int birthMax, int survivalMin, int survivalMax) = 0;
    virtual void setVoxelData(const VoxelData& data) = 0;
    virtual void setGridSize(uint32_t size) = 0;  // Note: VulkanEngine uses uint32_t for cube size
    virtual void setVoxelSize(float size) = 0;
    virtual void setRuleSet(const GameRules::RuleSet& ruleSet) = 0;
    virtual void resetSimulation() = 0;
};


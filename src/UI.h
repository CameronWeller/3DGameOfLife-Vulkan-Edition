#pragma once

#include <GLFW/glfw3.h>
#include <string>
#include <functional>
#include <glm/glm.hpp>
#include "Camera.h"
#include "RayCaster.h"

class UI {
public:
    UI(GLFWwindow* window, Camera* camera);
    ~UI();
    
    void init();
    void render();
    
    // Simulation controls
    void setPaused(bool paused) { isPaused = paused; }
    bool isPaused() const { return isPaused; }
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
    GLFWwindow* window;
    Camera* camera;
    bool isPaused;
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
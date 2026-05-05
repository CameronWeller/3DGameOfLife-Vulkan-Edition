#pragma once

#include <glad/glad.h>
#include <vector>
#include <array>
#include <memory>
#include <string>
#include <cstdint>
#include "../../../include/GameRules.h"
#include "../../../include/PatternManager.h"
#include "compute/OpenGLCompute.h"
#include "rendering/OpenGLVoxelRenderer.h"

namespace OpenGLHIP {

class Grid3D {
public:
    Grid3D(uint32_t width, uint32_t height, uint32_t depth);
    ~Grid3D();

    // Grid management
    void initialize();
    void cleanup();
    void update();
    void resize(uint32_t width, uint32_t height, uint32_t depth);
    
    // Cell operations
    void setCell(uint32_t x, uint32_t y, uint32_t z, bool alive);
    bool getCell(uint32_t x, uint32_t y, uint32_t z) const;
    void clear();
    void randomize(float density = 0.5f);
    
    // Rule set management
    void setRuleSet(const GameRules::RuleSet& ruleSet);
    const GameRules::RuleSet& getCurrentRuleSet() const { return currentRuleSet_; }
    void setRules(const GameRules::RuleSet& rules);
    const GameRules::RuleSet& getRules() const { return rules_; }
    
    // Boundary management
    void setBoundaryType(GameRules::BoundaryType type);
    GameRules::BoundaryType getBoundaryType() const { return boundaryType_; }
    
    // Pattern management
    bool loadPattern(const std::string& filename);
    bool savePattern(const std::string& filename) const;
    ::PatternManager::Pattern getCurrentPattern() const;
    
    // Grid properties
    uint32_t getWidth() const { return width_; }
    uint32_t getHeight() const { return height_; }
    uint32_t getDepth() const { return depth_; }
    uint32_t getTotalCells() const { return width_ * height_ * depth_; }
    
    // Statistics
    uint64_t getGeneration() const { return generation_; }
    uint64_t getPopulation() const { return population_; }
    
    // Compute integration
    OpenGLCompute& getCompute() { return *compute_; }
    const OpenGLCompute& getCompute() const { return *compute_; }
    
    // Rendering integration
    void updateVoxelRenderer(OpenGLVoxelRenderer& renderer) const;
    void syncStateToGPU();
    void syncStateFromGPU();

private:
    // Grid dimensions
    uint32_t width_;
    uint32_t height_;
    uint32_t depth_;
    
    // State tracking
    std::vector<bool> currentState_;
    std::vector<bool> nextState_;
    uint64_t generation_;
    uint64_t population_;
    bool needsStateSync_;
    
    // Rule set and boundary
    GameRules::RuleSet currentRuleSet_;
    GameRules::RuleSet rules_;
    GameRules::BoundaryType boundaryType_;
    
    // OpenGL compute resources
    std::unique_ptr<OpenGLCompute> compute_;
    
    // Initialization helpers
    void createComputeResources();
    void destroyComputeResources();
    
    // Utility functions
    uint32_t getIndex(uint32_t x, uint32_t y, uint32_t z) const;
    void updatePopulation();
    bool isInitialized_;
    bool isValidPosition(uint32_t x, uint32_t y, uint32_t z) const;
    bool getWrappedCell(int x, int y, int z) const;
    
    // Game logic (reused from original)
    void applyRules();
    uint8_t countNeighbors(uint32_t x, uint32_t y, uint32_t z) const;
};

} // namespace OpenGLHIP

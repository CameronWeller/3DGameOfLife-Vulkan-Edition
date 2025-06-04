#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include <array>
#include <memory>
#include "GameRules.h"
#include "PatternManager.h"
#include "VulkanError.h"
#include <vk_mem_alloc.h>
#include <glm/glm.hpp>
#include <string>

namespace VulkanHIP {

struct ComputePushConstants {
    uint32_t width;
    uint32_t height;
    uint32_t depth;
    float time;
    uint32_t ruleSet;  // 0: Classic, 1: HighLife, 2: Day & Night, 3: Custom
};

struct RenderPushConstants {
    glm::mat4 viewProj;
    glm::vec3 cameraPos;
    float voxelSize;
    glm::vec4 frustumPlanes[6];
};

struct LODLevel {
    uint32_t width;
    uint32_t height;
    uint32_t depth;
    VkImage image;
    VkImageView imageView;
    VmaAllocation memory;
};

// Forward declarations
class VulkanEngine;

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
    const GameRules::RuleSet& getCurrentRuleSet() const { return currentRuleSet; }
    void setRules(const GameRules::RuleSet& rules);
    const GameRules::RuleSet& getRules() const { return rules_; }
    
    // Boundary management
    void setBoundaryType(GameRules::BoundaryType type);
    GameRules::BoundaryType getBoundaryType() const { return boundaryType; }
    
    // Pattern management
    bool loadPattern(const std::string& filename);
    bool savePattern(const std::string& filename) const;
    PatternManager::Pattern getCurrentPattern() const;
    
    // Buffer management
    VkBuffer getStateBuffer() const { return stateBuffer; }
    VkBuffer getNextStateBuffer() const { return nextStateBuffer; }
    VkDescriptorSet getDescriptorSet() const { return descriptorSet; }
    
    // Grid properties
    uint32_t getWidth() const { return width; }
    uint32_t getHeight() const { return height; }
    uint32_t getDepth() const { return depth; }
    uint32_t getTotalCells() const { return width * height * depth; }
    
    // Statistics
    uint64_t getGeneration() const { return generation; }
    uint64_t getPopulation() const { return population; }

    // Rendering
    void createRenderResources();
    void destroyRenderResources();
    void render(VkCommandBuffer commandBuffer, const RenderPushConstants& pushConstants);
    void updateLOD(const glm::vec3& cameraPos);

private:
    // Grid dimensions
    uint32_t width;
    uint32_t height;
    uint32_t depth;
    
    // State tracking
    std::vector<bool> currentState;
    std::vector<bool> nextState;
    uint64_t generation;
    uint64_t population;
    bool needsStateSync;
    
    // Rule set and boundary
    GameRules::RuleSet currentRuleSet;
    GameRules::RuleSet rules_;
    GameRules::BoundaryType boundaryType;
    
    // Vulkan resources
    VkBuffer stateBuffer;
    VkBuffer nextStateBuffer;
    VkDeviceMemory stateMemory;
    VkDeviceMemory nextStateMemory;
    VkDescriptorSet descriptorSet;
    VkPipeline computePipeline;
    VkPipelineLayout pipelineLayout;
    
    // Initialization helpers
    void createBuffers();
    void destroyBuffers();
    void createComputeResources();
    void destroyComputeResources();
    void createDescriptorSet();
    void updateDescriptorSet();
    
    // Utility functions
    uint32_t getIndex(uint32_t x, uint32_t y, uint32_t z) const;
    void updatePopulation();
    bool isInitialized;
    bool isValidPosition(uint32_t x, uint32_t y, uint32_t z) const;
    bool getWrappedCell(int x, int y, int z) const;
    
    // Compute resources
    VkCommandBuffer computeCommandBuffer;
    VkDescriptorSetLayout descriptorSetLayout;
    VkDescriptorPool descriptorPool;
    VkPipeline populationPipeline;
    VkBuffer populationBuffer;
    VmaAllocation populationMemory;
    std::vector<uint8_t> computeShaderCode;
    
    void recordComputeCommands();
    
    // State synchronization
    void syncStateToGPU();
    void syncStateFromGPU();

    // Rendering resources
    VkPipeline graphicsPipeline;
    VkPipelineLayout graphicsPipelineLayout;
    VkDescriptorSetLayout renderDescriptorSetLayout;
    VkDescriptorPool renderDescriptorPool;
    VkDescriptorSet renderDescriptorSet;
    VkBuffer vertexBuffer;
    VkBuffer indexBuffer;
    VkBuffer instanceBuffer;
    VmaAllocation vertexMemory;
    VmaAllocation indexMemory;
    VmaAllocation instanceMemory;
    VkImage stateImage;
    VkImageView stateImageView;
    VmaAllocation stateImageMemory;
    VkSampler stateSampler;

    // Rendering methods
    void createGraphicsPipeline();
    void createVertexBuffer();
    void createIndexBuffer();
    void createInstanceBuffer();
    void createStateImage();
    void updateInstanceBuffer();
    void updateStateImage();

    // LOD resources
    std::vector<LODLevel> lodLevels;
    VkBuffer lodBuffer;
    VmaAllocation lodMemory;
    
    // LOD methods
    void createLODResources();
    void destroyLODResources();
    void updateLODTextures();
    void generateMipmaps(VkImage image, uint32_t width, uint32_t height, uint32_t depth);
    
    // Frustum culling
    void updateFrustumPlanes(const glm::mat4& viewProj);
    bool isVisible(const glm::vec3& position, float radius) const;
}; 

} // namespace VulkanHIP 
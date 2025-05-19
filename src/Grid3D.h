#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include <array>
#include <memory>

class Grid3D {
public:
    Grid3D(uint32_t width, uint32_t height, uint32_t depth);
    ~Grid3D();

    // Grid management
    void initialize();
    void update();
    void resize(uint32_t width, uint32_t height, uint32_t depth);
    
    // Cell operations
    void setCell(uint32_t x, uint32_t y, uint32_t z, bool state);
    bool getCell(uint32_t x, uint32_t y, uint32_t z) const;
    void clear();
    
    // Buffer management
    VkBuffer getStateBuffer() const { return stateBuffer; }
    VkBuffer getNextStateBuffer() const { return nextStateBuffer; }
    VkDeviceMemory getStateMemory() const { return stateMemory; }
    VkDeviceMemory getNextStateMemory() const { return nextStateMemory; }
    
    // Grid properties
    uint32_t getWidth() const { return width; }
    uint32_t getHeight() const { return height; }
    uint32_t getDepth() const { return depth; }
    uint32_t getTotalCells() const { return width * height * depth; }
    
    // Statistics
    uint32_t getPopulation() const { return population; }
    uint32_t getGeneration() const { return generation; }

private:
    // Grid dimensions
    uint32_t width;
    uint32_t height;
    uint32_t depth;
    
    // State tracking
    uint32_t population;
    uint32_t generation;
    
    // Vulkan resources
    VkBuffer stateBuffer;
    VkBuffer nextStateBuffer;
    VkDeviceMemory stateMemory;
    VkDeviceMemory nextStateMemory;
    
    // Memory management
    void createBuffers();
    void destroyBuffers();
    void freeMemory();
    
    // Helper functions
    uint32_t getIndex(uint32_t x, uint32_t y, uint32_t z) const;
    bool isValidPosition(uint32_t x, uint32_t y, uint32_t z) const;
    uint32_t countNeighbors(uint32_t x, uint32_t y, uint32_t z) const;
    
    // Compute resources
    VkCommandBuffer computeCommandBuffer;
    VkDescriptorSetLayout descriptorSetLayout;
    VkDescriptorPool descriptorPool;
    VkDescriptorSet descriptorSet;
    VkPipelineLayout pipelineLayout;
    VkPipeline computePipeline;
    VkPipeline populationPipeline;
    VkBuffer populationBuffer;
    VkDeviceMemory populationMemory;
    
    void createComputeResources();
    void destroyComputeResources();
    void recordComputeCommands();
}; 
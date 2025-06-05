#include "Grid3D.h"
#include "VulkanEngine.h"
#include "VulkanMemoryManager.h"
#include "PatternManager.h"
#include <stdexcept>
#include <algorithm>
#include <random>
#include <cstring>

namespace VulkanHIP {

Grid3D::Grid3D(uint32_t width, uint32_t height, uint32_t depth)
    : width(width), height(height), depth(depth),
      population(0), generation(0),
      currentRuleSet(RuleSet::CLASSIC),
      boundaryType(GameRules::BoundaryType::TOROIDAL),
      stateBuffer(VK_NULL_HANDLE), nextStateBuffer(VK_NULL_HANDLE),
      stateMemory(VK_NULL_HANDLE), nextStateMemory(VK_NULL_HANDLE),
      computeCommandBuffer(VK_NULL_HANDLE),
      descriptorSetLayout(VK_NULL_HANDLE),
      descriptorPool(VK_NULL_HANDLE),
      descriptorSet(VK_NULL_HANDLE),
      pipelineLayout(VK_NULL_HANDLE),
      computePipeline(VK_NULL_HANDLE),
      isInitialized(false) {
    
    initialize();
}

Grid3D::~Grid3D() {
    cleanup();
}

void Grid3D::initialize() {
    if (isInitialized) return;
    
    // Initialize state vectors
    currentState.resize(width * height * depth, false);
    nextState.resize(width * height * depth, false);
    
    // Create Vulkan resources
    createBuffers();
    createComputeResources();
    
    isInitialized = true;
}

void Grid3D::cleanup() {
    if (!isInitialized) return;
    
    destroyComputeResources();
    destroyBuffers();
    
    currentState.clear();
    nextState.clear();
    
    isInitialized = false;
}

void Grid3D::createBuffers() {
    VkDevice device = VulkanHIP::VulkanEngine::getInstance()->getVulkanContext()->getDevice();
    auto& memoryManager = VulkanHIP::VulkanEngine::getInstance()->getMemoryManager();
    
    VkDeviceSize bufferSize = sizeof(uint32_t) * width * height * depth;
    
    // Create state buffer
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = bufferSize;
    bufferInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    
    VmaAllocationCreateInfo allocInfo{};
    // Use modern VMA patterns - GPU-only storage buffers for compute
    allocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
    allocInfo.flags = 0; // Device-local, no host access needed
    
    VK_CHECK(vmaCreateBuffer(memoryManager.getAllocator(), &bufferInfo, &allocInfo,
        &stateBuffer, &stateMemory, nullptr));
    
    // Create next state buffer
    VK_CHECK(vmaCreateBuffer(memoryManager.getAllocator(), &bufferInfo, &allocInfo,
        &nextStateBuffer, &nextStateMemory, nullptr));
    
    // Create staging buffer for initial data
    auto stagingBuffer = memoryManager.createStagingBuffer(bufferSize);
    void* data = memoryManager.mapStagingBuffer(stagingBuffer);
    
    // Convert bool vector to uint32_t data for GPU
    std::vector<uint32_t> gpuData(width * height * depth);
    for (size_t i = 0; i < currentState.size(); ++i) {
        gpuData[i] = currentState[i] ? 1 : 0;
    }
    
    memcpy(data, gpuData.data(), bufferSize);
    memoryManager.unmapStagingBuffer(stagingBuffer);
    
    // Copy initial data to state buffer
    VkCommandBuffer commandBuffer = VulkanHIP::VulkanEngine::getInstance()->beginSingleTimeCommands();
    VkBufferCopy copyRegion{};
    copyRegion.size = bufferSize;
    vkCmdCopyBuffer(commandBuffer, stagingBuffer.buffer, stateBuffer, 1, &copyRegion);
    VulkanHIP::VulkanEngine::getInstance()->endSingleTimeCommands(commandBuffer);
    
    // Clean up staging buffer
    memoryManager.destroyStagingBuffer(stagingBuffer);
}

void Grid3D::destroyBuffers() {
    if (stateBuffer != VK_NULL_HANDLE) {
        vmaDestroyBuffer(VulkanHIP::VulkanEngine::getInstance()->getMemoryManager().getAllocator(), stateBuffer, stateMemory);
        stateBuffer = VK_NULL_HANDLE;
        stateMemory = VK_NULL_HANDLE;
    }
    
    if (nextStateBuffer != VK_NULL_HANDLE) {
        vmaDestroyBuffer(VulkanHIP::VulkanEngine::getInstance()->getMemoryManager().getAllocator(), nextStateBuffer, nextStateMemory);
        nextStateBuffer = VK_NULL_HANDLE;
        nextStateMemory = VK_NULL_HANDLE;
    }
}

void Grid3D::createComputeResources() {
    VkDevice device = VulkanHIP::VulkanEngine::getInstance()->getVulkanContext()->getDevice();
    
    // Create descriptor set layout
    VkDescriptorSetLayoutBinding stateBufferBinding{};
    stateBufferBinding.binding = 0;
    stateBufferBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    stateBufferBinding.descriptorCount = 1;
    stateBufferBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    
    VkDescriptorSetLayoutBinding nextStateBufferBinding{};
    nextStateBufferBinding.binding = 1;
    nextStateBufferBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    nextStateBufferBinding.descriptorCount = 1;
    nextStateBufferBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    
    std::array<VkDescriptorSetLayoutBinding, 2> bindings = {stateBufferBinding, nextStateBufferBinding};
    
    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    layoutInfo.pBindings = bindings.data();
    
    VK_CHECK(vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &descriptorSetLayout));
    
    // Create descriptor pool
    std::array<VkDescriptorPoolSize, 1> poolSizes{};
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    poolSizes[0].descriptorCount = 2;
    
    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();
    poolInfo.maxSets = 1;
    
    VK_CHECK(vkCreateDescriptorPool(device, &poolInfo, nullptr, &descriptorPool));
    
    // Create descriptor set
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = descriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &descriptorSetLayout;
    
    VK_CHECK(vkAllocateDescriptorSets(device, &allocInfo, &descriptorSet));
    
    // Update descriptor set
    VkDescriptorBufferInfo stateBufferInfo{};
    stateBufferInfo.buffer = stateBuffer;
    stateBufferInfo.offset = 0;
    stateBufferInfo.range = VK_WHOLE_SIZE;
    
    VkDescriptorBufferInfo nextStateBufferInfo{};
    nextStateBufferInfo.buffer = nextStateBuffer;
    nextStateBufferInfo.offset = 0;
    nextStateBufferInfo.range = VK_WHOLE_SIZE;
    
    std::array<VkWriteDescriptorSet, 2> descriptorWrites{};
    
    descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[0].dstSet = descriptorSet;
    descriptorWrites[0].dstBinding = 0;
    descriptorWrites[0].dstArrayElement = 0;
    descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    descriptorWrites[0].descriptorCount = 1;
    descriptorWrites[0].pBufferInfo = &stateBufferInfo;
    
    descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[1].dstSet = descriptorSet;
    descriptorWrites[1].dstBinding = 1;
    descriptorWrites[1].dstArrayElement = 0;
    descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    descriptorWrites[1].descriptorCount = 1;
    descriptorWrites[1].pBufferInfo = &nextStateBufferInfo;
    
    vkUpdateDescriptorSets(device, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
    
    // Create pipeline layout
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &descriptorSetLayout;
    
    // Add push constants for grid dimensions and rule set
    VkPushConstantRange pushConstantRange{};
    pushConstantRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    pushConstantRange.offset = 0;
    pushConstantRange.size = sizeof(ComputePushConstants);
    
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;
    
    VK_CHECK(vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipelineLayout));
    
    // Create compute pipeline
    auto computeStage = VulkanHIP::VulkanEngine::getInstance()->getShaderManager()->createComputeStage(
        "shaders/game_of_life_3d.comp.spv");
    
    VkComputePipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineInfo.stage = computeStage;
    pipelineInfo.layout = pipelineLayout;
    
    VK_CHECK(vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &computePipeline));
    
    // Create command buffer for compute operations
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    
    // Fix: Use the correct way to get compute queue family index
    auto queueFamilies = VulkanHIP::VulkanEngine::getInstance()->getVulkanContext()->getQueueFamilyIndices();
    if (queueFamilies.hasCompute()) {
        poolInfo.queueFamilyIndex = queueFamilies.computeFamily.value();
    } else {
        // Fallback to graphics queue family if no dedicated compute queue
        poolInfo.queueFamilyIndex = queueFamilies.graphicsFamily.value();
    }
    
    VkCommandPool computeCommandPool;
    VK_CHECK(vkCreateCommandPool(device, &poolInfo, nullptr, &computeCommandPool));
    
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = computeCommandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1;
    
    VK_CHECK(vkAllocateCommandBuffers(device, &allocInfo, &computeCommandBuffer));
    
    // Record compute commands
    recordComputeCommands();
}

void Grid3D::destroyComputeResources() {
    VkDevice device = VulkanHIP::VulkanEngine::getInstance()->getVulkanContext()->getDevice();
    
    if (computePipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(device, computePipeline, nullptr);
        computePipeline = VK_NULL_HANDLE;
    }
    
    if (pipelineLayout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
        pipelineLayout = VK_NULL_HANDLE;
    }
    
    if (descriptorPool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(device, descriptorPool, nullptr);
        descriptorPool = VK_NULL_HANDLE;
    }
    
    if (descriptorSetLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);
        descriptorSetLayout = VK_NULL_HANDLE;
    }
}

void Grid3D::update() {
    if (!isInitialized) return;
    
    VkDevice device = VulkanHIP::VulkanEngine::getInstance()->getVulkanContext()->getDevice();
    VkCommandBuffer commandBuffer = VulkanHIP::VulkanEngine::getInstance()->beginSingleTimeCommands();
    
    // Bind compute pipeline
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, computePipeline);
    
    // Bind descriptor set
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);
    
    // Push constants
    ComputePushConstants pushConstants{};
    pushConstants.width = width;
    pushConstants.height = height;
    pushConstants.depth = depth;
    pushConstants.time = static_cast<float>(glfwGetTime());
    
    vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(ComputePushConstants), &pushConstants);
    
    // Dispatch compute shader
    uint32_t groupSizeX = 8;
    uint32_t groupSizeY = 8;
    uint32_t groupSizeZ = 8;
    
    uint32_t dispatchX = (width + groupSizeX - 1) / groupSizeX;
    uint32_t dispatchY = (height + groupSizeY - 1) / groupSizeY;
    uint32_t dispatchZ = (depth + groupSizeZ - 1) / groupSizeZ;
    
    vkCmdDispatch(commandBuffer, dispatchX, dispatchY, dispatchZ);
    
    // Add memory barrier to ensure compute shader has finished
    VkMemoryBarrier memoryBarrier{};
    memoryBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    memoryBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    memoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    
    vkCmdPipelineBarrier(
        commandBuffer,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        0,
        1, &memoryBarrier,
        0, nullptr,
        0, nullptr
    );
    
    // End command buffer
    VulkanHIP::VulkanEngine::getInstance()->endSingleTimeCommands(commandBuffer);
    
    // Swap buffers
    std::swap(stateBuffer, nextStateBuffer);
    std::swap(stateMemory, nextStateMemory);
    
    // Update descriptor set with new buffer bindings
    VkDescriptorBufferInfo stateBufferInfo{};
    stateBufferInfo.buffer = stateBuffer;
    stateBufferInfo.offset = 0;
    stateBufferInfo.range = VK_WHOLE_SIZE;
    
    VkDescriptorBufferInfo nextStateBufferInfo{};
    nextStateBufferInfo.buffer = nextStateBuffer;
    nextStateBufferInfo.offset = 0;
    nextStateBufferInfo.range = VK_WHOLE_SIZE;
    
    std::array<VkWriteDescriptorSet, 2> descriptorWrites{};
    
    descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[0].dstSet = descriptorSet;
    descriptorWrites[0].dstBinding = 0;
    descriptorWrites[0].dstArrayElement = 0;
    descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    descriptorWrites[0].descriptorCount = 1;
    descriptorWrites[0].pBufferInfo = &stateBufferInfo;
    
    descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[1].dstSet = descriptorSet;
    descriptorWrites[1].dstBinding = 1;
    descriptorWrites[1].dstArrayElement = 0;
    descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    descriptorWrites[1].descriptorCount = 1;
    descriptorWrites[1].pBufferInfo = &nextStateBufferInfo;
    
    vkUpdateDescriptorSets(device, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
    
    // Update generation counter
    generation++;
    
    // Optionally sync state back to CPU if needed
    if (needsStateSync) {
        syncStateFromGPU();
        needsStateSync = false;
    }
}

void Grid3D::setCell(uint32_t x, uint32_t y, uint32_t z, bool state) {
    if (x >= width || y >= height || z >= depth) return;
    
    uint32_t index = getIndex(x, y, z);
    bool currentCellState = currentState[index];
    
    if (currentCellState != state) {
        currentState[index] = state;
        population += state ? 1 : -1;
        
        // Update GPU buffer
        VkDevice device = VulkanHIP::VulkanEngine::getInstance()->getVulkanContext()->getDevice();
        auto& memoryManager = VulkanHIP::VulkanEngine::getInstance()->getMemoryManager();
        
        // Create staging buffer for the update
        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = sizeof(uint32_t);
        bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        
        VmaAllocationCreateInfo allocInfo{};
        // Use modern VMA patterns - staging buffer for CPU-to-GPU transfer
        allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
        allocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | 
                          VMA_ALLOCATION_CREATE_HOST_ACCESS_ALLOW_TRANSFER_INSTEAD_BIT |
                          VMA_ALLOCATION_CREATE_MAPPED_BIT;
        
        VkBuffer stagingBuffer;
        VmaAllocation stagingMemory;
        VK_CHECK(vmaCreateBuffer(memoryManager.getAllocator(), &bufferInfo, &allocInfo,
            &stagingBuffer, &stagingMemory, nullptr));
        
        // Copy data to staging buffer
        void* data;
        vmaMapMemory(memoryManager.getAllocator(), stagingMemory, &data);
        uint32_t value = state ? 1 : 0;
        memcpy(data, &value, sizeof(uint32_t));
        vmaUnmapMemory(memoryManager.getAllocator(), stagingMemory);
        
        // Copy to device buffer
        VkCommandBuffer commandBuffer = VulkanHIP::VulkanEngine::getInstance()->beginSingleTimeCommands();
        
        VkBufferCopy copyRegion{};
        copyRegion.srcOffset = 0;
        copyRegion.dstOffset = index * sizeof(uint32_t);
        copyRegion.size = sizeof(uint32_t);
        
        vkCmdCopyBuffer(commandBuffer, stagingBuffer, stateBuffer, 1, &copyRegion);
        
        VulkanHIP::VulkanEngine::getInstance()->endSingleTimeCommands(commandBuffer);
        
        // Cleanup staging buffer
        vmaDestroyBuffer(memoryManager.getAllocator(), stagingBuffer, stagingMemory);
    }
}

bool Grid3D::getCell(uint32_t x, uint32_t y, uint32_t z) const {
    if (x >= width || y >= height || z >= depth) return false;
    return currentState[getIndex(x, y, z)];
}

void Grid3D::clear() {
    std::fill(currentState.begin(), currentState.end(), false);
    population = 0;
    syncStateToGPU();
}

void Grid3D::resize(uint32_t newWidth, uint32_t newHeight, uint32_t newDepth) {
    width = newWidth;
    height = newHeight;
    depth = newDepth;
    
    currentState.resize(width * height * depth, false);
    nextState.resize(width * height * depth, false);
    
    initialize();
}

void Grid3D::setRuleSet(RuleSet ruleSet) {
    if (currentRuleSet == ruleSet) return;
    
    currentRuleSet = ruleSet;
    
    // Update push constants with new rule set
    ComputePushConstants pushConstants{};
    pushConstants.width = width;
    pushConstants.height = height;
    pushConstants.depth = depth;
    pushConstants.time = static_cast<float>(glfwGetTime());
    pushConstants.ruleSet = static_cast<uint32_t>(ruleSet);
    
    // Update push constants in the compute pipeline
    VkCommandBuffer commandBuffer = VulkanHIP::VulkanEngine::getInstance()->beginSingleTimeCommands();
    
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, computePipeline);
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);
    vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(ComputePushConstants), &pushConstants);
    
    // Dispatch a single compute shader invocation to update the rule set
    vkCmdDispatch(commandBuffer, 1, 1, 1);
    
    VulkanHIP::VulkanEngine::getInstance()->endSingleTimeCommands(commandBuffer);
    
    // Force a state sync to ensure the new rules are applied
    needsStateSync = true;
}

void Grid3D::setBoundaryType(GameRules::BoundaryType type) {
    boundaryType = type;
}

uint32_t Grid3D::getIndex(uint32_t x, uint32_t y, uint32_t z) const {
    return z * width * height + y * width + x;
}

bool Grid3D::isValidPosition(uint32_t x, uint32_t y, uint32_t z) const {
    return x < width && y < height && z < depth;
}

bool Grid3D::getWrappedCell(int32_t x, int32_t y, int32_t z) const {
    switch (boundaryType) {
        case GameRules::BoundaryType::TOROIDAL:
            // Wrap coordinates
            x = (x + width) % width;
            y = (y + height) % height;
            z = (z + depth) % depth;
            return currentState[getIndex(x, y, z)];
            
        case GameRules::BoundaryType::MIRROR:
            // Mirror coordinates
            if (x < 0) x = -x;
            if (y < 0) y = -y;
            if (z < 0) z = -z;
            if (x >= width) x = 2 * width - x - 1;
            if (y >= height) y = 2 * height - y - 1;
            if (z >= depth) z = 2 * depth - z - 1;
            return currentState[getIndex(x, y, z)];
            
        case GameRules::BoundaryType::FIXED:
        case GameRules::BoundaryType::INFINITE:
        default:
            return false;
    }
}

uint32_t Grid3D::countNeighbors(uint32_t x, uint32_t y, uint32_t z) const {
    uint32_t count = 0;
    
    // Check all 26 neighbors in 3D space
    for (int32_t dz = -1; dz <= 1; dz++) {
        for (int32_t dy = -1; dy <= 1; dy++) {
            for (int32_t dx = -1; dx <= 1; dx++) {
                // Skip the center cell
                if (dx == 0 && dy == 0 && dz == 0) continue;
                
                if (getWrappedCell(x + dx, y + dy, z + dz)) {
                    count++;
                }
            }
        }
    }
    
    return count;
}

void Grid3D::syncStateToGPU() {
    VkDevice device = VulkanHIP::VulkanEngine::getInstance()->getVulkanContext()->getDevice();
    auto& memoryManager = VulkanHIP::VulkanEngine::getInstance()->getMemoryManager();
    
    // Create staging buffer
    VkDeviceSize bufferSize = sizeof(uint32_t) * width * height * depth;
    auto stagingBuffer = memoryManager.createStagingBuffer(bufferSize);
    
    // Copy data to staging buffer
    void* data = memoryManager.mapStagingBuffer(stagingBuffer);
    std::vector<uint32_t> gpuData(currentState.size());
    for (size_t i = 0; i < currentState.size(); i++) {
        gpuData[i] = currentState[i] ? 1 : 0;
    }
    memcpy(data, gpuData.data(), bufferSize);
    memoryManager.unmapStagingBuffer(stagingBuffer);
    
    // Copy to device buffer
    VkCommandBuffer commandBuffer = VulkanHIP::VulkanEngine::getInstance()->beginSingleTimeCommands();
    
    VkBufferCopy copyRegion{};
    copyRegion.size = bufferSize;
    
    vkCmdCopyBuffer(commandBuffer, stagingBuffer.buffer, stateBuffer, 1, &copyRegion);
    
    VulkanHIP::VulkanEngine::getInstance()->endSingleTimeCommands(commandBuffer);
    
    // Cleanup staging buffer
    memoryManager.destroyStagingBuffer(stagingBuffer);
}

void Grid3D::syncStateFromGPU() {
    VkDevice device = VulkanHIP::VulkanEngine::getInstance()->getVulkanContext()->getDevice();
    auto& memoryManager = VulkanHIP::VulkanEngine::getInstance()->getMemoryManager();
    
    // Create staging buffer
    VkDeviceSize bufferSize = sizeof(uint32_t) * width * height * depth;
    auto stagingBuffer = memoryManager.createStagingBuffer(bufferSize);
    
    // Copy from device buffer to staging buffer
    VkCommandBuffer commandBuffer = VulkanHIP::VulkanEngine::getInstance()->beginSingleTimeCommands();
    
    VkBufferCopy copyRegion{};
    copyRegion.size = bufferSize;
    
    vkCmdCopyBuffer(commandBuffer, stateBuffer, stagingBuffer.buffer, 1, &copyRegion);
    
    VulkanHIP::VulkanEngine::getInstance()->endSingleTimeCommands(commandBuffer);
    
    // Map staging buffer and copy to CPU state
    void* data = memoryManager.mapStagingBuffer(stagingBuffer);
    std::vector<uint32_t> gpuData(width * height * depth);
    memcpy(gpuData.data(), data, bufferSize);
    memoryManager.unmapStagingBuffer(stagingBuffer);
    
    // Update CPU state
    population = 0;
    for (size_t i = 0; i < currentState.size(); i++) {
        currentState[i] = gpuData[i] != 0;
        if (currentState[i]) population++;
    }
    
    // Cleanup staging buffer
    memoryManager.destroyStagingBuffer(stagingBuffer);
}

bool Grid3D::loadPattern(const std::string& filename) {
    // Resize grid if needed
    if (width != pattern.width || height != pattern.height || depth != pattern.depth) {
        resize(pattern.width, pattern.height, pattern.depth);
    }
    
    // Copy cell data
    currentState = pattern.cells;
    nextState = pattern.cells;
    
    // Update rule set and boundary type
    currentRuleSet = static_cast<RuleSet>(pattern.ruleSet);
    boundaryType = pattern.boundaryType;
    
    // Reset statistics
    population = std::count(currentState.begin(), currentState.end(), true);
    generation = 0;
    
    // Sync to GPU
    syncStateToGPU();
    
    return true;
}

bool Grid3D::savePattern(const std::string& filename) const {
    PatternManager::Pattern pattern(
        "Current State",
        "Saved pattern from simulation",
        width, height, depth,
        currentState,
        0, // ruleSet index - will need to map GameRules::RuleSet to uint32_t
        static_cast<uint32_t>(boundaryType)
    );
    
    return PatternManager::savePattern(filename, pattern);
}

PatternManager::Pattern Grid3D::getCurrentPattern() const {
    return PatternManager::Pattern(
        "Current State",
        "Current simulation state",
        width, height, depth,
        currentState,
        0, // ruleSet index - will need to map GameRules::RuleSet to uint32_t
        static_cast<uint32_t>(boundaryType)
    );
}

void Grid3D::randomize(float density) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> dis(0.0f, 1.0f);
    
    population = 0;
    for (uint32_t i = 0; i < currentState.size(); i++) {
        bool state = dis(gen) < density; // Use provided density
        currentState[i] = state;
        if (state) population++;
    }
    
    syncStateToGPU();
}

void Grid3D::setRules(const GameRules::RuleSet& rules) {
    rules_ = rules;
}

void Grid3D::createLODResources() {
    VkDevice device = VulkanHIP::VulkanEngine::getInstance()->getVulkanContext()->getDevice();
    auto& memoryManager = VulkanHIP::VulkanEngine::getInstance()->getMemoryManager();
    
    // Create LOD levels
    uint32_t currentWidth = width;
    uint32_t currentHeight = height;
    uint32_t currentDepth = depth;
    
    for (int i = 0; i < 4; i++) {
        LODLevel level;
        level.width = currentWidth;
        level.height = currentHeight;
        level.depth = currentDepth;
        
        // Create 3D image for this LOD level
        VkImageCreateInfo imageInfo{};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_3D;
        imageInfo.extent.width = currentWidth;
        imageInfo.extent.height = currentHeight;
        imageInfo.extent.depth = currentDepth;
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 1;
        imageInfo.format = VK_FORMAT_R8_UNORM;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        
        VmaAllocationCreateInfo allocInfo{};
        // Use modern VMA patterns - GPU-only texture images
        allocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
        allocInfo.flags = 0; // Device-local, no host access needed for textures
        
        VK_CHECK(vmaCreateImage(memoryManager.getAllocator(), &imageInfo, &allocInfo,
            &level.image, &level.memory, nullptr));
        
        // Create image view
        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = level.image;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_3D;
        viewInfo.format = imageInfo.format;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;
        
        VK_CHECK(vkCreateImageView(device, &viewInfo, nullptr, &level.imageView));
        
        lodLevels.push_back(level);
        
        // Halve dimensions for next level
        currentWidth = std::max(1u, currentWidth / 2);
        currentHeight = std::max(1u, currentHeight / 2);
        currentDepth = std::max(1u, currentDepth / 2);
    }
    
    // Create LOD buffer for instance data
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = width * height * depth * sizeof(float);
    bufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    
    VmaAllocationCreateInfo allocInfo{};
    // Use modern VMA patterns - dynamic vertex buffer with CPU updates
    allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
    allocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                      VMA_ALLOCATION_CREATE_HOST_ACCESS_ALLOW_TRANSFER_INSTEAD_BIT |
                      VMA_ALLOCATION_CREATE_MAPPED_BIT;
    
    VK_CHECK(vmaCreateBuffer(memoryManager.getAllocator(), &bufferInfo, &allocInfo,
        &lodBuffer, &lodMemory, nullptr));
    
    updateLODTextures();
}

void Grid3D::destroyLODResources() {
    VkDevice device = VulkanHIP::VulkanEngine::getInstance()->getVulkanContext()->getDevice();
    auto& memoryManager = VulkanHIP::VulkanEngine::getInstance()->getMemoryManager();
    
    for (auto& level : lodLevels) {
        if (level.imageView != VK_NULL_HANDLE) {
            vkDestroyImageView(device, level.imageView, nullptr);
        }
        if (level.image != VK_NULL_HANDLE) {
            vmaDestroyImage(memoryManager.getAllocator(), level.image, level.memory);
        }
    }
    lodLevels.clear();
    
    if (lodBuffer != VK_NULL_HANDLE) {
        vmaDestroyBuffer(memoryManager.getAllocator(), lodBuffer, lodMemory);
        lodBuffer = VK_NULL_HANDLE;
        lodMemory = VK_NULL_HANDLE;
    }
}

void Grid3D::updateLODTextures() {
    VkDevice device = VulkanHIP::VulkanEngine::getInstance()->getVulkanContext()->getDevice();
    auto& memoryManager = VulkanHIP::VulkanEngine::getInstance()->getMemoryManager();
    
    // Create staging buffer
    VkDeviceSize bufferSize = width * height * depth;
    auto stagingBuffer = memoryManager.createStagingBuffer(bufferSize);
    
    // Copy data to staging buffer
    void* data = memoryManager.mapStagingBuffer(stagingBuffer);
    std::vector<uint8_t> imageData(bufferSize);
    for (size_t i = 0; i < currentState.size(); i++) {
        imageData[i] = currentState[i] ? 255 : 0;
    }
    memcpy(data, imageData.data(), bufferSize);
    memoryManager.unmapStagingBuffer(stagingBuffer);
    
    // Copy to each LOD level
    for (auto& level : lodLevels) {
        VkCommandBuffer commandBuffer = VulkanHIP::VulkanEngine::getInstance()->beginSingleTimeCommands();
        
        // Transition image layout
        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = level.image;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        
        vkCmdPipelineBarrier(commandBuffer,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            0,
            0, nullptr,
            0, nullptr,
            1, &barrier);
        
        // Copy buffer to image
        VkBufferImageCopy region{};
        region.bufferOffset = 0;
        region.bufferRowLength = 0;
        region.bufferImageHeight = 0;
        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.mipLevel = 0;
        region.imageSubresource.baseArrayLayer = 0;
        region.imageSubresource.layerCount = 1;
        region.imageOffset = {0, 0, 0};
        region.imageExtent = {level.width, level.height, level.depth};
        
        vkCmdCopyBufferToImage(commandBuffer, stagingBuffer.buffer, level.image,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
        
        // Generate mipmaps
        generateMipmaps(level.image, level.width, level.height, level.depth);
        
        // Transition image layout for shader access
        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        
        vkCmdPipelineBarrier(commandBuffer,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            0,
            0, nullptr,
            0, nullptr,
            1, &barrier);
        
        VulkanHIP::VulkanEngine::getInstance()->endSingleTimeCommands(commandBuffer);
    }
    
    // Cleanup staging buffer
    memoryManager.destroyStagingBuffer(stagingBuffer);
}

void Grid3D::generateMipmaps(VkImage image, uint32_t width, uint32_t height, uint32_t depth) {
    VkCommandBuffer commandBuffer = VulkanHIP::VulkanEngine::getInstance()->beginSingleTimeCommands();
    
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.image = image;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    barrier.subresourceRange.levelCount = 1;
    
    int32_t mipWidth = width;
    int32_t mipHeight = height;
    int32_t mipDepth = depth;
    
    for (uint32_t i = 1; i < 4; i++) {
        barrier.subresourceRange.baseMipLevel = i - 1;
        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        
        vkCmdPipelineBarrier(commandBuffer,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            0,
            0, nullptr,
            0, nullptr,
            1, &barrier);
        
        VkImageBlit blit{};
        blit.srcOffsets[0] = {0, 0, 0};
        blit.srcOffsets[1] = {mipWidth, mipHeight, mipDepth};
        blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        blit.srcSubresource.mipLevel = i - 1;
        blit.srcSubresource.baseArrayLayer = 0;
        blit.srcSubresource.layerCount = 1;
        blit.dstOffsets[0] = {0, 0, 0};
        blit.dstOffsets[1] = {mipWidth > 1 ? mipWidth / 2 : 1, mipHeight > 1 ? mipHeight / 2 : 1, mipDepth > 1 ? mipDepth / 2 : 1};
        blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        blit.dstSubresource.mipLevel = i;
        blit.dstSubresource.baseArrayLayer = 0;
        blit.dstSubresource.layerCount = 1;
        
        vkCmdBlitImage(commandBuffer,
            image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1, &blit,
            VK_FILTER_LINEAR);
        
        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        
        vkCmdPipelineBarrier(commandBuffer,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            0,
            0, nullptr,
            0, nullptr,
            1, &barrier);
        
        if (mipWidth > 1) mipWidth /= 2;
        if (mipHeight > 1) mipHeight /= 2;
        if (mipDepth > 1) mipDepth /= 2;
    }
    
    barrier.subresourceRange.baseMipLevel = 3;
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    
    vkCmdPipelineBarrier(commandBuffer,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        0,
        0, nullptr,
        0, nullptr,
        1, &barrier);
    
    VulkanHIP::VulkanEngine::getInstance()->endSingleTimeCommands(commandBuffer);
}

void Grid3D::updateLOD(const glm::vec3& cameraPos) {
    VkDevice device = VulkanHIP::VulkanEngine::getInstance()->getVulkanContext()->getDevice();
    auto& memoryManager = VulkanHIP::VulkanEngine::getInstance()->getMemoryManager();
    
    void* data;
    vmaMapMemory(memoryManager.getAllocator(), lodMemory, &data);
    
    std::vector<float> lodData(width * height * depth);
    
    for (uint32_t z = 0; z < depth; z++) {
        for (uint32_t y = 0; y < height; y++) {
            for (uint32_t x = 0; x < width; x++) {
                glm::vec3 pos(x, y, z);
                float dist = glm::length(cameraPos - pos);
                float lod = std::min(3.0f, std::floor(std::log2(dist / voxelSize)));
                lodData[z * width * height + y * width + x] = lod;
            }
        }
    }
    
    memcpy(data, lodData.data(), lodData.size() * sizeof(float));
    vmaUnmapMemory(memoryManager.getAllocator(), lodMemory);
}

void Grid3D::updateFrustumPlanes(const glm::mat4& viewProj) {
    // Extract frustum planes from view-projection matrix
    
    // Left plane
    frustumPlanes[0] = glm::vec4(
        viewProj[0][3] + viewProj[0][0],
        viewProj[1][3] + viewProj[1][0],
        viewProj[2][3] + viewProj[2][0],
        viewProj[3][3] + viewProj[3][0]
    );
    
    // Right plane
    frustumPlanes[1] = glm::vec4(
        viewProj[0][3] - viewProj[0][0],
        viewProj[1][3] - viewProj[1][0],
        viewProj[2][3] - viewProj[2][0],
        viewProj[3][3] - viewProj[3][0]
    );
    
    // Bottom plane
    frustumPlanes[2] = glm::vec4(
        viewProj[0][3] + viewProj[0][1],
        viewProj[1][3] + viewProj[1][1],
        viewProj[2][3] + viewProj[2][1],
        viewProj[3][3] + viewProj[3][1]
    );
    
    // Top plane
    frustumPlanes[3] = glm::vec4(
        viewProj[0][3] - viewProj[0][1],
        viewProj[1][3] - viewProj[1][1],
        viewProj[2][3] - viewProj[2][1],
        viewProj[3][3] - viewProj[3][1]
    );
    
    // Near plane
    frustumPlanes[4] = glm::vec4(
        viewProj[0][3] + viewProj[0][2],
        viewProj[1][3] + viewProj[1][2],
        viewProj[2][3] + viewProj[2][2],
        viewProj[3][3] + viewProj[3][2]
    );
    
    // Far plane
    frustumPlanes[5] = glm::vec4(
        viewProj[0][3] - viewProj[0][2],
        viewProj[1][3] - viewProj[1][2],
        viewProj[2][3] - viewProj[2][2],
        viewProj[3][3] - viewProj[3][2]
    );
    
    // Normalize planes
    for (int i = 0; i < 6; i++) {
        float length = glm::length(glm::vec3(frustumPlanes[i]));
        frustumPlanes[i] /= length;
    }
}

bool Grid3D::isVisible(const glm::vec3& position, float radius) const {
    for (int i = 0; i < 6; i++) {
        if (glm::dot(glm::vec4(position, 1.0f), frustumPlanes[i]) + radius < 0.0f) {
            return false;
        }
    }
    return true;
}

void Grid3D::recordComputeCommands() {
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
    
    VK_CHECK(vkBeginCommandBuffer(computeCommandBuffer, &beginInfo));
    
    vkCmdBindPipeline(computeCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, computePipeline);
    vkCmdBindDescriptorSets(computeCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE,
        pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);
    
    ComputePushConstants pushConstants{};
    pushConstants.width = width;
    pushConstants.height = height;
    pushConstants.depth = depth;
    pushConstants.time = 0.0f;  // Updated during update()
    pushConstants.ruleSet = 0; // Will map GameRules::RuleSet to uint32_t later
    
    vkCmdPushConstants(computeCommandBuffer, pipelineLayout,
        VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(pushConstants), &pushConstants);
    
    // Dispatch compute shader
    uint32_t groupsX = (width + 7) / 8;
    uint32_t groupsY = (height + 7) / 8;
    uint32_t groupsZ = (depth + 7) / 8;
    vkCmdDispatch(computeCommandBuffer, groupsX, groupsY, groupsZ);
    
    VK_CHECK(vkEndCommandBuffer(computeCommandBuffer));
}

} // namespace VulkanHIP 
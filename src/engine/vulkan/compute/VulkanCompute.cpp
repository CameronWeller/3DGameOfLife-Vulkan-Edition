#include "engine/vulkan/compute/VulkanCompute.h"
#include "VulkanContext.h"
#include "Grid3D.h"
#include "VulkanMemoryManager.h"
#include <stdexcept>
#include <fstream>
#include <vector>
#include <array>
#include <cstring>

namespace VulkanHIP {

VulkanCompute::VulkanCompute(VulkanContext* context)
    : context_(context) {
    if (!context_) {
        throw std::runtime_error("VulkanCompute: Invalid context");
    }
}

VulkanCompute::~VulkanCompute() {
    cleanup();
}

void VulkanCompute::createComputePipeline() {
    // Create compute descriptor set layout
    createComputeDescriptorSetLayout();
    
    // Load compute shader
    auto computeShaderCode = readFile("shaders/game_of_life_3d.comp.spv");
    VkShaderModule computeShaderModule = createShaderModule(computeShaderCode);
    
    VkPipelineShaderStageCreateInfo computeShaderStageInfo{};
    computeShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    computeShaderStageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    computeShaderStageInfo.module = computeShaderModule;
    computeShaderStageInfo.pName = "main";
    
    // Push constants
    VkPushConstantRange pushConstantRange{};
    pushConstantRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    pushConstantRange.offset = 0;
    pushConstantRange.size = sizeof(GameOfLifePushConstants);
    
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &computePipeline_.descriptorSetLayout;
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;
    
    if (vkCreatePipelineLayout(context_->getDevice(), &pipelineLayoutInfo, nullptr, &computePipeline_.layout) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create compute pipeline layout!");
    }
    
    VkComputePipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineInfo.layout = computePipeline_.layout;
    pipelineInfo.stage = computeShaderStageInfo;
    
    if (vkCreateComputePipelines(context_->getDevice(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &computePipeline_.pipeline) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create compute pipeline!");
    }
    
    vkDestroyShaderModule(context_->getDevice(), computeShaderModule, nullptr);
}

void VulkanCompute::createComputeDescriptorSetLayout() {
    std::array<VkDescriptorSetLayoutBinding, 2> bindings{};
    
    // Current state buffer
    bindings[0].binding = 0;
    bindings[0].descriptorCount = 1;
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bindings[0].pImmutableSamplers = nullptr;
    bindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    
    // Next state buffer
    bindings[1].binding = 1;
    bindings[1].descriptorCount = 1;
    bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bindings[1].pImmutableSamplers = nullptr;
    bindings[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    
    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    layoutInfo.pBindings = bindings.data();
    
    if (vkCreateDescriptorSetLayout(context_->getDevice(), &layoutInfo, nullptr, &computePipeline_.descriptorSetLayout) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create compute descriptor set layout!");
    }
}

void VulkanCompute::createComputeDescriptorPool() {
    std::array<VkDescriptorPoolSize, 1> poolSizes{};
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    poolSizes[0].descriptorCount = 2; // Two storage buffers (current and next state)
    
    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();
    poolInfo.maxSets = 1;
    
    if (vkCreateDescriptorPool(context_->getDevice(), &poolInfo, nullptr, &computeDescriptorPool_) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create compute descriptor pool!");
    }
}

void VulkanCompute::createComputeDescriptorSets() {
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = computeDescriptorPool_;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &computePipeline_.descriptorSetLayout;
    
    computePipeline_.descriptorSets.resize(1);
    if (vkAllocateDescriptorSets(context_->getDevice(), &allocInfo, computePipeline_.descriptorSets.data()) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate compute descriptor sets!");
    }
    
    // Update descriptor sets with buffer bindings
    std::array<VkWriteDescriptorSet, 2> descriptorWrites{};
    
    VkDescriptorBufferInfo stateBufferInfo{};
    stateBufferInfo.buffer = computePipeline_.stateBuffer;
    stateBufferInfo.offset = 0;
    stateBufferInfo.range = VK_WHOLE_SIZE;
    
    descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[0].dstSet = computePipeline_.descriptorSets[0];
    descriptorWrites[0].dstBinding = 0;
    descriptorWrites[0].dstArrayElement = 0;
    descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    descriptorWrites[0].descriptorCount = 1;
    descriptorWrites[0].pBufferInfo = &stateBufferInfo;
    
    VkDescriptorBufferInfo nextStateBufferInfo{};
    nextStateBufferInfo.buffer = computePipeline_.nextStateBuffer;
    nextStateBufferInfo.offset = 0;
    nextStateBufferInfo.range = VK_WHOLE_SIZE;
    
    descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[1].dstSet = computePipeline_.descriptorSets[0];
    descriptorWrites[1].dstBinding = 1;
    descriptorWrites[1].dstArrayElement = 0;
    descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    descriptorWrites[1].descriptorCount = 1;
    descriptorWrites[1].pBufferInfo = &nextStateBufferInfo;
    
    vkUpdateDescriptorSets(context_->getDevice(), static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
}

void VulkanCompute::createComputeBuffers(uint32_t gridSize) {
    VkDeviceSize bufferSize = gridSize * gridSize * gridSize * sizeof(uint32_t);
    
    // Note: This is a simplified implementation. In production, use VMA or VulkanMemoryManager
    // For now, we'll create basic buffers without VMA
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = bufferSize;
    bufferInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    
    if (vkCreateBuffer(context_->getDevice(), &bufferInfo, nullptr, &computePipeline_.stateBuffer) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create state buffer!");
    }
    
    if (vkCreateBuffer(context_->getDevice(), &bufferInfo, nullptr, &computePipeline_.nextStateBuffer) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create next state buffer!");
    }
    
    // Allocate memory (simplified - should use memory manager)
    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(context_->getDevice(), computePipeline_.stateBuffer, &memRequirements);
    
    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = 0; // Simplified - should find appropriate memory type
    
    // Note: This is a placeholder. Proper implementation would:
    // 1. Find memory type with VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    // 2. Allocate memory for both buffers
    // 3. Bind buffers to memory
}

void VulkanCompute::updateComputePushConstants(const GameOfLifePushConstants& constants) {
    computePipeline_.pushConstants = constants;
}

void VulkanCompute::submitComputeWork() {
    // This is a placeholder - proper implementation would:
    // 1. Allocate command buffer from compute command pool
    // 2. Record compute dispatch command
    // 3. Submit to compute queue
    // 4. Wait for completion
}

void VulkanCompute::cleanup() {
    if (computePipeline_.pipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(context_->getDevice(), computePipeline_.pipeline, nullptr);
        computePipeline_.pipeline = VK_NULL_HANDLE;
    }
    
    if (computePipeline_.layout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(context_->getDevice(), computePipeline_.layout, nullptr);
        computePipeline_.layout = VK_NULL_HANDLE;
    }
    
    if (computePipeline_.descriptorSetLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(context_->getDevice(), computePipeline_.descriptorSetLayout, nullptr);
        computePipeline_.descriptorSetLayout = VK_NULL_HANDLE;
    }
    
    if (computeDescriptorPool_ != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(context_->getDevice(), computeDescriptorPool_, nullptr);
        computeDescriptorPool_ = VK_NULL_HANDLE;
    }
    
    if (computePipeline_.stateBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(context_->getDevice(), computePipeline_.stateBuffer, nullptr);
        computePipeline_.stateBuffer = VK_NULL_HANDLE;
    }
    
    if (computePipeline_.nextStateBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(context_->getDevice(), computePipeline_.nextStateBuffer, nullptr);
        computePipeline_.nextStateBuffer = VK_NULL_HANDLE;
    }
    
    if (computeCommandPool_ != VK_NULL_HANDLE) {
        vkDestroyCommandPool(context_->getDevice(), computeCommandPool_, nullptr);
        computeCommandPool_ = VK_NULL_HANDLE;
    }
}

std::vector<char> VulkanCompute::readFile(const std::string& filename) {
    std::ifstream file(filename, std::ios::ate | std::ios::binary);
    
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open file: " + filename);
    }
    
    size_t fileSize = static_cast<size_t>(file.tellg());
    std::vector<char> buffer(fileSize);
    
    file.seekg(0);
    file.read(buffer.data(), fileSize);
    file.close();
    
    return buffer;
}

VkShaderModule VulkanCompute::createShaderModule(const std::vector<char>& code) {
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = code.size();
    createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());
    
    VkShaderModule shaderModule;
    if (vkCreateShaderModule(context_->getDevice(), &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create shader module!");
    }
    
    return shaderModule;
}

}
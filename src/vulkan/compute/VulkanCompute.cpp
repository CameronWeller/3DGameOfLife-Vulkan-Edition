#include "vulkan/compute/VulkanCompute.h"
#include "VulkanContext.h"
#include "Grid3D.h"
#include <stdexcept>
#include <fstream>
#include <vector>

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
}

}
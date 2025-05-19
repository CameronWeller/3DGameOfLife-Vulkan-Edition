#include "VulkanEngine.h"
#include <iostream>
#include <fstream>
#include <stdexcept>
#include <cstring>
#include <set>
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <vector>
#include <optional>
#include <array>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <limits>
#include "VulkanContext.h"
#include "WindowManager.h"

// Initialize the static instance pointer
VulkanEngine* VulkanEngine::instance_ = nullptr;

// Constructor
VulkanEngine::VulkanEngine()
    : swapChain(VK_NULL_HANDLE),
      swapChainImageFormat(VK_FORMAT_UNDEFINED),
      renderPass(VK_NULL_HANDLE),
      depthImage(VK_NULL_HANDLE),
      depthImageMemory(VK_NULL_HANDLE),
      depthImageView(VK_NULL_HANDLE),
      colorImage(VK_NULL_HANDLE),
      colorImageMemory(VK_NULL_HANDLE),
      colorImageView(VK_NULL_HANDLE),
      vertexBuffer(VK_NULL_HANDLE),
      vertexBufferMemory(VK_NULL_HANDLE),
      indexBuffer(VK_NULL_HANDLE),
      indexBufferMemory(VK_NULL_HANDLE),
      descriptorPool(VK_NULL_HANDLE),
      textureImageView(VK_NULL_HANDLE),
      textureSampler(VK_NULL_HANDLE),
      currentFrame(0),
      framebufferResized(false) {
    
    if (instance_ != nullptr) {
        throw std::runtime_error("VulkanEngine instance already exists!");
    }
    instance_ = this;
    
    // Set up vertex data
    vertices = {
        {{-0.5f, -0.5f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
        {{0.5f, -0.5f, 0.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}},
        {{0.0f, 0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.5f, 1.0f}}
    };

    // Set up index data
    indices = {0, 1, 2};

    // Initialize enabledFeatures_
    memset(&enabledFeatures_, 0, sizeof(VkPhysicalDeviceFeatures));
}

// Destructor
VulkanEngine::~VulkanEngine() {
    cleanup();
    if (instance_ == this) {
        instance_ = nullptr;
    }
}

// Initialize the engine
void VulkanEngine::init() {
    // Create window manager first
    windowManager_ = std::make_unique<WindowManager>(WIDTH, HEIGHT, WINDOW_TITLE);
    
    // Create Vulkan context with window manager
    vulkanContext_ = std::make_unique<VulkanContext>(
        windowManager_,
        getRequiredInstanceExtensions(),
        enableValidationLayers ? validationLayers_ : std::vector<const char*>()
    );

    // Initialize device manager with required extensions and features
    vulkanContext_->initDeviceManager(deviceExtensions_, enabledFeatures_);

    // Create memory pool
    memoryPool_ = std::make_unique<MemoryPool>(vulkanContext_->getDevice(), vulkanContext_->getPhysicalDevice());

    // Get queue family indices
    queueFamilyIndices_ = vulkanContext_->getQueueFamilyIndices();

    // Create command pools
    createCommandPools();

    // Create descriptor set layout
    createDescriptorSetLayout();

    // Create graphics pipeline
    createGraphicsPipeline();

    // Create swap chain
    createSwapChain();

    // Create image views
    createImageViews();

    // Create render pass
    createRenderPass();

    // Create depth resources
    createDepthResources();

    // Create color resources
    createColorResources();

    // Create framebuffers
    createFramebuffers();

    // Create vertex buffer
    createVertexBuffer();

    // Create uniform buffers
    createUniformBuffers();

    // Create descriptor pool
    createDescriptorPool();

    // Create descriptor sets
    createDescriptorSets();

    // Create command buffers
    createCommandBuffers();

    // Create sync objects
    createSyncObjects();

    std::cout << "VulkanEngine core context initialized." << std::endl;
}

// Placeholder for the new applyEnabledDeviceFeatures
void VulkanEngine::applyEnabledDeviceFeatures(VkPhysicalDeviceFeatures& features) {
    features.samplerAnisotropy = VK_TRUE;
    features.fillModeNonSolid = VK_TRUE;
    features.wideLines = VK_TRUE;
    features.multiDrawIndirect = VK_TRUE;
    features.drawIndirectFirstInstance = VK_TRUE;
    features.shaderInt64 = VK_TRUE;
    features.shaderFloat64 = VK_TRUE;
    features.shaderInt16 = VK_TRUE;
    features.shaderStorageImageExtendedFormats = VK_TRUE;
    features.shaderStorageImageMultisample = VK_TRUE;
    features.shaderStorageImageReadWithoutFormat = VK_TRUE;
    features.shaderStorageImageWriteWithoutFormat = VK_TRUE;
    features.shaderUniformBufferArrayDynamicIndexing = VK_TRUE;
    features.shaderSampledImageArrayDynamicIndexing = VK_TRUE;
    features.shaderStorageBufferArrayDynamicIndexing = VK_TRUE;
    features.shaderStorageImageArrayDynamicIndexing = VK_TRUE;
    features.shaderClipDistance = VK_TRUE;
    features.shaderCullDistance = VK_TRUE;
    features.shaderResourceResidency = VK_TRUE;
    features.shaderResourceMinLod = VK_TRUE;
    features.sparseBinding = VK_TRUE;
    features.sparseResidencyBuffer = VK_TRUE;
    features.sparseResidencyImage2D = VK_TRUE;
    features.sparseResidencyImage3D = VK_TRUE;
    features.sparseResidency2Samples = VK_TRUE;
    features.sparseResidency4Samples = VK_TRUE;
    features.sparseResidency8Samples = VK_TRUE;
    features.sparseResidency16Samples = VK_TRUE;
    features.sparseResidencyAliased = VK_TRUE;
    features.variableMultisampleRate = VK_TRUE;
    features.inheritedQueries = VK_TRUE;
}

std::vector<const char*> VulkanEngine::getRequiredInstanceExtensions() {
    std::vector<const char*> extensions;
    
    // Add extensions required by GLFW
    uint32_t glfwExtensionCount = 0;
    const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
    extensions.insert(extensions.end(), glfwExtensions, glfwExtensions + glfwExtensionCount);
    
    // Add any additional required extensions
    extensions.insert(extensions.end(), instanceExtensions_.begin(), instanceExtensions_.end());
    
    return extensions;
}

// Draw a frame
void VulkanEngine::drawFrame() {
    vkWaitForFences(vulkanContext_->getDevice(), 1, &inFlightFences[currentFrame_], VK_TRUE, UINT64_MAX);

    uint32_t imageIndex;
    VkResult result = vkAcquireNextImageKHR(vulkanContext_->getDevice(), swapChain_, UINT64_MAX, 
                                           imageAvailableSemaphores[currentFrame_], VK_NULL_HANDLE, &imageIndex);

    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        recreateSwapChain();
        return;
    } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        throw std::runtime_error("Failed to acquire swap chain image!");
    }

    updateUniformBuffer(currentFrame_);

    vkResetFences(vulkanContext_->getDevice(), 1, &inFlightFences[currentFrame_]);

    vkResetCommandBuffer(commandBuffers[currentFrame_], 0);
    recordCommandBuffer(commandBuffers[currentFrame_], imageIndex);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    VkSemaphore waitSemaphores[] = {imageAvailableSemaphores[currentFrame_]};
    VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffers[currentFrame_];

    VkSemaphore signalSemaphores[] = {renderFinishedSemaphores[currentFrame_]};
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;

    if (vkQueueSubmit(vulkanContext_->getGraphicsQueue(), 1, &submitInfo, inFlightFences[currentFrame_]) != VK_SUCCESS) {
        throw std::runtime_error("Failed to submit draw command buffer!");
    }

    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores;

    VkSwapchainKHR swapChains[] = {swapChain_};
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapChains;
    presentInfo.pImageIndices = &imageIndex;
    presentInfo.pResults = nullptr;

    result = vkQueuePresentKHR(vulkanContext_->getPresentQueue(), &presentInfo);

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || framebufferResized_) {
        framebufferResized_ = false;
        recreateSwapChain();
    } else if (result != VK_SUCCESS) {
        throw std::runtime_error("Failed to present swap chain image!");
    }

    currentFrame_ = (currentFrame_ + 1) % MAX_FRAMES_IN_FLIGHT;
}

void VulkanEngine::waitForComputeCompletion() {
    vkWaitForFences(vulkanContext_->getDevice(), 1, &computeFences[currentFrame_], VK_TRUE, UINT64_MAX);
}

void VulkanEngine::createCommandPools() {
    VkCommandPoolCreateInfo graphicsPoolInfo{};
    graphicsPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    graphicsPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    graphicsPoolInfo.queueFamilyIndex = queueFamilyIndices_.graphicsFamily.value();

    if (vkCreateCommandPool(vulkanContext_->getDevice(), &graphicsPoolInfo, nullptr, &graphicsCommandPool_) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create graphics command pool!");
    }

    VkCommandPoolCreateInfo computePoolInfo{};
    computePoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    computePoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    computePoolInfo.queueFamilyIndex = queueFamilyIndices_.computeFamily.value();

    if (vkCreateCommandPool(vulkanContext_->getDevice(), &computePoolInfo, nullptr, &computeCommandPool_) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create compute command pool!");
    }
}

// Run the engine
void VulkanEngine::run() {
    while (!windowManager_->shouldClose()) {
        windowManager_->pollEvents();
        drawFrame();
    }

    vkDeviceWaitIdle(vulkanContext_->getDevice());
}

// Record a command buffer for drawing a frame
void VulkanEngine::recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex) {
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
        throw std::runtime_error("Failed to begin recording command buffer!");
    }

    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = renderPass_;
    renderPassInfo.framebuffer = swapChainFramebuffers[imageIndex];
    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = swapChainExtent;

    std::array<VkClearValue, 2> clearValues{};
    clearValues[0].color = {{0.0f, 0.0f, 0.0f, 1.0f}};
    clearValues[1].depthStencil = {1.0f, 0};

    renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
    renderPassInfo.pClearValues = clearValues.data();

    vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline_);

    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(swapChainExtent.width);
    viewport.height = static_cast<float>(swapChainExtent.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = swapChainExtent;
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

    VkBuffer vertexBuffers[] = {vertexBuffer_};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);

    vkCmdBindIndexBuffer(commandBuffer, indexBuffer_, 0, VK_INDEX_TYPE_UINT32);

    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout_, 0, 1, 
                           &descriptorSets[currentFrame_], 0, nullptr);

    vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(indices.size()), 1, 0, 0, 0);

    vkCmdEndRenderPass(commandBuffer);

    if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
        throw std::runtime_error("Failed to record command buffer!");
    }
}

// Create a compute pipeline
VkPipeline VulkanEngine::createComputePipeline(const std::string& shaderPath) {
    VkDevice currentDevice = vulkanContext_->getDevice();
    if (currentDevice == VK_NULL_HANDLE) {
        throw std::runtime_error("Device not available for compute pipeline creation");
    }

    auto shaderCode = readFile(shaderPath);
    VkShaderModule computeShaderModule = createShaderModule(shaderCode); 
    // Note: The created shaderModule should be managed (stored for cleanup) by a future ShaderManager or PipelineManager
    // For now, VulkanEngine might still temporarily hold it in shaderModules if createShaderModule adds it there.
    // Or, it must be destroyed after pipeline creation if not stored globally.

    VkPipelineShaderStageCreateInfo shaderStageInfo{};
    shaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    shaderStageInfo.module = computeShaderModule;
    shaderStageInfo.pName = "main";

    // descriptorSetLayout and pipelineLayout are still members of VulkanEngine for now
    // They will be obtained from DescriptorSetManager and PipelineManager later.
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &descriptorSetLayout_; 
    
    VkPushConstantRange pushConstantRange{};
    pushConstantRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    pushConstantRange.offset = 0;
    pushConstantRange.size = sizeof(uint32_t) * 3; 
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;

    // Assuming pipelineLayout is a member to be created/managed by PipelineManager later
    // For now, we might be creating a local one or using a member of VulkanEngine.
    // Let's assume it's a member `this->pipelineLayout_Compute` for this specific pipeline for now.
    VkPipelineLayout tempPipelineLayout; // This should be a member or managed by PipelineManager
    if (vkCreatePipelineLayout(currentDevice, &pipelineLayoutInfo, nullptr, &tempPipelineLayout) != VK_SUCCESS) {
        vkDestroyShaderModule(currentDevice, computeShaderModule, nullptr); // Clean up shader module on failure
        throw std::runtime_error("Failed to create compute pipeline layout!");
    }

    VkComputePipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineInfo.stage = shaderStageInfo;
    pipelineInfo.layout = tempPipelineLayout; // Use the layout created for this pipeline

    VkPipeline computePipeline;
    if (vkCreateComputePipelines(currentDevice, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &computePipeline) != VK_SUCCESS) {
        vkDestroyPipelineLayout(currentDevice, tempPipelineLayout, nullptr); // Clean up layout
        vkDestroyShaderModule(currentDevice, computeShaderModule, nullptr); // Clean up shader module
        throw std::runtime_error("Failed to create compute pipeline!");
    }

    // Shader module can be destroyed after pipeline creation if not managed elsewhere
    vkDestroyShaderModule(currentDevice, computeShaderModule, nullptr);
    // The tempPipelineLayout should also be managed (destroyed when this compute pipeline is destroyed)
    // This highlights the need for PipelineManager to own layouts and pipelines together.

    return computePipeline;
}

// Destroy a compute pipeline
void VulkanEngine::destroyComputePipeline(VkPipeline pipeline) {
    // This function might need to also destroy the associated pipelineLayout
    // if it's uniquely created for this compute pipeline.
    VkDevice currentDevice = vulkanContext_->getDevice();
    if (currentDevice == VK_NULL_HANDLE || pipeline == VK_NULL_HANDLE) return;

    vkDestroyPipeline(currentDevice, pipeline, nullptr);
    // If a unique pipelineLayout was created with this compute pipeline, destroy it here.
    // For now, assuming a shared or globally managed pipelineLayout, or that it's handled elsewhere.
}

// Begin a single-time command buffer
VkCommandBuffer VulkanEngine::beginSingleTimeCommands() {
    VkDevice currentDevice = vulkanContext_->getDevice();
    VkCommandPool poolToUse = graphicsCommandPool_; // Default to graphics pool, will be passed from CommandManager
    
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = poolToUse; 
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    if (vkAllocateCommandBuffers(currentDevice, &allocInfo, &commandBuffer) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate single-time command buffer");
    }

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
        // Need to free the command buffer if begin fails
        vkFreeCommandBuffers(currentDevice, poolToUse, 1, &commandBuffer);
        throw std::runtime_error("Failed to begin single-time command buffer");
    }
    return commandBuffer;
}

void VulkanEngine::endSingleTimeCommands(VkCommandBuffer commandBuffer) {
    VkDevice currentDevice = vulkanContext_->getDevice();
    VkQueue queueToUse = vulkanContext_->getGraphicsQueue();
    VkCommandPool poolToUse = graphicsCommandPool_;

    if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
        vkFreeCommandBuffers(currentDevice, poolToUse, 1, &commandBuffer);
        throw std::runtime_error("Failed to end single-time command buffer");
    }

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    VkResult result = vkQueueSubmit(queueToUse, 1, &submitInfo, VK_NULL_HANDLE);
    if (result != VK_SUCCESS) {
        vkFreeCommandBuffers(currentDevice, poolToUse, 1, &commandBuffer);
        throw std::runtime_error("Failed to submit single-time command buffer");
    }

    result = vkQueueWaitIdle(queueToUse);
    if (result != VK_SUCCESS) {
        vkFreeCommandBuffers(currentDevice, poolToUse, 1, &commandBuffer);
        throw std::runtime_error("Queue wait idle failed for single-time command buffer");
    }

    vkFreeCommandBuffers(currentDevice, poolToUse, 1, &commandBuffer);
}

void VulkanEngine::copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size) {
    VkCommandBuffer commandBuffer = beginSingleTimeCommands();
    VkBufferCopy copyRegion{};
    copyRegion.size = size;
    vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);
    endSingleTimeCommands(commandBuffer);
}

// This is the createBuffer that takes VkBuffer& and VkDeviceMemory& as output params
// It will be used by systems that manage their own memory outside MemoryPool (e.g. initial vertex buffer setup)
// Or, it can be a helper within MemoryPool itself.
void VulkanEngine::createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory) {
    VkDevice currentDevice = vulkanContext_->getDevice();
    if (currentDevice == VK_NULL_HANDLE) {
        throw std::runtime_error("Device not available for buffer creation");
    }

    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(currentDevice, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create buffer!");
    }

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(currentDevice, buffer, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    // Use MemoryPool's findMemoryType since it has access to physicalDevice
    allocInfo.memoryTypeIndex = memoryPool_->findMemoryType(memRequirements.memoryTypeBits, properties);

    if (vkAllocateMemory(currentDevice, &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS) {
        vkDestroyBuffer(currentDevice, buffer, nullptr); // Clean up created buffer
        throw std::runtime_error("Failed to allocate buffer memory!");
    }

    if (vkBindBufferMemory(currentDevice, buffer, bufferMemory, 0) != VK_SUCCESS) {
        vkDestroyBuffer(currentDevice, buffer, nullptr); // Clean up
        vkFreeMemory(currentDevice, bufferMemory, nullptr); // Clean up
        throw std::runtime_error("Failed to bind buffer memory!");
    }
}

// Read a file into a vector of chars
std::vector<char> VulkanEngine::readFile(const std::string& filename) {
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

void VulkanEngine::cleanup() {
    cleanupSwapChain();

    // Destroy command pools
    if (graphicsCommandPool_ != VK_NULL_HANDLE) {
        vkDestroyCommandPool(vulkanContext_->getDevice(), graphicsCommandPool_, nullptr);
        graphicsCommandPool_ = VK_NULL_HANDLE;
    }
    if (computeCommandPool_ != VK_NULL_HANDLE) {
        vkDestroyCommandPool(vulkanContext_->getDevice(), computeCommandPool_, nullptr);
        computeCommandPool_ = VK_NULL_HANDLE;
    }

    // Destroy descriptor set layout
    if (descriptorSetLayout_ != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(vulkanContext_->getDevice(), descriptorSetLayout_, nullptr);
        descriptorSetLayout_ = VK_NULL_HANDLE;
    }

    // Destroy pipeline layout
    if (pipelineLayout_ != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(vulkanContext_->getDevice(), pipelineLayout_, nullptr);
        pipelineLayout_ = VK_NULL_HANDLE;
    }

    // Destroy graphics pipeline
    if (graphicsPipeline_ != VK_NULL_HANDLE) {
        vkDestroyPipeline(vulkanContext_->getDevice(), graphicsPipeline_, nullptr);
        graphicsPipeline_ = VK_NULL_HANDLE;
    }

    // Destroy shader modules
    for (auto shaderModule : shaderModules) {
        if (shaderModule != VK_NULL_HANDLE) {
            vkDestroyShaderModule(vulkanContext_->getDevice(), shaderModule, nullptr);
        }
    }
    shaderModules.clear();

    // Clean up memory pool
    memoryPool_.reset();

    // Clean up Vulkan context
    vulkanContext_.reset();

    // Clean up window manager last
    windowManager_.reset();

    std::cout << "VulkanEngine cleanup finished." << std::endl;
}

// Placeholder for framebufferResizeCallback - will be handled by WindowManager
void VulkanEngine::framebufferResizeCallback(GLFWwindow* window, int width, int height) {
    auto app = reinterpret_cast<VulkanEngine*>(glfwGetWindowUserPointer(window));
    // app->framebufferResized_ = true; // This flag will be managed by SwapchainManager or VulkanEngine based on WindowManager events
}

VkShaderModule VulkanEngine::createShaderModule(const std::vector<char>& code) {
    VkDevice currentDevice = vulkanContext_->getDevice();
    if (currentDevice == VK_NULL_HANDLE) {
        throw std::runtime_error("Device not available for shader module creation");
    }

    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = code.size();
    createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

    VkShaderModule shaderModule;
    if (vkCreateShaderModule(currentDevice, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create shader module!");
    }
    // shaderModules.push_back(shaderModule); // This logic will move to a ShaderManager or PipelineManager
    return shaderModule;
}

void VulkanEngine::createCommandBuffers() {
    commandBuffers.resize(MAX_FRAMES_IN_FLIGHT);

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = graphicsCommandPool_;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = (uint32_t) commandBuffers.size();

    if (vkAllocateCommandBuffers(vulkanContext_->getDevice(), &allocInfo, commandBuffers.data()) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate command buffers!");
    }
}

// MemoryPool implementation (still in VulkanEngine.cpp for now, to be moved)
MemoryPool::MemoryPool(VkDevice dev, VkPhysicalDevice pd) 
    : device_(dev), 
      physicalDevice_(pd),
      maxStagingSize_(0) {
    if (device_ == VK_NULL_HANDLE) {
        throw std::runtime_error("MemoryPool: Device cannot be null");
    }
    if (physicalDevice_ == VK_NULL_HANDLE) {
        throw std::runtime_error("MemoryPool: Physical device cannot be null");
    }
}

MemoryPool::~MemoryPool() {
    // Ensure device_ is valid if it came from VulkanContext and VulkanContext might be destroyed first.
    // This highlights the ownership and lifetime issues we are resolving.
    // For now, assume device_ is valid or cleanup is handled.
    if (device_ == VK_NULL_HANDLE) return; // Or log warning

    for (const auto& allocation : bufferPool_) {
        if (allocation.buffer != VK_NULL_HANDLE) vkDestroyBuffer(device_, allocation.buffer, nullptr);
        if (allocation.memory != VK_NULL_HANDLE) vkFreeMemory(device_, allocation.memory, nullptr);
    }
    bufferPool_.clear();
    for (const auto& staging : stagingPool_) {
        if (staging.buffer != VK_NULL_HANDLE) vkDestroyBuffer(device_, staging.buffer, nullptr);
        if (staging.memory != VK_NULL_HANDLE) vkFreeMemory(device_, staging.memory, nullptr);
    }
    stagingPool_.clear();
}

// This is MemoryPool::findMemoryType. It needs a physical device.
// It will get it from VulkanEngine::getInstance()->getVulkanContext()->getPhysicalDevice()
// This is a temporary measure. This method should ideally not rely on a global singleton.
// Once DeviceManager is fully integrated into VulkanContext, VulkanContext can pass VkPhysicalDevice to MemoryPool constructor
// or MemoryPool can ask VulkanContext for it.
uint32_t MemoryPool::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) {
    if (physicalDevice_ == VK_NULL_HANDLE) {
        throw std::runtime_error("Physical device not available for MemoryPool::findMemoryType");
    }

    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice_, &memProperties);
    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    throw std::runtime_error("Failed to find suitable memory type (MemoryPool)!");
}


MemoryPool::BufferAllocation MemoryPool::allocateBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties) {
    if (size == 0) throw std::runtime_error("MemoryPool: Cannot allocate buffer of size 0.");
    // ... (rest of existing allocateBuffer implementation, ensure it uses this->findMemoryType) ...
    // Create new buffer if no suitable one found
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VkBuffer buffer;
    if (vkCreateBuffer(device_, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create buffer in MemoryPool!");
    }

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(device_, buffer, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = this->findMemoryType(memRequirements.memoryTypeBits, properties); // Use member findMemoryType

    VkDeviceMemory memory;
    if (vkAllocateMemory(device_, &allocInfo, nullptr, &memory) != VK_SUCCESS) {
        vkDestroyBuffer(device_, buffer, nullptr);
        throw std::runtime_error("Failed to allocate buffer memory in MemoryPool!");
    }

    vkBindBufferMemory(device_, buffer, memory, 0);

    BufferAllocation allocation{buffer, memory, size, usage, properties, true};
    bufferPool_.push_back(allocation);
    return allocation;
}

// ... (MemoryPool::freeBuffer, getStagingBuffer, returnStagingBuffer implementations, ensure they use this->findMemoryType for staging if needed)
// Ensure getStagingBuffer also uses this->findMemoryType
MemoryPool::StagingBuffer MemoryPool::getStagingBuffer(VkDeviceSize size) {
    if (size == 0) throw std::runtime_error("MemoryPool: Cannot get staging buffer of size 0.");
    // ... (rest of existing getStagingBuffer implementation) ...
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT; // Or _DST_BIT or both depending on usage
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VkBuffer buffer;
    if (vkCreateBuffer(device_, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create staging buffer in MemoryPool!");
    }
    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(device_, buffer, &memRequirements);
    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = this->findMemoryType(
        memRequirements.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    );
    VkDeviceMemory memory;
    if (vkAllocateMemory(device_, &allocInfo, nullptr, &memory) != VK_SUCCESS) {
        vkDestroyBuffer(device_, buffer, nullptr);
        throw std::runtime_error("Failed to allocate staging memory in MemoryPool!");
    }
    vkBindBufferMemory(device_, buffer, memory, 0);
    StagingBuffer staging{buffer, memory, size, true};
    stagingPool_.push_back(staging);
    maxStagingSize_ = std::max(maxStagingSize_, size);
    return staging;
}

void VulkanEngine::createSwapChain() {
    SwapChainSupportDetails swapChainSupport = querySwapChainSupport(vulkanContext_->getPhysicalDevice());

    VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
    VkPresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);
    VkExtent2D extent = chooseSwapExtent(swapChainSupport.capabilities);

    uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
    if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount) {
        imageCount = swapChainSupport.capabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = vulkanContext_->getSurface();
    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = surfaceFormat.format;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    createInfo.imageExtent = extent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    QueueFamilyIndices indices = queueFamilyIndices_;
    uint32_t queueFamilyIndices[] = {indices.graphicsFamily.value(), indices.presentFamily.value()};

    if (indices.graphicsFamily != indices.presentFamily) {
        createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices = queueFamilyIndices;
    } else {
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        createInfo.queueFamilyIndexCount = 0;
        createInfo.pQueueFamilyIndices = nullptr;
    }

    createInfo.preTransform = swapChainSupport.capabilities.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode = presentMode;
    createInfo.clipped = VK_TRUE;
    createInfo.oldSwapchain = VK_NULL_HANDLE;

    if (vkCreateSwapchainKHR(vulkanContext_->getDevice(), &createInfo, nullptr, &swapChain_) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create swap chain!");
    }

    vkGetSwapchainImagesKHR(vulkanContext_->getDevice(), swapChain_, &imageCount, nullptr);
    swapChainImages.resize(imageCount);
    vkGetSwapchainImagesKHR(vulkanContext_->getDevice(), swapChain_, &imageCount, swapChainImages.data());

    swapChainImageFormat = surfaceFormat.format;
    swapChainExtent = extent;
}

void VulkanEngine::createImageViews() {
    swapChainImageViews.resize(swapChainImages.size());

    for (size_t i = 0; i < swapChainImages.size(); i++) {
        VkImageViewCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        createInfo.image = swapChainImages[i];
        createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        createInfo.format = swapChainImageFormat;
        createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        createInfo.subresourceRange.baseMipLevel = 0;
        createInfo.subresourceRange.levelCount = 1;
        createInfo.subresourceRange.baseArrayLayer = 0;
        createInfo.subresourceRange.layerCount = 1;

        if (vkCreateImageView(vulkanContext_->getDevice(), &createInfo, nullptr, &swapChainImageViews[i]) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create image views!");
        }
    }
}

void VulkanEngine::cleanupSwapChain() {
    for (auto imageView : swapChainImageViews) {
        vkDestroyImageView(vulkanContext_->getDevice(), imageView, nullptr);
    }

    if (swapChain_ != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(vulkanContext_->getDevice(), swapChain_, nullptr);
        swapChain_ = VK_NULL_HANDLE;
    }
}

void VulkanEngine::recreateSwapChain() {
    int width = 0, height = 0;
    windowManager_->getFramebufferSize(&width, &height);
    while (width == 0 || height == 0) {
        windowManager_->getFramebufferSize(&width, &height);
        windowManager_->waitEvents();
    }

    vkDeviceWaitIdle(vulkanContext_->getDevice());

    cleanupSwapChain();

    createSwapChain();
    createImageViews();
    createRenderPass();
    createColorResources();
    createDepthResources();
    createFramebuffers();
}

VkSurfaceFormatKHR VulkanEngine::chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats) const {
    for (const auto& availableFormat : availableFormats) {
        if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB && 
            availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return availableFormat;
        }
    }
    return availableFormats[0];
}

VkPresentModeKHR VulkanEngine::chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes) const {
    for (const auto& availablePresentMode : availablePresentModes) {
        if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
            return availablePresentMode;
        }
    }
    return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D VulkanEngine::chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities) const {
    if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
        return capabilities.currentExtent;
    } else {
        int width, height;
        windowManager_->getFramebufferSize(&width, &height);

        VkExtent2D actualExtent = {
            static_cast<uint32_t>(width),
            static_cast<uint32_t>(height)
        };

        actualExtent.width = std::clamp(actualExtent.width, 
            capabilities.minImageExtent.width, 
            capabilities.maxImageExtent.width);
        actualExtent.height = std::clamp(actualExtent.height, 
            capabilities.minImageExtent.height, 
            capabilities.maxImageExtent.height);

        return actualExtent;
    }
}

SwapChainSupportDetails VulkanEngine::querySwapChainSupport(VkPhysicalDevice device) const {
    SwapChainSupportDetails details;

    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, vulkanContext_->getSurface(), &details.capabilities);

    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, vulkanContext_->getSurface(), &formatCount, nullptr);

    if (formatCount != 0) {
        details.formats.resize(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, vulkanContext_->getSurface(), &formatCount, details.formats.data());
    }

    uint32_t presentModeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, vulkanContext_->getSurface(), &presentModeCount, nullptr);

    if (presentModeCount != 0) {
        details.presentModes.resize(presentModeCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, vulkanContext_->getSurface(), &presentModeCount, details.presentModes.data());
    }

    return details;
}

void VulkanEngine::createRenderPass() {
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = swapChainImageFormat;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentDescription depthAttachment{};
    depthAttachment.format = findDepthFormat();
    depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference colorAttachmentRef{};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depthAttachmentRef{};
    depthAttachmentRef.attachment = 1;
    depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;
    subpass.pDepthStencilAttachment = &depthAttachmentRef;

    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    std::array<VkAttachmentDescription, 2> attachments = {colorAttachment, depthAttachment};
    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
    renderPassInfo.pAttachments = attachments.data();
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &dependency;

    if (vkCreateRenderPass(vulkanContext_->getDevice(), &renderPassInfo, nullptr, &renderPass_) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create render pass!");
    }
}

void VulkanEngine::createFramebuffers() {
    swapChainFramebuffers.resize(swapChainImageViews.size());

    for (size_t i = 0; i < swapChainImageViews.size(); i++) {
        std::array<VkImageView, 2> attachments = {
            swapChainImageViews[i],
            depthImageView_
        };

        VkFramebufferCreateInfo framebufferInfo{};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = renderPass_;
        framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
        framebufferInfo.pAttachments = attachments.data();
        framebufferInfo.width = swapChainExtent.width;
        framebufferInfo.height = swapChainExtent.height;
        framebufferInfo.layers = 1;

        if (vkCreateFramebuffer(vulkanContext_->getDevice(), &framebufferInfo, nullptr, &swapChainFramebuffers[i]) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create framebuffer!");
        }
    }
}

void VulkanEngine::createDepthResources() {
    VkFormat depthFormat = findDepthFormat();

    createImage(swapChainExtent.width, swapChainExtent.height, depthFormat, VK_IMAGE_TILING_OPTIMAL, 
               VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 
               depthImage_, depthImageMemory_);
    createImageView(depthImage_, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT, depthImageView_);
}

void VulkanEngine::createColorResources() {
    VkFormat colorFormat = swapChainImageFormat;

    createImage(swapChainExtent.width, swapChainExtent.height, colorFormat, VK_IMAGE_TILING_OPTIMAL,
               VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
               VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, colorImage_, colorImageMemory_);
    createImageView(colorImage_, colorFormat, VK_IMAGE_ASPECT_COLOR_BIT, colorImageView_);
}

VkFormat VulkanEngine::findDepthFormat() {
    return findSupportedFormat(
        {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT},
        VK_IMAGE_TILING_OPTIMAL,
        VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
    );
}

VkFormat VulkanEngine::findSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features) {
    for (VkFormat format : candidates) {
        VkFormatProperties props;
        vkGetPhysicalDeviceFormatProperties(vulkanContext_->getPhysicalDevice(), format, &props);

        if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features) {
            return format;
        } else if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features) {
            return format;
        }
    }

    throw std::runtime_error("Failed to find supported format!");
}

bool VulkanEngine::hasStencilComponent(VkFormat format) {
    return format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT;
}

void VulkanEngine::createImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling,
                             VkImageUsageFlags usage, VkMemoryPropertyFlags properties,
                             VkImage& image, VkDeviceMemory& imageMemory) {
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = width;
    imageInfo.extent.height = height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = format;
    imageInfo.tiling = tiling;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = usage;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.flags = 0;

    if (vkCreateImage(vulkanContext_->getDevice(), &imageInfo, nullptr, &image) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create image!");
    }

    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(vulkanContext_->getDevice(), image, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(vulkanContext_->getPhysicalDevice(), memRequirements.memoryTypeBits, properties);

    if (vkAllocateMemory(vulkanContext_->getDevice(), &allocInfo, nullptr, &imageMemory) != VK_SUCCESS) {
        vkDestroyImage(vulkanContext_->getDevice(), image, nullptr);
        throw std::runtime_error("Failed to allocate image memory!");
    }

    vkBindImageMemory(vulkanContext_->getDevice(), image, imageMemory, 0);
}

void VulkanEngine::createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags,
                                 VkImageView& imageView) {
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = format;
    viewInfo.subresourceRange.aspectMask = aspectFlags;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    if (vkCreateImageView(vulkanContext_->getDevice(), &viewInfo, nullptr, &imageView) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create texture image view!");
    }
}

void VulkanEngine::createVertexBuffer() {
    VkDeviceSize bufferSize = sizeof(vertices_[0]) * vertices_.size();

    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, 
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, 
                stagingBuffer, stagingBufferMemory);

    void* data;
    vkMapMemory(vulkanContext_->getDevice(), stagingBufferMemory, 0, bufferSize, 0, &data);
    memcpy(data, vertices_.data(), (size_t) bufferSize);
    vkUnmapMemory(vulkanContext_->getDevice(), stagingBufferMemory);

    createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, 
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, vertexBuffer_, vertexBufferMemory_);

    copyBuffer(stagingBuffer, vertexBuffer_, bufferSize);

    vkDestroyBuffer(vulkanContext_->getDevice(), stagingBuffer, nullptr);
    vkFreeMemory(vulkanContext_->getDevice(), stagingBufferMemory, nullptr);
}

void VulkanEngine::createIndexBuffer() {
    VkDeviceSize bufferSize = sizeof(indices[0]) * indices.size();

    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, 
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, 
                stagingBuffer, stagingBufferMemory);

    void* data;
    vkMapMemory(vulkanContext_->getDevice(), stagingBufferMemory, 0, bufferSize, 0, &data);
    memcpy(data, indices.data(), (size_t) bufferSize);
    vkUnmapMemory(vulkanContext_->getDevice(), stagingBufferMemory);

    createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, 
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, indexBuffer_, indexBufferMemory_);

    copyBuffer(stagingBuffer, indexBuffer_, bufferSize);

    vkDestroyBuffer(vulkanContext_->getDevice(), stagingBuffer, nullptr);
    vkFreeMemory(vulkanContext_->getDevice(), stagingBufferMemory, nullptr);
}

void VulkanEngine::createUniformBuffers() {
    VkDeviceSize bufferSize = sizeof(UniformBufferObject);

    uniformBuffers.resize(MAX_FRAMES_IN_FLIGHT);
    uniformBuffersMemory.resize(MAX_FRAMES_IN_FLIGHT);
    uniformBuffersMapped.resize(MAX_FRAMES_IN_FLIGHT);

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        createBuffer(bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, 
                    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, 
                    uniformBuffers[i], uniformBuffersMemory[i]);

        vkMapMemory(vulkanContext_->getDevice(), uniformBuffersMemory[i], 0, bufferSize, 0, &uniformBuffersMapped[i]);
    }
}

void VulkanEngine::createDescriptorPool() {
    std::array<VkDescriptorPoolSize, 2> poolSizes{};
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[0].descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
    poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[1].descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();
    poolInfo.maxSets = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);

    if (vkCreateDescriptorPool(vulkanContext_->getDevice(), &poolInfo, nullptr, &descriptorPool_) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create descriptor pool!");
    }
}

void VulkanEngine::createDescriptorSets() {
    std::vector<VkDescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, descriptorSetLayout_);
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = descriptorPool_;
    allocInfo.descriptorSetCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
    allocInfo.pSetLayouts = layouts.data();

    descriptorSets.resize(MAX_FRAMES_IN_FLIGHT);
    if (vkAllocateDescriptorSets(vulkanContext_->getDevice(), &allocInfo, descriptorSets.data()) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate descriptor sets!");
    }

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        VkDescriptorBufferInfo bufferInfo{};
        bufferInfo.buffer = uniformBuffers[i];
        bufferInfo.offset = 0;
        bufferInfo.range = sizeof(UniformBufferObject);

        VkDescriptorImageInfo imageInfo{};
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfo.imageView = textureImageView_;
        imageInfo.sampler = textureSampler_;

        std::array<VkWriteDescriptorSet, 2> descriptorWrites{};

        descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[0].dstSet = descriptorSets[i];
        descriptorWrites[0].dstBinding = 0;
        descriptorWrites[0].dstArrayElement = 0;
        descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptorWrites[0].descriptorCount = 1;
        descriptorWrites[0].pBufferInfo = &bufferInfo;

        descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[1].dstSet = descriptorSets[i];
        descriptorWrites[1].dstBinding = 1;
        descriptorWrites[1].dstArrayElement = 0;
        descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptorWrites[1].descriptorCount = 1;
        descriptorWrites[1].pImageInfo = &imageInfo;

        vkUpdateDescriptorSets(vulkanContext_->getDevice(), static_cast<uint32_t>(descriptorWrites.size()), 
                             descriptorWrites.data(), 0, nullptr);
    }
}

void VulkanEngine::createSyncObjects() {
    imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);

    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        if (vkCreateSemaphore(vulkanContext_->getDevice(), &semaphoreInfo, nullptr, &imageAvailableSemaphores[i]) != VK_SUCCESS ||
            vkCreateSemaphore(vulkanContext_->getDevice(), &semaphoreInfo, nullptr, &renderFinishedSemaphores[i]) != VK_SUCCESS ||
            vkCreateFence(vulkanContext_->getDevice(), &fenceInfo, nullptr, &inFlightFences[i]) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create synchronization objects for a frame!");
        }
    }
}

void VulkanEngine::updateUniformBuffer(uint32_t currentImage) {
    static auto startTime = std::chrono::high_resolution_clock::now();

    auto currentTime = std::chrono::high_resolution_clock::now();
    float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

    UniformBufferObject ubo{};
    ubo.model = glm::rotate(glm::mat4(1.0f), time * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
    ubo.view = glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
    
    float aspectRatio = static_cast<float>(swapChainExtent.width) / static_cast<float>(swapChainExtent.height);
    ubo.proj = glm::perspective(glm::radians(45.0f), aspectRatio, 0.1f, 10.0f);
    ubo.proj[1][1] *= -1;

    memcpy(uniformBuffersMapped[currentImage], &ubo, sizeof(ubo));
} 
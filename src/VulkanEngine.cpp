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
#include <nlohmann/json.hpp>
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_vulkan.h>
#include <imgui_impl_glfw.h>
#include "VulkanContext.h"
#include "WindowManager.h"
#include "SaveManager.h"

namespace VulkanHIP {

// Initialize the static instance pointer
VulkanEngine* VulkanEngine::instance_ = nullptr;

// Constructor
VulkanEngine::VulkanEngine()
    : swapChain_(VK_NULL_HANDLE),
      swapChainImageFormat_(VK_FORMAT_UNDEFINED),
      renderPass_(VK_NULL_HANDLE),
      depthImage_(VK_NULL_HANDLE),
      depthImageAllocation_(VK_NULL_HANDLE),
      depthImageView_(VK_NULL_HANDLE),
      colorImage_(VK_NULL_HANDLE),
      colorImageAllocation_(VK_NULL_HANDLE),
      colorImageView_(VK_NULL_HANDLE),
      vertexBuffer_(VK_NULL_HANDLE),
      vertexBufferAllocation_(VK_NULL_HANDLE),
      indexBuffer_(VK_NULL_HANDLE),
      indexBufferAllocation_(VK_NULL_HANDLE),
      descriptorPool_(VK_NULL_HANDLE),
      textureImageView_(VK_NULL_HANDLE),
      textureSampler_(VK_NULL_HANDLE),
      currentFrame_(0),
      framebufferResized_(false),
      graphicsCommandPool_(VK_NULL_HANDLE),
      computeCommandPool_(VK_NULL_HANDLE),
      descriptorSetLayout_(VK_NULL_HANDLE),
      saveManager_(std::make_unique<SaveManager>()),
      loadingElapsed_(0.0f),
      isLoading_(false),
      lastAutoSave_(std::chrono::steady_clock::now()) {
    
    if (instance_ != nullptr) {
        throw std::runtime_error("VulkanEngine instance already exists!");
    }
    instance_ = this;
    
    // Set up vertex data
    vertices_ = {
        {{-0.5f, -0.5f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
        {{0.5f, -0.5f, 0.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}},
        {{0.0f, 0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.5f, 1.0f}}
    };

    // Set up index data
    indices_ = {0, 1, 2};

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
    // Initialize window manager
    windowManager_ = std::make_shared<WindowManager>();
    WindowManager::WindowConfig config;
    config.width = WIDTH;
    config.height = HEIGHT;
    config.title = WINDOW_TITLE;
    windowManager_->init(config);
    
    // Initialize Vulkan context
    vulkanContext_ = std::make_unique<VulkanContext>();
    vulkanContext_->init(getRequiredInstanceExtensions());
    
    // Create surface
    surface_ = windowManager_->createSurface(vulkanContext_->getVkInstance());
    
    // Initialize memory manager
    memoryManager_ = std::make_unique<VulkanMemoryManager>(
        vulkanContext_->getDevice(),
        vulkanContext_->getPhysicalDevice()
    );
    
    // Create command pools
    createCommandPools();
    
    // Create swap chain
    createSwapChain();
    createImageViews();
    createRenderPass();
    createFramebuffers();
    createDepthResources();
    createColorResources();
    createDescriptorSetLayout();
    createGraphicsPipeline();
    createCommandBuffers();
    createSyncObjects();
    
    // Initialize ImGui
    initImGui();
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
    vkWaitForFences(vulkanContext_->getDevice(), 1, &inFlightFences_[currentFrame_], VK_TRUE, UINT64_MAX);

    uint32_t imageIndex;
    VkResult result = vkAcquireNextImageKHR(vulkanContext_->getDevice(), swapChain_, UINT64_MAX, 
                                           imageAvailableSemaphores_[currentFrame_], VK_NULL_HANDLE, &imageIndex);

    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        recreateSwapChain();
        return;
    } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        throw std::runtime_error("Failed to acquire swap chain image!");
    }

    updateUniformBuffer(currentFrame_);

    vkResetFences(vulkanContext_->getDevice(), 1, &inFlightFences_[currentFrame_]);

    vkResetCommandBuffer(commandBuffers_[currentFrame_], 0);
    recordCommandBuffer(commandBuffers_[currentFrame_], imageIndex);

    // Begin ImGui frame
    beginImGuiFrame();

    // Draw ImGui windows
    drawMenu();
    drawSavePicker();
    if (isLoading_) {
        drawLoading();
    }

    // End ImGui frame
    endImGuiFrame();

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    VkSemaphore waitSemaphores[] = {imageAvailableSemaphores_[currentFrame_]};
    VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffers_[currentFrame_];

    VkSemaphore signalSemaphores[] = {renderFinishedSemaphores_[currentFrame_]};
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;

    if (vkQueueSubmit(vulkanContext_->getGraphicsQueue(), 1, &submitInfo, inFlightFences_[currentFrame_]) != VK_SUCCESS) {
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
    VK_CHECK(vkWaitForFences(vulkanContext_->getDevice(), 1, &computeFences[currentFrame_], VK_TRUE, UINT64_MAX));
    VK_CHECK(vkResetFences(vulkanContext_->getDevice(), 1, &computeFences[currentFrame_]));
}

void VulkanEngine::submitComputeCommand(VkCommandBuffer commandBuffer) {
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    // Wait for graphics queue to finish if needed
    VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
    auto graphicsQueue = vulkanContext_->getGraphicsQueue();
    auto computeQueue = vulkanContext_->getComputeQueue();
    if (graphicsQueue != computeQueue) {
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = &renderFinishedSemaphores_[currentFrame_];
        submitInfo.pWaitDstStageMask = &waitStage;
    }

    if (vkQueueSubmit(computeQueue, 1, &submitInfo, computeFences[currentFrame_]) != VK_SUCCESS) {
        throw std::runtime_error("Failed to submit compute command!");
    }
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
    while (!windowManager_->shouldClose() && currentState_ != App::State::Exiting) {
        windowManager_->pollEvents();
        switch (currentState_) {
            case App::State::Menu:
                drawMenu();
                break;
            case App::State::Running:
                drawFrame();
                break;
            case App::State::SavePicker:
                drawSavePicker();
                break;
            case App::State::Loading:
                drawLoading();
                break;
            case App::State::Exiting:
                break;
        }
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
    renderPassInfo.framebuffer = swapChainFramebuffers_[imageIndex];
    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = swapChainExtent_;

    std::array<VkClearValue, 2> clearValues{};
    clearValues[0].color = {{0.0f, 0.0f, 0.0f, 1.0f}};
    clearValues[1].depthStencil = {1.0f, 0};

    renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
    renderPassInfo.pClearValues = clearValues.data();

    vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline_);

    VkBuffer vertexBuffers[] = {vertexBuffer_};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);

    vkCmdBindIndexBuffer(commandBuffer, indexBuffer_, 0, VK_INDEX_TYPE_UINT32);

    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout_, 0, 1, &descriptorSets_[currentFrame_], 0, nullptr);

    vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(indices_.size()), 1, 0, 0, 0);

    // Record ImGui draw commands
    if (imguiInitialized_) {
        ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), commandBuffer);
    }

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

    // Load compute shader
    auto computeShaderCode = readFile(shaderPath);
    ShaderModule computeShaderModule(createShaderModule(computeShaderCode));

    VkPipelineShaderStageCreateInfo shaderStageInfo{};
    shaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    shaderStageInfo.module = computeShaderModule;
    shaderStageInfo.pName = "main";

    // Create pipeline layout
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &descriptorSetLayout_;

    PipelineLayout pipelineLayout;
    VK_CHECK(vkCreatePipelineLayout(currentDevice, &pipelineLayoutInfo, nullptr, pipelineLayout.address()));

    VkComputePipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineInfo.stage = shaderStageInfo;
    pipelineInfo.layout = pipelineLayout;

    Pipeline computePipeline;
    VK_CHECK(vkCreateComputePipelines(currentDevice, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, computePipeline.address()));

    // Store pipeline and layout for cleanup
    computePipelines_.push_back({std::move(computePipeline), std::move(pipelineLayout)});

    return computePipelines_.back().pipeline;
}

// Destroy a compute pipeline
void VulkanEngine::destroyComputePipeline(VkPipeline pipeline) {
    VkDevice currentDevice = vulkanContext_->getDevice();
    if (currentDevice == VK_NULL_HANDLE || pipeline == VK_NULL_HANDLE) return;

    // Find and remove the pipeline info
    auto it = std::find_if(computePipelines_.begin(), computePipelines_.end(),
        [pipeline](const ComputePipelineInfo& info) { return info.pipeline == pipeline; });

    if (it != computePipelines_.end()) {
        vkDestroyPipeline(currentDevice, it->pipeline, nullptr);
        vkDestroyPipelineLayout(currentDevice, it->layout, nullptr);
        computePipelines_.erase(it);
    }
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
    vkDeviceWaitIdle(vulkanContext_->getDevice());

    // Cleanup ImGui
    cleanupImGui();

    // Cleanup swap chain
    cleanupSwapChain();

    // Cleanup descriptor set layout
    if (descriptorSetLayout_ != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(vulkanContext_->getDevice(), descriptorSetLayout_, nullptr);
        descriptorSetLayout_ = VK_NULL_HANDLE;
    }

    // Cleanup command pools
    if (graphicsCommandPool_ != VK_NULL_HANDLE) {
        vkDestroyCommandPool(vulkanContext_->getDevice(), graphicsCommandPool_, nullptr);
        graphicsCommandPool_ = VK_NULL_HANDLE;
    }
    if (computeCommandPool_ != VK_NULL_HANDLE) {
        vkDestroyCommandPool(vulkanContext_->getDevice(), computeCommandPool_, nullptr);
        computeCommandPool_ = VK_NULL_HANDLE;
    }

    // Cleanup sync objects
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        if (imageAvailableSemaphores_[i] != VK_NULL_HANDLE) {
            vkDestroySemaphore(vulkanContext_->getDevice(), imageAvailableSemaphores_[i], nullptr);
            imageAvailableSemaphores_[i] = VK_NULL_HANDLE;
        }
        if (renderFinishedSemaphores_[i] != VK_NULL_HANDLE) {
            vkDestroySemaphore(vulkanContext_->getDevice(), renderFinishedSemaphores_[i], nullptr);
            renderFinishedSemaphores_[i] = VK_NULL_HANDLE;
        }
        if (inFlightFences_[i] != VK_NULL_HANDLE) {
            vkDestroyFence(vulkanContext_->getDevice(), inFlightFences_[i], nullptr);
            inFlightFences_[i] = VK_NULL_HANDLE;
        }
    }

    // Cleanup Vulkan context
    vulkanContext_.reset();

    // Cleanup window manager
    windowManager_.reset();
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
    // Free existing command buffers if any
    if (!commandBuffers_.empty()) {
        vkFreeCommandBuffers(vulkanContext_->getDevice(), graphicsCommandPool_, 
                           static_cast<uint32_t>(commandBuffers_.size()), commandBuffers_.data());
        commandBuffers_.clear();
    }

    commandBuffers_.resize(MAX_FRAMES_IN_FLIGHT);

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = graphicsCommandPool_;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = static_cast<uint32_t>(commandBuffers_.size());

    if (vkAllocateCommandBuffers(vulkanContext_->getDevice(), &allocInfo, commandBuffers_.data()) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate command buffers!");
    }
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
    swapChainImages_.resize(imageCount);
    vkGetSwapchainImagesKHR(vulkanContext_->getDevice(), swapChain_, &imageCount, swapChainImages_.data());

    swapChainImageFormat_ = surfaceFormat.format;
    swapChainExtent_ = extent;
}

void VulkanEngine::createImageViews() {
    swapChainImageViews_.resize(swapChainImages_.size());

    for (size_t i = 0; i < swapChainImages_.size(); i++) {
        VkImageViewCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        createInfo.image = swapChainImages_[i];
        createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        createInfo.format = swapChainImageFormat_;
        createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        createInfo.subresourceRange.baseMipLevel = 0;
        createInfo.subresourceRange.levelCount = 1;
        createInfo.subresourceRange.baseArrayLayer = 0;
        createInfo.subresourceRange.layerCount = 1;

        if (vkCreateImageView(vulkanContext_->getDevice(), &createInfo, nullptr, &swapChainImageViews_[i]) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create image views!");
        }
    }
}

void VulkanEngine::cleanupSwapChain() {
    for (auto imageView : swapChainImageViews_) {
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

    // Get surface capabilities
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, vulkanContext_->getSurface(), &details.capabilities);

    // Get surface formats
    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, vulkanContext_->getSurface(), &formatCount, nullptr);
    if (formatCount != 0) {
        details.formats.resize(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, vulkanContext_->getSurface(), &formatCount, details.formats.data());
    }

    // Get present modes
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
    colorAttachment.format = swapChainImageFormat_;
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
    swapChainFramebuffers_.resize(swapChainImageViews_.size());

    for (size_t i = 0; i < swapChainImageViews_.size(); i++) {
        std::array<VkImageView, 2> attachments = {
            swapChainImageViews_[i],
            depthImageView_
        };

        VkFramebufferCreateInfo framebufferInfo{};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = renderPass_;
        framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
        framebufferInfo.pAttachments = attachments.data();
        framebufferInfo.width = swapChainExtent_.width;
        framebufferInfo.height = swapChainExtent_.height;
        framebufferInfo.layers = 1;

        if (vkCreateFramebuffer(vulkanContext_->getDevice(), &framebufferInfo, nullptr, &swapChainFramebuffers_[i]) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create framebuffer!");
        }
    }
}

void VulkanEngine::createDepthResources() {
    VkFormat depthFormat = findDepthFormat();
    auto& memoryManager = vulkanContext_->getMemoryManager();

    auto depthImageAlloc = memoryManager.allocateImage(
        swapChainExtent_.width,
        swapChainExtent_.height,
        depthFormat,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
    );
    depthImage_ = depthImageAlloc.image;
    depthImageAllocation_ = depthImageAlloc.allocation;

    memoryManager.createImageView(depthImage_, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT, depthImageView_);
}

void VulkanEngine::createColorResources() {
    VkFormat colorFormat = swapChainImageFormat_;
    auto& memoryManager = vulkanContext_->getMemoryManager();

    auto colorImageAlloc = memoryManager.allocateImage(
        swapChainExtent_.width,
        swapChainExtent_.height,
        colorFormat,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
    );
    colorImage_ = colorImageAlloc.image;
    colorImageAllocation_ = colorImageAlloc.allocation;

    memoryManager.createImageView(colorImage_, colorFormat, VK_IMAGE_ASPECT_COLOR_BIT, colorImageView_);
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

void VulkanEngine::createVertexBuffer() {
    VkDeviceSize bufferSize = sizeof(vertices_[0]) * vertices_.size();
    auto& memoryManager = vulkanContext_->getMemoryManager();

    // Create staging buffer
    auto stagingBuffer = memoryManager.createStagingBuffer(bufferSize);
    void* data = memoryManager.mapStagingBuffer(stagingBuffer);
    memcpy(data, vertices_.data(), (size_t)bufferSize);
    memoryManager.unmapStagingBuffer(stagingBuffer);

    // Create vertex buffer
    auto vertexBufferAlloc = memoryManager.createBuffer(
        bufferSize,
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VMA_MEMORY_USAGE_GPU_ONLY
    );
    vertexBuffer_ = vertexBufferAlloc.buffer;
    vertexBufferAllocation_ = vertexBufferAlloc.allocation;

    // Copy data
    copyBuffer(stagingBuffer.buffer, vertexBuffer_, bufferSize);

    // Destroy staging buffer
    memoryManager.destroyStagingBuffer(stagingBuffer);
}

void VulkanEngine::createIndexBuffer() {
    VkDeviceSize bufferSize = sizeof(indices_[0]) * indices_.size();
    auto& memoryManager = vulkanContext_->getMemoryManager();

    // Create staging buffer
    auto stagingBuffer = memoryManager.createStagingBuffer(bufferSize);
    void* data = memoryManager.mapStagingBuffer(stagingBuffer);
    memcpy(data, indices_.data(), (size_t)bufferSize);
    memoryManager.unmapStagingBuffer(stagingBuffer);

    // Create index buffer
    auto indexBufferAlloc = memoryManager.createBuffer(
        bufferSize,
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        VMA_MEMORY_USAGE_GPU_ONLY
    );
    indexBuffer_ = indexBufferAlloc.buffer;
    indexBufferAllocation_ = indexBufferAlloc.allocation;

    // Copy data
    copyBuffer(stagingBuffer.buffer, indexBuffer_, bufferSize);

    // Destroy staging buffer
    memoryManager.destroyStagingBuffer(stagingBuffer);
}

void VulkanEngine::createUniformBuffers() {
    VkDeviceSize bufferSize = sizeof(UniformBufferObject);
    auto& memoryManager = vulkanContext_->getMemoryManager();

    uniformBuffers_.resize(MAX_FRAMES_IN_FLIGHT);
    uniformBufferAllocations_.resize(MAX_FRAMES_IN_FLIGHT);
    uniformBuffersMapped_.resize(MAX_FRAMES_IN_FLIGHT);

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        auto allocation = memoryManager.createBuffer(
            bufferSize,
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VMA_MEMORY_USAGE_CPU_TO_GPU
        );
        uniformBuffers_[i] = allocation.buffer;
        uniformBufferAllocations_[i] = allocation.allocation;
        uniformBuffersMapped_[i] = memoryManager.mapMemory(allocation);
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

    descriptorSets_.resize(MAX_FRAMES_IN_FLIGHT);
    if (vkAllocateDescriptorSets(vulkanContext_->getDevice(), &allocInfo, descriptorSets_.data()) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate descriptor sets!");
    }

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        VkDescriptorBufferInfo bufferInfo{};
        bufferInfo.buffer = uniformBuffers_[i];
        bufferInfo.offset = 0;
        bufferInfo.range = sizeof(UniformBufferObject);

        VkDescriptorImageInfo imageInfo{};
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfo.imageView = textureImageView_;
        imageInfo.sampler = textureSampler_;

        std::array<VkWriteDescriptorSet, 2> descriptorWrites{};

        descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[0].dstSet = descriptorSets_[i];
        descriptorWrites[0].dstBinding = 0;
        descriptorWrites[0].dstArrayElement = 0;
        descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptorWrites[0].descriptorCount = 1;
        descriptorWrites[0].pBufferInfo = &bufferInfo;

        descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[1].dstSet = descriptorSets_[i];
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
    imageAvailableSemaphores_.resize(MAX_FRAMES_IN_FLIGHT);
    renderFinishedSemaphores_.resize(MAX_FRAMES_IN_FLIGHT);
    inFlightFences_.resize(MAX_FRAMES_IN_FLIGHT);

    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        if (vkCreateSemaphore(vulkanContext_->getDevice(), &semaphoreInfo, nullptr, &imageAvailableSemaphores_[i]) != VK_SUCCESS ||
            vkCreateSemaphore(vulkanContext_->getDevice(), &semaphoreInfo, nullptr, &renderFinishedSemaphores_[i]) != VK_SUCCESS ||
            vkCreateFence(vulkanContext_->getDevice(), &fenceInfo, nullptr, &inFlightFences_[i]) != VK_SUCCESS) {
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
    
    float aspectRatio = static_cast<float>(swapChainExtent_.width) / static_cast<float>(swapChainExtent_.height);
    ubo.proj = glm::perspective(glm::radians(45.0f), aspectRatio, 0.1f, 10.0f);
    ubo.proj[1][1] *= -1;

    memcpy(uniformBuffersMapped_[currentImage], &ubo, sizeof(ubo));
}

void VulkanEngine::drawMenu() {
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    ImGui::SetNextWindowPos({0,0}, ImGuiCond_Always);
    ImGui::SetNextWindowSize({400,300}, ImGuiCond_Always);
    ImGui::Begin("Main Menu", nullptr,
                 ImGuiWindowFlags_NoResize |
                 ImGuiWindowFlags_NoCollapse |
                 ImGuiWindowFlags_NoMove);

    if (ImGui::Button("Continue", ImVec2(-1,40))) {
        loadLastSave();
        currentState_ = App::State::Running;
    }

    if (ImGui::Button("Load Save...", ImVec2(-1,40))) {
        showSavePicker_ = true;
    }

    if (ImGui::Button("New Project", ImVec2(-1,40))) {
        newProject();
        currentState_ = App::State::Running;
    }

    if (ImGui::Button("Exit", ImVec2(-1,40))) {
        currentState_ = App::State::Exiting;
    }
    ImGui::End();

    if (showSavePicker_) drawSavePicker();

    ImGui::Render();
    // ImGui rendering handled in drawFrame
}

void VulkanEngine::drawSavePicker() {
    ImGui::SetNextWindowPos({100,100}, ImGuiCond_Always);
    ImGui::SetNextWindowSize({300,400}, ImGuiCond_Always);
    ImGui::Begin("Load Save", &showSavePicker_,
                 ImGuiWindowFlags_NoResize |
                 ImGuiWindowFlags_NoCollapse);

    saveFiles_ = saveManager_->getSaveFiles();
    for (size_t i = 0; i < saveFiles_.size(); ++i) {
        if (ImGui::Selectable(saveFiles_[i].displayName.c_str(), selectedSaveIndex_ == i)) {
            selectedSaveIndex_ = i;
        }
    }

    if (ImGui::Button("Load", ImVec2(-1,40)) && selectedSaveIndex_ >= 0) {
        loadSave(saveFiles_[selectedSaveIndex_].filename);
        currentState_ = App::State::Running;
        showSavePicker_ = false;
    }

    ImGui::End();
}

void VulkanEngine::newProject() {
    // Create a new empty VoxelData instance
    loadedVoxelData_ = VoxelData();
    
    // Add some default voxels to demonstrate functionality
    Voxel defaultVoxel;
    defaultVoxel.position = glm::vec3(0.0f, 0.0f, 0.0f);
    defaultVoxel.color = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f); // Red
    defaultVoxel.type = 0;
    defaultVoxel.active = true;
    loadedVoxelData_.addVoxel(defaultVoxel);

    // Add a few more voxels in a pattern
    for (int x = -1; x <= 1; x++) {
        for (int y = -1; y <= 1; y++) {
            if (x == 0 && y == 0) continue; // Skip center (already added)
            Voxel voxel;
            voxel.position = glm::vec3(x * 0.5f, y * 0.5f, 0.0f);
            voxel.color = glm::vec4(0.0f, 1.0f, 0.0f, 1.0f); // Green
            voxel.type = 1;
            voxel.active = true;
            loadedVoxelData_.addVoxel(voxel);
        }
    }

    // Create vertex buffers for the voxels
    createVoxelBuffers();

    // Transition to running state
    currentState_ = App::State::Running;
}

void VulkanEngine::loadLastSave() {
    std::string lastSave = saveManager_->getLastSaveFile();
    if (!lastSave.empty()) {
        loadSave(lastSave);
    }
}

void VulkanEngine::loadSave(const std::string& filename) {
    std::lock_guard<std::mutex> lock(loadingMutex_);
    if (isLoading_) return;
    
    isLoading_ = true;
    loadingElapsed_ = 0.0f;
    loadingProgress_ = 0.0f;
    loadingStatus_ = "Loading save file...";
    shouldCancelLoading_ = false;
    currentState_ = App::State::Loading;

    loadingFuture_ = std::async(std::launch::async, [this, filename]() {
        try {
            // Phase 1: Read file (25%)
            updateLoadingState("Reading save file...", 0.25f);
            if (shouldCancelLoading_) {
                updateLoadingState("Loading cancelled", 0.0f);
                return false;
            }
            
            VoxelData temp;
            bool ok = saveManager_->loadSaveFile(filename, temp);
            if (!ok) {
                std::string error = saveManager_->getLastError();
                updateLoadingState("Failed to load save file: " + error, 0.0f);
                return false;
            }

            // Phase 2: Process voxel data (50%)
            updateLoadingState("Processing voxel data...", 0.75f);
            if (shouldCancelLoading_) {
                updateLoadingState("Loading cancelled", 0.0f);
                return false;
            }
            
            {
                std::lock_guard<std::mutex> lock(loadingMutex_);
                loadedVoxelData_ = std::move(temp);
            }

            // Phase 3: Complete (100%)
            updateLoadingState("Complete!", 1.0f);
            return true;
        }
        catch (const SaveManager::SaveError& e) {
            updateLoadingState(std::string("Save error: ") + e.what(), 0.0f);
            return false;
        }
        catch (const std::exception& e) {
            updateLoadingState(std::string("Error: ") + e.what(), 0.0f);
            return false;
        }
    });
}

void VulkanEngine::saveCurrent() {
    std::string filename = generateSaveFileName("save_");
    // If SaveManager::saveCurrentState requires more arguments, pass them here
    saveManager_->saveCurrentState(filename, loadedVoxelData_);
}

void VulkanEngine::setAppState(App::State newState) {
    currentState_ = newState;
}

void VulkanEngine::drawLoading() {
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    ImGui::SetNextWindowPos({static_cast<float>(WIDTH/2 - 150), static_cast<float>(HEIGHT/2 - 50)}, ImGuiCond_Always);
    ImGui::SetNextWindowSize({300.0f, 100.0f}, ImGuiCond_Always);
    ImGui::Begin("Loading", nullptr,
                 ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar);

    std::string status;
    float progress;
    {
        std::lock_guard<std::mutex> lock(loadingMutex_);
        status = loadingStatus_;
        progress = loadingProgress_;
    }
    
    ImGui::Text("%s", status.c_str());

    if (loadingFuture_.valid() && loadingFuture_.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready) {
        bool success = false;
        try {
            success = loadingFuture_.get();
        } catch (const std::exception& e) {
            std::cerr << "Loading failed: " << e.what() << std::endl;
            success = false;
        }

        std::lock_guard<std::mutex> lock(loadingMutex_);
        if (success) {
            // Create vertex buffers for the loaded voxel data
            createVoxelBuffers();
            currentState_ = App::State::Running;
        } else {
            currentState_ = App::State::Menu;
        }
        isLoading_ = false;
    }

    ImGui::ProgressBar(progress, ImVec2(-1.0f, 0.0f));

    ImGui::End();

    ImGui::Render();
    // ImGui rendering handled in drawFrame
}

void VulkanEngine::updateLoadingState(const std::string& status, float progress) {
    std::lock_guard<std::mutex> lock(loadingMutex_);
    loadingStatus_ = status;
    loadingProgress_ = progress;
}

void VulkanEngine::cancelLoading() {
    std::lock_guard<std::mutex> lock(loadingMutex_);
    if (!isLoading_) return;
    
    shouldCancelLoading_ = true;
    updateLoadingState("Cancelling...", loadingProgress_);
}

bool VulkanEngine::isAutoSaveDue() const {
    std::lock_guard<std::mutex> lock(saveMutex_);
    if (!autoSaveEnabled_) return false;
    auto now = std::chrono::steady_clock::now();
    return (now - lastAutoSave_) >= AUTO_SAVE_INTERVAL;
}

void VulkanEngine::performAutoSave() {
    try {
        std::string filename = generateSaveFileName(AUTO_SAVE_PREFIX);
        if (saveManager_->saveCurrentState(filename, loadedVoxelData_)) {
            std::cout << "Auto-save completed: " << filename << std::endl;
        } else {
            std::cerr << "Auto-save failed: " << saveManager_->getLastError() << std::endl;
        }
    }
    catch (const SaveManager::SaveError& e) {
        std::cerr << "Auto-save error: " << e.what() << std::endl;
    }
    catch (const std::exception& e) {
        std::cerr << "Unexpected auto-save error: " << e.what() << std::endl;
    }
}

void VulkanEngine::cleanupOldAutoSaves() {
    try {
        auto saves = saveManager_->getSaveFiles();
        std::vector<std::string> autoSaves;
        
        // Collect auto-save filenames
        for (const auto& save : saves) {
            if (save.filename.find(AUTO_SAVE_PREFIX) == 0) {
                autoSaves.push_back(save.filename);
            }
        }

        // Sort by filename (which includes timestamp)
        std::sort(autoSaves.begin(), autoSaves.end());

        // Remove oldest auto-saves if we have too many
        while (autoSaves.size() > MAX_AUTO_SAVES) {
            if (!saveManager_->deleteSaveFile(autoSaves.front())) {
                std::cerr << "Failed to delete old auto-save: " << saveManager_->getLastError() << std::endl;
            }
            autoSaves.erase(autoSaves.begin());
        }
    }
    catch (const SaveManager::SaveError& e) {
        std::cerr << "Error cleaning up auto-saves: " << e.what() << std::endl;
    }
    catch (const std::exception& e) {
        std::cerr << "Unexpected error cleaning up auto-saves: " << e.what() << std::endl;
    }
}

void VulkanEngine::performManualSave() {
    try {
        std::string filename = generateSaveFileName(MANUAL_SAVE_PREFIX);
        if (saveManager_->saveCurrentState(filename, loadedVoxelData_)) {
            std::cout << "Manual save completed: " << filename << std::endl;
        } else {
            std::cerr << "Manual save failed: " << saveManager_->getLastError() << std::endl;
        }
    }
    catch (const SaveManager::SaveError& e) {
        std::cerr << "Manual save error: " << e.what() << std::endl;
    }
    catch (const std::exception& e) {
        std::cerr << "Unexpected manual save error: " << e.what() << std::endl;
    }
}

std::string VulkanEngine::generateSaveFileName(const char* prefix) const {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << prefix << std::put_time(std::localtime(&time), "%Y%m%d_%H%M%S");
    return ss.str();
}

void VulkanEngine::createVoxelBuffers() {
    // Create vertex data for voxels
    createVoxelVertexData(loadedVoxelData_);

    // Create vertex buffer
    VkDeviceSize vertexBufferSize = sizeof(Vertex) * voxelVertices_.size();
    auto memoryManager = vulkanContext_->getMemoryManager();
    if (!memoryManager) {
        throw std::runtime_error("Memory manager not available for voxel buffer creation");
    }

    // Create staging buffer for vertices
    auto vertexStagingBuffer = memoryManager->createStagingBuffer(vertexBufferSize);
    void* vertexData = memoryManager->mapStagingBuffer(vertexStagingBuffer);
    memcpy(vertexData, voxelVertices_.data(), vertexBufferSize);
    memoryManager->unmapStagingBuffer(vertexStagingBuffer);

    // Create vertex buffer
    auto vertexBufferAlloc = memoryManager->createBuffer(
        vertexBufferSize,
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VMA_MEMORY_USAGE_GPU_ONLY
    );
    voxelVertexBuffer_ = vertexBufferAlloc.buffer;
    voxelVertexBufferAllocation_ = vertexBufferAlloc.allocation;

    // Copy vertex data
    copyBuffer(vertexStagingBuffer.buffer, voxelVertexBuffer_, vertexBufferSize);

    // Create index buffer
    VkDeviceSize indexBufferSize = sizeof(uint32_t) * voxelIndices_.size();
    auto indexStagingBuffer = memoryManager->createStagingBuffer(indexBufferSize);
    void* indexData = memoryManager->mapStagingBuffer(indexStagingBuffer);
    memcpy(indexData, voxelIndices_.data(), indexBufferSize);
    memoryManager->unmapStagingBuffer(indexStagingBuffer);

    // Create index buffer
    auto indexBufferAlloc = memoryManager->createBuffer(
        indexBufferSize,
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        VMA_MEMORY_USAGE_GPU_ONLY
    );
    voxelIndexBuffer_ = indexBufferAlloc.buffer;
    voxelIndexBufferAllocation_ = indexBufferAlloc.allocation;

    // Copy index data
    copyBuffer(indexStagingBuffer.buffer, voxelIndexBuffer_, indexBufferSize);

    // Clean up staging buffers
    memoryManager->destroyStagingBuffer(vertexStagingBuffer);
    memoryManager->destroyStagingBuffer(indexStagingBuffer);
}

void VulkanEngine::createVoxelVertexData(const VoxelData& voxelData) {
    voxelVertices_.clear();
    voxelIndices_.clear();

    // Create a cube for each voxel
    for (const auto& voxel : voxelData.getVoxels()) {
        if (!voxel.active) continue;

        // Calculate cube vertices
        glm::vec3 pos = voxel.position;
        glm::vec3 color = glm::vec3(voxel.color);
        float size = 0.5f; // Half-size of the cube

        // Add vertices for the cube
        uint32_t baseIndex = static_cast<uint32_t>(voxelVertices_.size());

        // Front face
        voxelVertices_.push_back({{pos.x - size, pos.y - size, pos.z + size}, color, {0.0f, 0.0f}});
        voxelVertices_.push_back({{pos.x + size, pos.y - size, pos.z + size}, color, {1.0f, 0.0f}});
        voxelVertices_.push_back({{pos.x + size, pos.y + size, pos.z + size}, color, {1.0f, 1.0f}});
        voxelVertices_.push_back({{pos.x - size, pos.y + size, pos.z + size}, color, {0.0f, 1.0f}});

        // Back face
        voxelVertices_.push_back({{pos.x - size, pos.y - size, pos.z - size}, color, {1.0f, 0.0f}});
        voxelVertices_.push_back({{pos.x - size, pos.y + size, pos.z - size}, color, {1.0f, 1.0f}});
        voxelVertices_.push_back({{pos.x + size, pos.y + size, pos.z - size}, color, {0.0f, 1.0f}});
        voxelVertices_.push_back({{pos.x + size, pos.y - size, pos.z - size}, color, {0.0f, 0.0f}});

        // Add indices for the cube
        // Front face
        voxelIndices_.push_back(baseIndex + 0);
        voxelIndices_.push_back(baseIndex + 1);
        voxelIndices_.push_back(baseIndex + 2);
        voxelIndices_.push_back(baseIndex + 2);
        voxelIndices_.push_back(baseIndex + 3);
        voxelIndices_.push_back(baseIndex + 0);

        // Back face
        voxelIndices_.push_back(baseIndex + 4);
        voxelIndices_.push_back(baseIndex + 5);
        voxelIndices_.push_back(baseIndex + 6);
        voxelIndices_.push_back(baseIndex + 6);
        voxelIndices_.push_back(baseIndex + 7);
        voxelIndices_.push_back(baseIndex + 4);

        // Top face
        voxelIndices_.push_back(baseIndex + 3);
        voxelIndices_.push_back(baseIndex + 2);
        voxelIndices_.push_back(baseIndex + 6);
        voxelIndices_.push_back(baseIndex + 6);
        voxelIndices_.push_back(baseIndex + 5);
        voxelIndices_.push_back(baseIndex + 3);

        // Bottom face
        voxelIndices_.push_back(baseIndex + 0);
        voxelIndices_.push_back(baseIndex + 4);
        voxelIndices_.push_back(baseIndex + 7);
        voxelIndices_.push_back(baseIndex + 7);
        voxelIndices_.push_back(baseIndex + 1);
        voxelIndices_.push_back(baseIndex + 0);

        // Right face
        voxelIndices_.push_back(baseIndex + 1);
        voxelIndices_.push_back(baseIndex + 7);
        voxelIndices_.push_back(baseIndex + 6);
        voxelIndices_.push_back(baseIndex + 6);
        voxelIndices_.push_back(baseIndex + 2);
        voxelIndices_.push_back(baseIndex + 1);

        // Left face
        voxelIndices_.push_back(baseIndex + 0);
        voxelIndices_.push_back(baseIndex + 3);
        voxelIndices_.push_back(baseIndex + 5);
        voxelIndices_.push_back(baseIndex + 5);
        voxelIndices_.push_back(baseIndex + 4);
        voxelIndices_.push_back(baseIndex + 0);
    }
}

void VulkanEngine::cleanupVoxelBuffers() {
    if (vulkanContext_ && vulkanContext_->getMemoryManager()) {
        auto memoryManager = vulkanContext_->getMemoryManager();
        
        if (voxelVertexBuffer_ != VK_NULL_HANDLE) {
            memoryManager->destroyBuffer({voxelVertexBuffer_, voxelVertexBufferAllocation_, {}, 0, nullptr});
            voxelVertexBuffer_ = VK_NULL_HANDLE;
        }
        if (voxelIndexBuffer_ != VK_NULL_HANDLE) {
            memoryManager->destroyBuffer({voxelIndexBuffer_, voxelIndexBufferAllocation_, {}, 0, nullptr});
            voxelIndexBuffer_ = VK_NULL_HANDLE;
        }
    }
}

void VulkanEngine::createGraphicsPipeline() {
    VkDevice device = vulkanContext_->getDevice();

    // Load shaders
    auto vertShaderCode = readFile("shaders/basic.vert.spv");
    auto fragShaderCode = readFile("shaders/basic.frag.spv");
    ShaderModule vertShaderModule(createShaderModule(vertShaderCode));
    ShaderModule fragShaderModule(createShaderModule(fragShaderCode));

    VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
    vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertShaderStageInfo.module = vertShaderModule;
    vertShaderStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
    fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragShaderStageInfo.module = fragShaderModule;
    fragShaderStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo, fragShaderStageInfo};

    // Vertex input
    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    
    auto bindingDescription = Vertex::getBindingDescription();
    auto attributeDescriptions = Vertex::getAttributeDescriptions();
    
    vertexInputInfo.vertexBindingDescriptionCount = 1;
    vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
    vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
    vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

    // Input assembly
    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    // Viewport and scissor (dynamic)
    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.pViewports = nullptr; // Dynamic
    viewportState.scissorCount = 1;
    viewportState.pScissors = nullptr; // Dynamic

    // Rasterizer
    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizer.depthBiasEnable = VK_FALSE;

    // Multisampling
    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    // Depth and stencil
    VkPipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable = VK_TRUE;
    depthStencil.depthWriteEnable = VK_TRUE;
    depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
    depthStencil.depthBoundsTestEnable = VK_FALSE;
    depthStencil.stencilTestEnable = VK_FALSE;

    // Color blending
    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                          VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;

    // Dynamic state
    std::vector<VkDynamicState> dynamicStates = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
    };
    VkPipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
    dynamicState.pDynamicStates = dynamicStates.data();

    // Pipeline layout
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &descriptorSetLayout_;

    if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipelineLayout_) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create pipeline layout!");
    }

    // Graphics pipeline
    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = shaderStages;
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pDepthStencilState = &depthStencil;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDynamicState = &dynamicState;
    pipelineInfo.layout = pipelineLayout_;
    pipelineInfo.renderPass = renderPass_;
    pipelineInfo.subpass = 0;
    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
    pipelineInfo.basePipelineIndex = -1;

    if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &graphicsPipeline_) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create graphics pipeline!");
    }
}

void VulkanEngine::initImGui() {
    // Create descriptor pool for ImGui
    createImGuiDescriptorPool();

    // Setup ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

    // Setup ImGui style
    ImGui::StyleColorsDark();

    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForVulkan(windowManager_->getWindow(), true);
    ImGui_ImplVulkan_InitInfo init_info = {};
    init_info.Instance = vulkanContext_->getInstance();
    init_info.PhysicalDevice = vulkanContext_->getPhysicalDevice();
    init_info.Device = vulkanContext_->getDevice();
    init_info.QueueFamily = queueFamilyIndices_.graphicsFamily.value();
    init_info.Queue = vulkanContext_->getGraphicsQueue();
    init_info.PipelineCache = VK_NULL_HANDLE;
    init_info.DescriptorPool = imguiDescriptorPool_;
    init_info.Subpass = 0;
    init_info.MinImageCount = 2;
    init_info.ImageCount = MAX_FRAMES_IN_FLIGHT;
    init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    init_info.Allocator = nullptr;
    init_info.CheckVkResultFn = nullptr;
    ImGui_ImplVulkan_Init(&init_info, renderPass_);

    // Upload fonts
    VkCommandBuffer command_buffer = beginSingleTimeCommands();
    ImGui_ImplVulkan_CreateFontsTexture(command_buffer);
    endSingleTimeCommands(command_buffer);
    ImGui_ImplVulkan_DestroyFontUploadObjects();

    imguiInitialized_ = true;
}

void VulkanEngine::cleanupImGui() {
    if (imguiInitialized_) {
        ImGui_ImplVulkan_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        cleanupImGuiDescriptorPool();
        imguiInitialized_ = false;
    }
}

void VulkanEngine::beginImGuiFrame() {
    if (imguiInitialized_) {
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
    }
}

void VulkanEngine::endImGuiFrame() {
    if (imguiInitialized_) {
        ImGui::Render();
        ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), commandBuffers_[currentFrame_]);

        // Update and Render additional Platform Windows
        if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
        }
    }
}

void VulkanEngine::createImGuiDescriptorPool() {
    VkDescriptorPoolSize pool_sizes[] = {
        { VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
        { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
        { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 }
    };

    VkDescriptorPoolCreateInfo pool_info = {};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    pool_info.maxSets = 1000;
    pool_info.poolSizeCount = std::size(pool_sizes);
    pool_info.pPoolSizes = pool_sizes;

    if (vkCreateDescriptorPool(vulkanContext_->getDevice(), &pool_info, nullptr, &imguiDescriptorPool_) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create ImGui descriptor pool!");
    }
}

void VulkanEngine::cleanupImGuiDescriptorPool() {
    if (imguiDescriptorPool_ != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(vulkanContext_->getDevice(), imguiDescriptorPool_, nullptr);
        imguiDescriptorPool_ = VK_NULL_HANDLE;
    }
}

} // namespace VulkanHIP
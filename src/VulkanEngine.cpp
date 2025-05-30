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
#include "VulkanBufferManager.h"
#include "vulkan/resources/VulkanImageManager.h"
#include "vulkan/rendering/VulkanSwapChain.h"
#include "vulkan/rendering/VulkanRenderer.h"
#include "vulkan/rendering/VoxelRenderer.h"
#include "vulkan/resources/VulkanFramebuffer.h"
#include "vulkan/compute/VulkanCompute.h"
#include "vulkan/ui/VulkanImGui.h"

namespace VulkanHIP {

// Initialize the static instance pointer
VulkanEngine* VulkanEngine::instance_ = nullptr;

// Constructor
VulkanEngine::VulkanEngine()
    : currentFrame_(0),
      framebufferResized_(false),
      device_(VK_NULL_HANDLE),
      physicalDevice_(VK_NULL_HANDLE),
      graphicsQueue_(VK_NULL_HANDLE),
      presentQueue_(VK_NULL_HANDLE),
      computeQueue_(VK_NULL_HANDLE),
      surface_(VK_NULL_HANDLE),
      imguiInitialized_(false),
      loadingElapsed_(0.0f),
      isLoading_(false),
      lastAutoSave_(std::chrono::steady_clock::now()),
      shouldCancelLoading_(false),
      loadingProgress_(0.0f),
      camera_(nullptr),  // Will be initialized properly in init()
      currentState_(App::State::Menu),
      isPaused_(false),
      showSavePicker_(false),
      selectedSaveIndex_(-1),
      autoSaveEnabled_(true),
      gridWidth_(64),
      gridHeight_(64),
      gridDepth_(64),
      currentRuleSet_(0),
      surviveMin_(2),
      surviveMax_(3),
      birthCount_(3),
      ruleSet_(0),
      renderMode_(0),
      minLODDistance_(10.0f),
      maxLODDistance_(100.0f),
      msaaSamples_(VK_SAMPLE_COUNT_1_BIT),
      currentComputeFrame_(0),
      renderPass_(VK_NULL_HANDLE),
      graphicsPipeline_(VK_NULL_HANDLE),
      pipelineLayout_(VK_NULL_HANDLE),
      descriptorSetLayout_(VK_NULL_HANDLE),
      descriptorPool_(VK_NULL_HANDLE),
      imguiDescriptorPool_(VK_NULL_HANDLE),
      computeDescriptorSetLayout_(VK_NULL_HANDLE),
      computeDescriptorPool_(VK_NULL_HANDLE),
      graphicsCommandPool_(VK_NULL_HANDLE),
      computeCommandPool_(VK_NULL_HANDLE),
      swapChain_(VK_NULL_HANDLE),
      swapChainImageFormat_(VK_FORMAT_UNDEFINED),
      swapChainExtent_({0, 0}),
      depthImage_(VK_NULL_HANDLE),
      depthImageView_(VK_NULL_HANDLE),
      depthImageAllocation_(VK_NULL_HANDLE),
      colorImage_(VK_NULL_HANDLE),
      colorImageView_(VK_NULL_HANDLE),
      colorImageAllocation_(VK_NULL_HANDLE),
      voxelVertexBuffer_(VK_NULL_HANDLE),
      voxelVertexBufferAllocation_(VK_NULL_HANDLE),
      voxelIndexBuffer_(VK_NULL_HANDLE),
      voxelIndexBufferAllocation_(VK_NULL_HANDLE),
      voxelInstanceBuffer_(VK_NULL_HANDLE),
      voxelInstanceBufferAllocation_(VK_NULL_HANDLE)
{
    if (instance_ != nullptr) {
        throw std::runtime_error("VulkanEngine instance already exists!");
    }
    instance_ = this;
    
    // Initialize enabledFeatures_
    memset(&enabledFeatures_, 0, sizeof(VkPhysicalDeviceFeatures));
    
    // Set up vertex data for a cube
    vertices_ = {
        // Front face
        {{-0.5f, -0.5f,  0.5f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}},
        {{ 0.5f, -0.5f,  0.5f}, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f}},
        {{ 0.5f,  0.5f,  0.5f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},
        {{-0.5f,  0.5f,  0.5f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}},
        
        // Back face
        {{-0.5f, -0.5f, -0.5f}, {0.0f, 0.0f, -1.0f}, {1.0f, 0.0f}},
        {{ 0.5f, -0.5f, -0.5f}, {0.0f, 0.0f, -1.0f}, {0.0f, 0.0f}},
        {{ 0.5f,  0.5f, -0.5f}, {0.0f, 0.0f, -1.0f}, {0.0f, 1.0f}},
        {{-0.5f,  0.5f, -0.5f}, {0.0f, 0.0f, -1.0f}, {1.0f, 1.0f}},
        
        // Top face
        {{-0.5f,  0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}},
        {{ 0.5f,  0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}},
        {{ 0.5f,  0.5f,  0.5f}, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f}},
        {{-0.5f,  0.5f,  0.5f}, {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f}},
        
        // Bottom face
        {{-0.5f, -0.5f, -0.5f}, {0.0f, -1.0f, 0.0f}, {0.0f, 1.0f}},
        {{ 0.5f, -0.5f, -0.5f}, {0.0f, -1.0f, 0.0f}, {1.0f, 1.0f}},
        {{ 0.5f, -0.5f,  0.5f}, {0.0f, -1.0f, 0.0f}, {1.0f, 0.0f}},
        {{-0.5f, -0.5f,  0.5f}, {0.0f, -1.0f, 0.0f}, {0.0f, 0.0f}},
        
        // Right face
        {{ 0.5f, -0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
        {{ 0.5f,  0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f}},
        {{ 0.5f,  0.5f,  0.5f}, {1.0f, 0.0f, 0.0f}, {1.0f, 1.0f}},
        {{ 0.5f, -0.5f,  0.5f}, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}},
        
        // Left face
        {{-0.5f, -0.5f, -0.5f}, {-1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}},
        {{-0.5f,  0.5f, -0.5f}, {-1.0f, 0.0f, 0.0f}, {1.0f, 1.0f}},
        {{-0.5f,  0.5f,  0.5f}, {-1.0f, 0.0f, 0.0f}, {0.0f, 1.0f}},
        {{-0.5f, -0.5f,  0.5f}, {-1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}}
    };

    // Set up index data for a cube
    indices_ = {
        0, 1, 2, 2, 3, 0,       // Front
        4, 5, 6, 6, 7, 4,       // Back
        8, 9, 10, 10, 11, 8,    // Top
        12, 13, 14, 14, 15, 12, // Bottom
        16, 17, 18, 18, 19, 16, // Right
        20, 21, 22, 22, 23, 20  // Left
    };
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
    try {
        // Initialize window manager
        windowManager_ = std::make_unique<WindowManager>();
        windowManager_->init(WIDTH, HEIGHT, WINDOW_TITLE);
        windowManager_->setFramebufferResizeCallback([](GLFWwindow* window, int width, int height) {
            auto engine = reinterpret_cast<VulkanEngine*>(glfwGetWindowUserPointer(window));
            engine->framebufferResized_ = true;
        });

        // Initialize Vulkan context
        vulkanContext_ = std::make_unique<VulkanContext>();
        vulkanContext_->init(windowManager_->getWindow());

        // Get device and queues
        device_ = vulkanContext_->getDevice();
        physicalDevice_ = vulkanContext_->getPhysicalDevice();
        graphicsQueue_ = vulkanContext_->getGraphicsQueue();
        presentQueue_ = vulkanContext_->getPresentQueue();
        computeQueue_ = vulkanContext_->getComputeQueue();
        queueFamilyIndices_ = vulkanContext_->getQueueFamilyIndices();

        // Initialize memory manager
        memoryManager_ = std::make_unique<VulkanMemoryManager>(device_, physicalDevice_);

        // Initialize managers in correct order
        bufferManager_ = std::make_unique<VulkanBufferManager>(vulkanContext_.get(), memoryManager_.get());
        imageManager_ = std::make_unique<VulkanImageManager>(vulkanContext_.get(), memoryManager_.get());
        swapChainManager_ = std::make_unique<VulkanSwapChain>(vulkanContext_.get(), windowManager_.get());
        
        // Create swap chain
        swapChainManager_->createSwapChain();
        swapChainManager_->createImageViews();
        
        // Create image resources
        imageManager_->createDepthResources();
        imageManager_->createColorResources();
        
        // Initialize framebuffer manager
        framebufferManager_ = std::make_unique<VulkanFramebuffer>(vulkanContext_.get(), swapChainManager_.get(), imageManager_.get());
        framebufferManager_->createRenderPass();
        framebufferManager_->createFramebuffers();
        
        // Initialize renderer
        renderer_ = std::make_unique<VulkanRenderer>(vulkanContext_.get(), swapChainManager_.get(), imageManager_.get());
        renderer_->createDescriptorSetLayout();
        renderer_->createGraphicsPipeline();
        renderer_->createDescriptorPool();
        renderer_->createDescriptorSets();
        
        // Initialize voxel renderer
        voxelRenderer_ = std::make_unique<VoxelRenderer>(vulkanContext_.get(), memoryManager_.get());
        voxelRenderer_->createVoxelRenderingResources();
        
        // Initialize compute manager
        computeManager_ = std::make_unique<VulkanCompute>(vulkanContext_.get());
        computeManager_->createComputePipeline();
        
        // Create buffers using buffer manager
        bufferManager_->createVertexBuffer(vertices_);
        bufferManager_->createIndexBuffer(indices_);
        bufferManager_->createUniformBuffers();
        
        // Initialize ImGui
        imguiManager_ = std::make_unique<VulkanImGui>(vulkanContext_.get(), windowManager_.get(), framebufferManager_->getRenderPass());
        imguiManager_->initialize();

        // Initialize camera with window
        camera_ = Camera(windowManager_->getWindow());
        camera_.setPosition(glm::vec3(0.0f, 0.0f, 5.0f));
        camera_.setTarget(glm::vec3(0.0f, 0.0f, 0.0f));
        camera_.setUp(glm::vec3(0.0f, 1.0f, 0.0f));
        
        // Set Grid3D reference in Camera for collision detection
        camera_.setGrid(&grid_);

        // Initialize save manager
        saveManager_ = std::make_unique<SaveManager>();

    } catch (const std::exception& e) {
        cleanup();
        throw std::runtime_error(std::string("Failed to initialize VulkanEngine: ") + e.what());
    }
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
    try {
        // Update simulation if running
        if (currentState_ == App::State::Running && !isPaused_) {
            compute_->update();
        }

        // Begin frame
        uint32_t imageIndex;
        if (!renderer_->beginFrame(imageIndex)) {
            return; // Swap chain needs recreation
        }

        // Update camera and uniform data
        camera_.update(0.016f); // Assume 60fps for now
        
        // Begin render pass
        renderer_->beginRenderPass(imageIndex);
        
        // Render voxels
        voxelRenderer_->render(camera_.getViewMatrix(), camera_.getProjectionMatrix());
        
        // Render ImGui
        imguiManager_->render();
        
        // End render pass and present
        renderer_->endRenderPass();
        renderer_->endFrame(imageIndex);
        
    } catch (const std::exception& e) {
        std::cerr << "Error in drawFrame: " << e.what() << std::endl;
        throw;
    }
}

void VulkanEngine::waitForComputeCompletion() {
    // Delegate to compute manager
    if (computeManager_) {
        computeManager_->waitForCompletion();
    }
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

    if (vkQueueSubmit(computeQueue, 1, &submitInfo, computeFences_[currentComputeFrame_]) != VK_SUCCESS) {
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
    try {
        auto lastFrameTime = std::chrono::high_resolution_clock::now();
        float deltaTime = 0.0f;

        while (!windowManager_->shouldClose() && currentState_ != App::State::Exiting) {
            // Calculate delta time
            auto currentTime = std::chrono::high_resolution_clock::now();
            deltaTime = std::chrono::duration<float>(currentTime - lastFrameTime).count();
            lastFrameTime = currentTime;

            // Handle window events
            windowManager_->pollEvents();

            // Begin ImGui frame
            beginImGuiFrame();

            // Update state based on current state
            switch (currentState_) {
                case App::State::Menu:
                    updateMenu(deltaTime);
                    break;
                case App::State::Running:
                    updateRunning(deltaTime);
                    break;
                case App::State::SavePicker:
                    updateSavePicker(deltaTime);
                    break;
                case App::State::Loading:
                    updateLoading(deltaTime);
                    break;
                case App::State::Exiting:
                    break;
            }

            // Draw frame
            drawFrame();

            // End ImGui frame
            endImGuiFrame();

            // Cap framerate to ~60fps when idle
            auto frameEndTime = std::chrono::high_resolution_clock::now();
            auto frameDuration = std::chrono::duration_cast<std::chrono::milliseconds>(
                frameEndTime - currentTime).count();
            
            if (frameDuration < 16) { // 16ms ~= 60fps
                std::this_thread::sleep_for(std::chrono::milliseconds(16 - frameDuration));
            }
        }

        // Wait for device idle before cleanup
        vkDeviceWaitIdle(device_);

    } catch (const std::exception& e) {
        std::cerr << "Error in main loop: " << e.what() << std::endl;
        cleanup();
        throw;
    }
}

void VulkanEngine::updateMenu(float deltaTime) {
    // Update menu state
    if (ImGui::Begin("Main Menu")) {
        if (ImGui::Button("Start Simulation")) {
            currentState_ = App::State::Running;
        }
        if (ImGui::Button("Load Pattern")) {
            currentState_ = App::State::SavePicker;
        }
        if (ImGui::Button("Exit")) {
            currentState_ = App::State::Exiting;
        }
    }
    ImGui::End();
}

void VulkanEngine::updateRunning(float deltaTime) {
    // Update simulation
    if (!isPaused_) {
        updateSimulation(deltaTime);
    }

    // Update camera
    camera_.update(deltaTime);

    // Update UI
    if (ImGui::Begin("Controls")) {
        ImGui::Checkbox("Pause", &isPaused_);
        if (ImGui::Button("Save Pattern")) {
            currentState_ = App::State::SavePicker;
        }
        if (ImGui::Button("Exit to Menu")) {
            currentState_ = App::State::Menu;
        }
    }
    ImGui::End();
}

void VulkanEngine::updateSavePicker(float deltaTime) {
    // Update save picker UI
    if (ImGui::Begin("Save/Load Pattern")) {
        if (ImGui::Button("Back")) {
            currentState_ = App::State::Menu;
        }
        // Add save/load pattern UI here
    }
    ImGui::End();
}

void VulkanEngine::updateLoading(float deltaTime) {
    // Update loading screen
    loadingElapsed_ += deltaTime;
    
    // Check if loading is complete
    if (loadingFuture_.valid() && loadingFuture_.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
        bool success = loadingFuture_.get();
        if (success) {
            currentState_ = App::State::Running;
        } else {
            currentState_ = App::State::Menu;
        }
        isLoading_ = false;
    }
}

void VulkanEngine::updateSimulation(float deltaTime) {
    // Update simulation state
    if (isAutoSaveDue()) {
        performAutoSave();
    }

    // Update compute shader parameters
    updateComputePushConstants();

    // Submit compute work
    submitComputeWork();

    // Wait for compute completion
    waitForComputeCompletion();
}

void VulkanEngine::updateSimulation() {
    // Delegate to compute manager
    if (computeManager_) {
        computeManager_->updateSimulation();
    }
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

    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(swapChainExtent_.width);
    viewport.height = static_cast<float>(swapChainExtent_.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = swapChainExtent_;
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout_, 0, 1, &descriptorSets_[currentFrame_], 0, nullptr);

    // Bind vertex and index buffers
    auto vertexBuffer = bufferManager_->getVertexBuffer();
    auto indexBuffer = bufferManager_->getIndexBuffer();
    VkBuffer vertexBuffers[] = {vertexBuffer};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
    vkCmdBindIndexBuffer(commandBuffer, indexBuffer, 0, VK_INDEX_TYPE_UINT32);

    // Bind instance buffer using VoxelRenderer
    if (voxelRenderer_) {
        auto instanceBuffer = voxelRenderer_->getInstanceBuffer();
        VkBuffer instanceBuffers[] = {instanceBuffer};
        vkCmdBindVertexBuffers(commandBuffer, 1, 1, instanceBuffers, offsets);

        // Draw instanced using VoxelRenderer instance count
        uint32_t instanceCount = voxelRenderer_->getInstanceCount();
        vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(indices_.size()), instanceCount, 0, 0, 0);
    }

    // Render ImGui using manager
    if (imguiManager_) {
        imguiManager_->render(commandBuffer);
    }

    vkCmdEndRenderPass(commandBuffer);

    if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
        throw std::runtime_error("Failed to record command buffer!");
    }
}

// Create a compute pipeline
VkPipeline VulkanEngine::createComputePipeline(const std::string& shaderPath) {
    VkDevice device = vulkanContext_->getDevice();

    // Load compute shader
    auto computeShaderCode = readFile(shaderPath);
    ShaderModule computeShaderModule(createShaderModule(computeShaderCode));

    // Create compute shader stage info
    VkPipelineShaderStageCreateInfo computeShaderStageInfo{};
    computeShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    computeShaderStageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    computeShaderStageInfo.module = computeShaderModule;
    computeShaderStageInfo.pName = "main";

    // Create pipeline layout
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;

    // Set up push constant range
    VkPushConstantRange pushConstantRange{};
    pushConstantRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    pushConstantRange.offset = 0;
    pushConstantRange.size = sizeof(GameOfLifePushConstants);

    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;

    // Set up descriptor set layout
    std::array<VkDescriptorSetLayoutBinding, 2> bindings{};
    
    // State buffer binding
    bindings[0].binding = 0;
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bindings[0].descriptorCount = 1;
    bindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    
    // Next state buffer binding
    bindings[1].binding = 1;
    bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bindings[1].descriptorCount = 1;
    bindings[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    layoutInfo.pBindings = bindings.data();

    VkDescriptorSetLayout descriptorSetLayout;
    if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &descriptorSetLayout) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create compute descriptor set layout!");
    }

    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &descriptorSetLayout;

    VkPipelineLayout pipelineLayout;
    if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
        vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);
        throw std::runtime_error("Failed to create compute pipeline layout!");
    }

    // Create compute pipeline
    VkComputePipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineInfo.stage = computeShaderStageInfo;
    pipelineInfo.layout = pipelineLayout;

    VkPipeline computePipeline;
    if (vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &computePipeline) != VK_SUCCESS) {
        vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
        vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);
        throw std::runtime_error("Failed to create compute pipeline!");
    }

    // Store pipeline info for cleanup
    ComputePipelineInfo pipelineInfo;
    pipelineInfo.pipeline = computePipeline;
    pipelineInfo.layout = pipelineLayout;
    pipelineInfo.descriptorSetLayout = descriptorSetLayout;
    computePipelines_.push_back(pipelineInfo);

    return computePipeline;
}

void VulkanEngine::createComputeDescriptorSetLayout() {
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

    VK_CHECK(vkCreateDescriptorSetLayout(vulkanContext_->getDevice(), &layoutInfo, nullptr, &computePipeline_.descriptorSetLayout));
}

void VulkanEngine::createComputeDescriptorPool() {
    std::array<VkDescriptorPoolSize, 1> poolSizes{};
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    poolSizes[0].descriptorCount = 2; // Two buffers per set

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();
    poolInfo.maxSets = 1;

    VK_CHECK(vkCreateDescriptorPool(vulkanContext_->getDevice(), &poolInfo, nullptr, &computePipeline_.descriptorPool));
}

void VulkanEngine::createComputeDescriptorSets() {
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = computePipeline_.descriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &computePipeline_.descriptorSetLayout;

    computePipeline_.descriptorSets.resize(1);
    VK_CHECK(vkAllocateDescriptorSets(vulkanContext_->getDevice(), &allocInfo, computePipeline_.descriptorSets.data()));

    // Update descriptor sets
    VkDescriptorBufferInfo stateBufferInfo{};
    stateBufferInfo.buffer = computePipeline_.stateBuffer;
    stateBufferInfo.offset = 0;
    stateBufferInfo.range = VK_WHOLE_SIZE;

    VkDescriptorBufferInfo nextStateBufferInfo{};
    nextStateBufferInfo.buffer = computePipeline_.nextStateBuffer;
    nextStateBufferInfo.offset = 0;
    nextStateBufferInfo.range = VK_WHOLE_SIZE;

    std::array<VkWriteDescriptorSet, 2> descriptorWrites{};

    descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[0].dstSet = computePipeline_.descriptorSets[0];
    descriptorWrites[0].dstBinding = 0;
    descriptorWrites[0].dstArrayElement = 0;
    descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    descriptorWrites[0].descriptorCount = 1;
    descriptorWrites[0].pBufferInfo = &stateBufferInfo;

    descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[1].dstSet = computePipeline_.descriptorSets[0];
    descriptorWrites[1].dstBinding = 1;
    descriptorWrites[1].dstArrayElement = 0;
    descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    descriptorWrites[1].descriptorCount = 1;
    descriptorWrites[1].pBufferInfo = &nextStateBufferInfo;

    vkUpdateDescriptorSets(vulkanContext_->getDevice(), static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
}

void VulkanEngine::createComputeBuffers() {
    VkDeviceSize bufferSize = gridWidth_ * gridHeight_ * gridDepth_ * sizeof(uint32_t);
    auto& memoryManager = vulkanContext_->getMemoryManager();

    // Create current state buffer
    auto currentStateAlloc = memoryManager.createBuffer(
        bufferSize,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VMA_MEMORY_USAGE_GPU_ONLY
    );
    pipelineInfo.currentStateBuffer = currentStateAlloc.buffer;
    pipelineInfo.currentStateBufferAllocation = currentStateAlloc.allocation;

    // Create next state buffer
    auto nextStateAlloc = memoryManager.createBuffer(
        bufferSize,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VMA_MEMORY_USAGE_GPU_ONLY
    );
    pipelineInfo.nextStateBuffer = nextStateAlloc.buffer;
    pipelineInfo.nextStateBufferAllocation = nextStateAlloc.allocation;
}

void VulkanEngine::updateComputePushConstants() {
    // Delegate to compute manager
    if (computeManager_) {
        computeManager_->updatePushConstants();
    }
}

void VulkanEngine::submitComputeWork() {
    // Delegate to compute manager
    if (computeManager_) {
        computeManager_->submitWork();
    }
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
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = graphicsCommandPool_;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    vkAllocateCommandBuffers(vulkanContext_->getDevice(), &allocInfo, &commandBuffer);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(commandBuffer, &beginInfo);

    return commandBuffer;
}

void VulkanEngine::endSingleTimeCommands(VkCommandBuffer commandBuffer) {
    vkEndCommandBuffer(commandBuffer);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    vkQueueSubmit(vulkanContext_->getGraphicsQueue(), 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(vulkanContext_->getGraphicsQueue());

    vkFreeCommandBuffers(vulkanContext_->getDevice(), graphicsCommandPool_, 1, &commandBuffer);
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
    if (device_ == VK_NULL_HANDLE) {
        return; // Already cleaned up
    }

    vkDeviceWaitIdle(device_);

    // Cleanup in reverse order
    if (imguiManager_) {
        imguiManager_->cleanup();
        imguiManager_.reset();
    }
    
    if (computeManager_) {
        computeManager_->cleanup();
        computeManager_.reset();
    }
    
    if (voxelRenderer_) {
        voxelRenderer_->cleanup();
        voxelRenderer_.reset();
    }
    
    if (renderer_) {
        renderer_->cleanup();
        renderer_.reset();
    }
    
    if (framebufferManager_) {
        framebufferManager_->cleanup();
        framebufferManager_.reset();
    }
    
    if (bufferManager_) {
        bufferManager_->cleanup();
        bufferManager_.reset();
    }
    
    if (imageManager_) {
        imageManager_->cleanup();
        imageManager_.reset();
    }
    
    if (swapChainManager_) {
        swapChainManager_->cleanup();
        swapChainManager_.reset();
    }
    
    saveManager_.reset();
    memoryManager_.reset();
    vulkanContext_.reset();
    windowManager_.reset();

    // Cleanup surface
    if (surface_ != VK_NULL_HANDLE) {
        vkDestroySurfaceKHR(vulkanContext_->getInstance(), surface_, nullptr);
        surface_ = VK_NULL_HANDLE;
    }

    // Reset device handle to indicate cleanup is complete
    device_ = VK_NULL_HANDLE;
}

// Placeholder for framebufferResizeCallback - will be handled by WindowManager
void VulkanEngine::framebufferResizeCallback(GLFWwindow* window, int width, int height) {
    auto app = reinterpret_cast<VulkanEngine*>(glfwGetWindowUserPointer(window));
    app->framebufferResized_ = true;
}

VkShaderModule VulkanEngine::createShaderModule(const std::vector<char>& code) {
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = code.size();
    createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

    VkShaderModule shaderModule;
    if (vkCreateShaderModule(vulkanContext_->getDevice(), &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create shader module!");
    }

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
    bufferManager_->createVertexBuffer(vertices_);
}

void VulkanEngine::createIndexBuffer() {
    bufferManager_->createIndexBuffer(indices_);
}

void VulkanEngine::createUniformBuffers() {
    bufferManager_->createUniformBuffers();
}

void VulkanEngine::createDescriptorPool() {
    std::array<VkDescriptorPoolSize, 2> poolSizes{};
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[0].descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
    poolSizes[1].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    poolSizes[1].descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT * 2); // For compute buffers

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

        VkWriteDescriptorSet descriptorWrite{};
        descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrite.dstSet = descriptorSets_[i];
        descriptorWrite.dstBinding = 0;
        descriptorWrite.dstArrayElement = 0;
        descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptorWrite.descriptorCount = 1;
        descriptorWrite.pBufferInfo = &bufferInfo;

        vkUpdateDescriptorSets(vulkanContext_->getDevice(), 1, &descriptorWrite, 0, nullptr);
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
    ubo.model = glm::rotate(glm::mat4(1.0f), time * glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    ubo.view = camera_.getViewMatrix();
    ubo.proj = glm::perspective(glm::radians(45.0f), swapChainExtent_.width / (float)swapChainExtent_.height, 0.1f, 1000.0f);
    ubo.proj[1][1] *= -1; // Flip Y for Vulkan
    ubo.cameraPos = camera_.getPosition();
    ubo.time = time;
    ubo.renderMode = renderMode_;
    ubo.minLODDistance = minLODDistance_;
    ubo.maxLODDistance = maxLODDistance_;

    void* data;
    VkResult result = vmaMapMemory(memoryManager_->getAllocator(), uniformBuffersAllocations_[currentImage], &data);
    if (result == VK_SUCCESS) {
        memcpy(data, &ubo, sizeof(ubo));
        vmaUnmapMemory(memoryManager_->getAllocator(), uniformBuffersAllocations_[currentImage]);
    }
}

void VulkanEngine::drawMenu() {
    ImGui::NewFrameForVulkan();
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
    saveFiles_ = saveManager_->listSaveFiles();
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
    std::string lastSave = saveManager_->getLastSaveFileName();
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
            PatternMetadata metadata;
            bool ok = saveManager_->loadState(filename);
            if (!ok) {
                std::string error = saveManager_->getLastError();
                updateLoadingState("Failed to load save file: " + error, 0.0f);
                return false;
            }
            // Get the loaded data from save manager
            temp = saveManager_->getCurrentState();
            metadata = saveManager_->getCurrentMetadata();

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
    PatternMetadata metadata;
    metadata.name = filename;
    metadata.description = "Auto-generated pattern";
    metadata.author = "VulkanEngine";
    metadata.creationDate = std::chrono::system_clock::now();
    metadata.tags = {};
    metadata.version = "1.0";
    saveManager_->saveState(filename, loadedVoxelData_);
}

void VulkanEngine::setAppState(App::State newState) {
    currentState_ = newState;
}

void VulkanEngine::drawLoading() {
    ImGui::NewFrameForVulkan();
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
    // Delegate to save manager
    return saveManager_ ? saveManager_->isAutoSaveDue() : false;
}

void VulkanEngine::performAutoSave() {
    // Delegate to save manager
    if (saveManager_) {
        saveManager_->performAutoSave();
    }
}

void VulkanEngine::cleanupOldAutoSaves() {
    try {
        auto saves = saveManager_->listSaveFiles();
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
            if (!saveManager_->deleteSave(autoSaves.front())) {
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
        PatternMetadata metadata;
        metadata.name = filename;
        metadata.description = "Manual save pattern";
        metadata.author = "VulkanEngine";
        metadata.creationDate = std::chrono::system_clock::now();
        metadata.tags = {};
        metadata.version = "1.0";
        if (saveManager_->saveState(filename, loadedVoxelData_)) {
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
    auto& memoryManager = vulkanContext_->getMemoryManager();

    // Create staging buffer for vertices
    auto vertexStagingBuffer = memoryManager.createStagingBuffer(vertexBufferSize);
    void* vertexData = memoryManager.mapStagingBuffer(vertexStagingBuffer);
    memcpy(vertexData, voxelVertices_.data(), vertexBufferSize);
    memoryManager.unmapStagingBuffer(vertexStagingBuffer);

    // Create vertex buffer
    auto vertexBufferAlloc = memoryManager.createBuffer(
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
    auto indexStagingBuffer = memoryManager.createStagingBuffer(indexBufferSize);
    void* indexData = memoryManager.mapStagingBuffer(indexStagingBuffer);
    memcpy(indexData, voxelIndices_.data(), indexBufferSize);
    memoryManager.unmapStagingBuffer(indexStagingBuffer);

    // Create index buffer
    auto indexBufferAlloc = memoryManager.createBuffer(
        indexBufferSize,
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        VMA_MEMORY_USAGE_GPU_ONLY
    );
    voxelIndexBuffer_ = indexBufferAlloc.buffer;
    voxelIndexBufferAllocation_ = indexBufferAlloc.allocation;

    // Copy index data
    copyBuffer(indexStagingBuffer.buffer, voxelIndexBuffer_, indexBufferSize);

    // Clean up staging buffers
    memoryManager.destroyStagingBuffer(vertexStagingBuffer);
    memoryManager.destroyStagingBuffer(indexStagingBuffer);
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
    if (vulkanContext_) {
        auto& memoryManager = vulkanContext_->getMemoryManager();
        
        if (voxelVertexBuffer_ != VK_NULL_HANDLE) {
            memoryManager.destroyBuffer({voxelVertexBuffer_, voxelVertexBufferAllocation_, {}, 0, nullptr});
            voxelVertexBuffer_ = VK_NULL_HANDLE;
        }
        if (voxelIndexBuffer_ != VK_NULL_HANDLE) {
            memoryManager.destroyBuffer({voxelIndexBuffer_, voxelIndexBufferAllocation_, {}, 0, nullptr});
            voxelIndexBuffer_ = VK_NULL_HANDLE;
        }
    }
}

void VulkanEngine::createGraphicsPipeline() {
    // Load shader code
    auto vertShaderCode = readFile("shaders/voxel.vert");
    auto fragShaderCode = readFile("shaders/voxel.frag");

    // Create shader modules
    ShaderModule vertShaderModule(createShaderModule(vertShaderCode));
    ShaderModule fragShaderModule(createShaderModule(fragShaderCode));

    // Vertex shader stage
    VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
    vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertShaderStageInfo.module = vertShaderModule;
    vertShaderStageInfo.pName = "main";

    // Fragment shader stage
    VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
    fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragShaderStageInfo.module = fragShaderModule;
    fragShaderStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo, fragShaderStageInfo};

    // Vertex input state
    auto bindingDescription = Vertex::getBindingDescription();
    auto attributeDescriptions = Vertex::getAttributeDescriptions();

    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount = 1;
    vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
    vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
    vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

    // Input assembly state
    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    // Viewport state
    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(swapChainExtent_.width);
    viewport.height = static_cast<float>(swapChainExtent_.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = swapChainExtent_;

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.pViewports = &viewport;
    viewportState.scissorCount = 1;
    viewportState.pScissors = &scissor;

    // Rasterization state
    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizer.depthBiasEnable = VK_FALSE;

    // Multisampling state
    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_TRUE;
    multisampling.rasterizationSamples = msaaSamples_;
    multisampling.minSampleShading = 0.2f;

    // Depth stencil state
    VkPipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable = VK_TRUE;
    depthStencil.depthWriteEnable = VK_TRUE;
    depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
    depthStencil.depthBoundsTestEnable = VK_FALSE;
    depthStencil.stencilTestEnable = VK_FALSE;

    // Color blend state
    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_TRUE;
    colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
    colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;

    // Dynamic state
    std::vector<VkDynamicState> dynamicStates = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_LINE_WIDTH
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

    // Push constant ranges
    VkPushConstantRange pushConstantRange{};
    pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    pushConstantRange.offset = 0;
    pushConstantRange.size = sizeof(UniformBufferObject);

    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;

    if (vkCreatePipelineLayout(vulkanContext_->getDevice(), &pipelineLayoutInfo, nullptr, &pipelineLayout_) != VK_SUCCESS) {
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

    if (vkCreateGraphicsPipelines(vulkanContext_->getDevice(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &graphicsPipeline_) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create graphics pipeline!");
    }
}

void VulkanEngine::initImGui() {
    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();

    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForVulkan(windowManager_->getWindow(), true);
    ImGui::VulkanInitInfo init_info = {};
    init_info.instance = vulkanContext_->getInstance();
    init_info.physicalDevice = vulkanContext_->getPhysicalDevice();
    init_info.device = vulkanContext_->getDevice();
    init_info.queueFamily = findQueueFamilies(vulkanContext_->getPhysicalDevice(), vulkanContext_->getSurface()).graphicsFamily.value();
    init_info.queue = vulkanContext_->getGraphicsQueue();
    init_info.pipelineCache = VK_NULL_HANDLE;
    init_info.descriptorPool = descriptorPool_;
    init_info.minImageCount = 2;
    init_info.imageCount = static_cast<uint32_t>(swapChainImages_.size());
    init_info.msaaSamples = VK_SAMPLE_COUNT_1_BIT;
    init_info.allocator = nullptr;

    ImGui::InitForVulkan(init_info);

    // Upload Fonts
    VkCommandBuffer commandBuffer = beginSingleTimeCommands();
    ImGui::CreateFontsTexture(commandBuffer);
    endSingleTimeCommands(commandBuffer);
    ImGui::DestroyFontUploadObjects();

    imguiInitialized_ = true;
}

void VulkanEngine::cleanupImGui() {
    if (imguiInitialized_) {
        ImGui::ShutdownForVulkan();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        imguiInitialized_ = false;
    }
}

void VulkanEngine::beginImGuiFrame() {
    if (imguiManager_) {
        imguiManager_->beginFrame();
    }
}

void VulkanEngine::endImGuiFrame() {
    if (imguiManager_) {
        imguiManager_->endFrame();
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

VkSurfaceKHR VulkanEngine::createWindowSurface() const {
    return windowManager_->createSurface(vulkanContext_->getInstance());
}

void VulkanEngine::createComputeCommandPool() {
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = vulkanContext_->getComputeQueueFamily();

    if (vkCreateCommandPool(vulkanContext_->getDevice(), &poolInfo, nullptr, &computeCommandPool_) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create compute command pool!");
    }
}

void VulkanEngine::createComputeCommandBuffers() {
    computeCommandBuffers_.resize(MAX_FRAMES_IN_FLIGHT);

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = computeCommandPool_;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = static_cast<uint32_t>(computeCommandBuffers_.size());

    if (vkAllocateCommandBuffers(vulkanContext_->getDevice(), &allocInfo, computeCommandBuffers_.data()) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate compute command buffers!");
    }
}

void VulkanEngine::createComputeBuffers(ComputePipelineInfo& pipelineInfo, uint32_t width, uint32_t height, uint32_t depth) {
    VkDeviceSize bufferSize = width * height * depth * sizeof(uint32_t);
    auto& memoryManager = vulkanContext_->getMemoryManager();

    // Create current state buffer
    auto currentStateAlloc = memoryManager.createBuffer(
        bufferSize,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VMA_MEMORY_USAGE_GPU_ONLY
    );
    pipelineInfo.currentStateBuffer = currentStateAlloc.buffer;
    pipelineInfo.currentStateBufferAllocation = currentStateAlloc.allocation;

    // Create next state buffer
    auto nextStateAlloc = memoryManager.createBuffer(
        bufferSize,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VMA_MEMORY_USAGE_GPU_ONLY
    );
    pipelineInfo.nextStateBuffer = nextStateAlloc.buffer;
    pipelineInfo.nextStateBufferAllocation = nextStateAlloc.allocation;
}

void VulkanEngine::createComputeDescriptorPool() {
    std::array<VkDescriptorPoolSize, 1> poolSizes{};
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    poolSizes[0].descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT * 2); // Two buffers per frame

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();
    poolInfo.maxSets = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);

    if (vkCreateDescriptorPool(vulkanContext_->getDevice(), &poolInfo, nullptr, &computeDescriptorPool_) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create compute descriptor pool!");
    }
}

void VulkanEngine::createComputeDescriptorSets(ComputePipelineInfo& pipelineInfo) {
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = computeDescriptorPool_;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &pipelineInfo.descriptorSetLayout;

    if (vkAllocateDescriptorSets(vulkanContext_->getDevice(), &allocInfo, &pipelineInfo.descriptorSet) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate compute descriptor set!");
    }

    // Update descriptor set with buffer information
    std::array<VkDescriptorBufferInfo, 2> bufferInfos{};
    bufferInfos[0].buffer = pipelineInfo.currentStateBuffer;
    bufferInfos[0].offset = 0;
    bufferInfos[0].range = VK_WHOLE_SIZE;

    bufferInfos[1].buffer = pipelineInfo.nextStateBuffer;
    bufferInfos[1].offset = 0;
    bufferInfos[1].range = VK_WHOLE_SIZE;

    std::array<VkWriteDescriptorSet, 2> descriptorWrites{};
    
    descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[0].dstSet = pipelineInfo.descriptorSet;
    descriptorWrites[0].dstBinding = 0;
    descriptorWrites[0].dstArrayElement = 0;
    descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    descriptorWrites[0].descriptorCount = 1;
    descriptorWrites[0].pBufferInfo = &bufferInfos[0];

    descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[1].dstSet = pipelineInfo.descriptorSet;
    descriptorWrites[1].dstBinding = 1;
    descriptorWrites[1].dstArrayElement = 0;
    descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    descriptorWrites[1].descriptorCount = 1;
    descriptorWrites[1].pBufferInfo = &bufferInfos[1];

    vkUpdateDescriptorSets(vulkanContext_->getDevice(), static_cast<uint32_t>(descriptorWrites.size()),
                          descriptorWrites.data(), 0, nullptr);
}

void VulkanEngine::updateComputePushConstants(const GameOfLifePushConstants& constants) {
    for (const auto& pipeline : computePipelines_) {
        vkCmdPushConstants(computeCommandBuffers_[currentComputeFrame_], pipeline.layout,
                          VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(GameOfLifePushConstants), &constants);
    }
}

void VulkanEngine::submitComputeCommand(VkCommandBuffer commandBuffer) {
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    if (vkQueueSubmit(vulkanContext_->getComputeQueue(), 1, &submitInfo, computeFences_[currentComputeFrame_]) != VK_SUCCESS) {
        throw std::runtime_error("Failed to submit compute command buffer!");
    }
}

void VulkanEngine::waitForComputeCompletion() {
    // Delegate to compute manager
    if (computeManager_) {
        computeManager_->waitForCompletion();
    }
}

// Update voxel instances
void VulkanEngine::updateVoxelInstances() {
    voxelInstances_.clear();
    
    // Read current state from compute buffer
    std::vector<uint32_t> currentState(gridWidth_ * gridHeight_ * gridDepth_);
    void* data;
    VkResult result = vmaMapMemory(memoryManager_->getAllocator(), computePipelines_[0].currentStateBufferAllocation, &data);
    if (result == VK_SUCCESS) {
        memcpy(currentState.data(), data, currentState.size() * sizeof(uint32_t));
        vmaUnmapMemory(memoryManager_->getAllocator(), computePipelines_[0].currentStateBufferAllocation);
    }

    // Create instances for active cells
    for (uint32_t z = 0; z < gridDepth_; z++) {
        for (uint32_t y = 0; y < gridHeight_; y++) {
            for (uint32_t x = 0; x < gridWidth_; x++) {
                uint32_t index = z * gridWidth_ * gridHeight_ + y * gridWidth_ + x;
                if (currentState[index] > 0) {
                    VoxelInstance instance;
                    instance.position = glm::vec3(x, y, z);
                    instance.color = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f); // Default color
                    instance.age = currentState[index];
                    instance.lod = 0; // Will be calculated in shader
                    voxelInstances_.push_back(instance);
                }
            }
        }
    }

    // Update instance buffer
    if (!voxelInstances_.empty()) {
        void* instanceData;
        VkResult mapResult = vmaMapMemory(memoryManager_->getAllocator(), voxelInstanceBufferAllocation_, &instanceData);
        if (mapResult == VK_SUCCESS) {
            memcpy(instanceData, voxelInstances_.data(), voxelInstances_.size() * sizeof(VoxelInstance));
            vmaUnmapMemory(memoryManager_->getAllocator(), voxelInstanceBufferAllocation_);
        }
    }
}

// Create compute pipelines
void VulkanEngine::createComputePipelines() {
    // Create Game of Life compute pipeline
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &computeDescriptorSetLayout_;
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    VkPushConstantRange pushConstantRange{};
    pushConstantRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    pushConstantRange.offset = 0;
    pushConstantRange.size = sizeof(GameOfLifePushConstants);
    pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;

    ComputePipelineInfo pipelineInfo;
    if (vkCreatePipelineLayout(vulkanContext_->getDevice(), &pipelineLayoutInfo, nullptr, &pipelineInfo.layout) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create compute pipeline layout!");
    }

    // Create compute shader module
    auto computeShaderCode = readFile("shaders/game_of_life_3d.comp");
    VkShaderModule computeShaderModule = createShaderModule(computeShaderCode);

    VkPipelineShaderStageCreateInfo shaderStageInfo{};
    shaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    shaderStageInfo.module = computeShaderModule;
    shaderStageInfo.pName = "main";

    VkComputePipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineInfo.stage = shaderStageInfo;
    pipelineInfo.layout = pipelineInfo.layout;

    if (vkCreateComputePipelines(vulkanContext_->getDevice(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipelineInfo.pipeline) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create compute pipeline!");
    }

    vkDestroyShaderModule(vulkanContext_->getDevice(), computeShaderModule, nullptr);

    // Set pipeline constants
    pipelineInfo.pushConstants.width = gridWidth_;
    pipelineInfo.pushConstants.height = gridHeight_;
    pipelineInfo.pushConstants.depth = gridDepth_;
    pipelineInfo.pushConstants.ruleSet = ruleSet_;
    pipelineInfo.pushConstants.surviveMin = surviveMin_;
    pipelineInfo.pushConstants.surviveMax = surviveMax_;
    pipelineInfo.pushConstants.birthCount = birthCount_;

    computePipelines_.push_back(pipelineInfo);
}

// Create compute descriptor set layout
void VulkanEngine::createComputeDescriptorSetLayout() {
    std::array<VkDescriptorSetLayoutBinding, 2> bindings{};
    bindings[0].binding = 0;
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bindings[0].descriptorCount = 1;
    bindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    bindings[1].binding = 1;
    bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bindings[1].descriptorCount = 1;
    bindings[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    layoutInfo.pBindings = bindings.data();

    if (vkCreateDescriptorSetLayout(vulkanContext_->getDevice(), &layoutInfo, nullptr, &computeDescriptorSetLayout_) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create compute descriptor set layout!");
    }
}

// Create compute command buffers
void VulkanEngine::createComputeCommandBuffers() {
    computeCommandBuffers_.resize(MAX_FRAMES_IN_FLIGHT);

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = computeCommandPool_;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = static_cast<uint32_t>(computeCommandBuffers_.size());

    if (vkAllocateCommandBuffers(vulkanContext_->getDevice(), &allocInfo, computeCommandBuffers_.data()) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate compute command buffers!");
    }
}

// Create compute command pool
void VulkanEngine::createComputeCommandPool() {
    QueueFamilyIndices queueFamilyIndices = findQueueFamilies(vulkanContext_->getPhysicalDevice(), vulkanContext_->getSurface());

    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = queueFamilyIndices.computeFamily.value();

    if (vkCreateCommandPool(vulkanContext_->getDevice(), &poolInfo, nullptr, &computeCommandPool_) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create compute command pool!");
    }
}

// Create synchronization objects
void VulkanEngine::createSyncObjects() {
    imageAvailableSemaphores_.resize(MAX_FRAMES_IN_FLIGHT);
    renderFinishedSemaphores_.resize(MAX_FRAMES_IN_FLIGHT);
    inFlightFences_.resize(MAX_FRAMES_IN_FLIGHT);
    computeFences_.resize(MAX_FRAMES_IN_FLIGHT);

    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        if (vkCreateSemaphore(vulkanContext_->getDevice(), &semaphoreInfo, nullptr, &imageAvailableSemaphores_[i]) != VK_SUCCESS ||
            vkCreateSemaphore(vulkanContext_->getDevice(), &semaphoreInfo, nullptr, &renderFinishedSemaphores_[i]) != VK_SUCCESS ||
            vkCreateFence(vulkanContext_->getDevice(), &fenceInfo, nullptr, &inFlightFences_[i]) != VK_SUCCESS ||
            vkCreateFence(vulkanContext_->getDevice(), &fenceInfo, nullptr, &computeFences_[i]) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create synchronization objects for a frame!");
        }
    }
}

QueueFamilyIndices VulkanEngine::findQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR surface) {
    QueueFamilyIndices indices;

    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

    int i = 0;
    for (const auto& queueFamily : queueFamilies) {
        if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            indices.graphicsFamily = i;
        }

        if (queueFamily.queueFlags & VK_QUEUE_COMPUTE_BIT) {
            indices.computeFamily = i;
        }

        VkBool32 presentSupport = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);

        if (presentSupport) {
            indices.presentFamily = i;
        }

        if (indices.isComplete()) {
            break;
        }

        i++;
    }

    return indices;
}

} // namespace VulkanHIP
#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include <string>
#include <stdexcept>
#include <optional>
#include <array>
#include <memory>
#include <set>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vk_mem_alloc.h>
#include <future>
#include <functional>
#include <unordered_map>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <atomic>
#include <mutex>

// ImGui includes
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_vulkan.h"

// Project includes
#include "Vertex.h"
#include "VulkanContext.h"
#include "WindowManager.h"
#include "VulkanMemoryManager.h"
#include "AppState.h"
#include "SaveManager.h"
#include "Camera.h"
#include "Grid3D.h"
#include "vulkan/resources/VulkanBufferManager.h"
#include "vulkan/rendering/VoxelRenderer.h"
#include "VulkanError.h"
#include "VoxelData.h"

namespace VulkanHIP {

// Forward declarations
class VulkanMemoryManager;
class VulkanContext;
class WindowManager;
class DeviceManager;
class VulkanImageManager;
class VulkanSwapChain;
class VulkanRenderer;
class VoxelRenderer;
class VulkanFramebuffer;
class VulkanCompute;
class VulkanImGui;

/**
 * @brief RAII wrapper for Vulkan resources
 */
template<typename T, typename Deleter>
class VulkanResource {
public:
    VulkanResource() : resource_(VK_NULL_HANDLE) {}
    explicit VulkanResource(T resource) : resource_(resource) {}
    ~VulkanResource() { if (resource_ != VK_NULL_HANDLE) Deleter()(resource_); }

    // Prevent copying
    VulkanResource(const VulkanResource&) = delete;
    VulkanResource& operator=(const VulkanResource&) = delete;

    // Allow moving
    VulkanResource(VulkanResource&& other) noexcept : resource_(other.resource_) {
        other.resource_ = VK_NULL_HANDLE;
    }
    VulkanResource& operator=(VulkanResource&& other) noexcept {
        if (this != &other) {
            if (resource_ != VK_NULL_HANDLE) {
                Deleter()(resource_);
            }
            resource_ = other.resource_;
            other.resource_ = VK_NULL_HANDLE;
        }
        return *this;
    }

    // Accessors
    T get() const { return resource_; }
    T* address() { return &resource_; }
    operator T() const { return resource_; }

private:
    T resource_;
};

// Type aliases for RAII resources
using Pipeline = VulkanResource<VkPipeline, struct PipelineDeleter>;
using PipelineLayout = VulkanResource<VkPipelineLayout, struct PipelineLayoutDeleter>;
using ShaderModule = VulkanResource<VkShaderModule, struct ShaderModuleDeleter>;

/**
 * @brief Details about swap chain capabilities
 */
struct SwapChainSupportDetails {
    VkSurfaceCapabilitiesKHR capabilities;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> presentModes;
};

/**
 * @brief Game of Life push constants for compute shader
 */
struct GameOfLifePushConstants {
    uint32_t width;
    uint32_t height;
    uint32_t depth;
    uint32_t ruleSet;  // 0: Classic, 1: HighLife, 2: Day & Night, 3: Custom, 4: 5766, 5: 4555
    uint32_t surviveMin;
    uint32_t surviveMax;
    uint32_t birthCount;
};

/**
 * @brief Compute pipeline information
 */
struct ComputePipelineInfo {
    VkPipeline pipeline;
    VkPipelineLayout layout;
    VkDescriptorSetLayout descriptorSetLayout;
    VkDescriptorPool descriptorPool;
    std::vector<VkDescriptorSet> descriptorSets;
    VkBuffer stateBuffer;
    VkBuffer nextStateBuffer;
    VmaAllocation stateBufferAllocation;
    VmaAllocation nextStateBufferAllocation;
    GameOfLifePushConstants pushConstants;
};

/**
 * @brief Helper macros for Vulkan error checking
 */
#define VK_CHECK(x) do { \
    VkResult err = x; \
    if (err) { \
        std::string errorMsg = "Vulkan error at " + std::string(__FILE__) + ":" + std::to_string(__LINE__); \
        switch (err) { \
            case VK_ERROR_VALIDATION_FAILED_EXT: \
                throw VulkanHIP::ValidationError(errorMsg); \
            case VK_ERROR_DEVICE_LOST: \
                throw VulkanHIP::DeviceLostError(errorMsg); \
            case VK_ERROR_OUT_OF_DEVICE_MEMORY: \
            case VK_ERROR_OUT_OF_HOST_MEMORY: \
                throw VulkanHIP::OutOfMemoryError(errorMsg); \
            default: \
                throw VulkanHIP::VulkanError(err, errorMsg); \
        } \
    } \
} while(0)

/**
 * @brief Helper function to check if a Vulkan result is an error
 */
inline bool isVulkanError(VkResult result) {
    return result < 0;
}

/**
 * @brief State machine for the Vulkan engine
 */
class EngineStateMachine {
public:
    using StateHandler = std::function<void()>;
    using StateTransition = std::function<bool()>;

    void addState(App::State state, StateHandler enter, StateHandler update, StateHandler exit) {
        states_[state] = {enter, update, exit};
    }

    void addTransition(App::State from, App::State to, StateTransition condition) {
        transitions_[from][to] = condition;
    }

    void setState(App::State newState) {
        if (currentState_ != newState) {
            if (currentState_ != App::State::None) {
                states_[currentState_].exit();
            }
            currentState_ = newState;
            if (currentState_ != App::State::None) {
                states_[currentState_].enter();
            }
        }
    }

    void update() {
        if (currentState_ != App::State::None) {
            states_[currentState_].update();
            for (const auto& [nextState, condition] : transitions_[currentState_]) {
                if (condition()) {
                    setState(nextState);
                    break;
                }
            }
        }
    }

private:
    struct StateHandlers {
        StateHandler enter;
        StateHandler update;
        StateHandler exit;
    };

    App::State currentState_ = App::State::None;
    std::unordered_map<App::State, StateHandlers> states_;
    std::unordered_map<App::State, std::unordered_map<App::State, StateTransition>> transitions_;
};

/**
 * @brief Main Vulkan engine class
 */
class VulkanEngine {
public:
    static VulkanEngine* getInstance() {
        static VulkanEngine instance;
        return &instance;
    }

    void init();
    void run();
    void cleanup();

    // Getters
    VkInstance getVkInstance() const { return vulkanContext_->getVkInstance(); }
    VkDevice getDevice() const { return vulkanContext_->getDevice(); }
    VkPhysicalDevice getPhysicalDevice() const { return vulkanContext_->getPhysicalDevice(); }
    VkQueue getGraphicsQueue() const { return vulkanContext_->getGraphicsQueue(); }
    VkQueue getComputeQueue() const { return vulkanContext_->getComputeQueue(); }
    VkQueue getPresentQueue() const { return vulkanContext_->getPresentQueue(); }

private:
    VulkanEngine() = default;
    ~VulkanEngine() = default;
    VulkanEngine(const VulkanEngine&) = delete;
    VulkanEngine& operator=(const VulkanEngine&) = delete;

    // Core initialization
    void initializeStateMachine();
    void createCommandPools();
    void createDescriptorSetLayout();
    void createGraphicsPipeline();
    void createShaderStages(const std::string& vertPath, const std::string& fragPath,
                           VkPipelineShaderStageCreateInfo& vertStageInfo,
                           VkPipelineShaderStageCreateInfo& fragStageInfo);

    // Compute pipeline
    void createComputePipeline();
    void createComputeDescriptorSetLayout();
    void createComputeDescriptorPool();
    void createComputeDescriptorSets();
    void createComputeBuffers();
    void updateComputePushConstants();
    void submitComputeWork();
    void createComputeCommandPool();
    void createComputeCommandBuffers();
    void createComputeBuffers(ComputePipelineInfo& pipelineInfo, uint32_t width, uint32_t height, uint32_t depth);
    void updateComputePushConstants(const GameOfLifePushConstants& constants);
    void submitComputeCommand(VkCommandBuffer commandBuffer);
    void waitForComputeCompletion();

    // Swap chain
    void createSwapChain();
    void createImageViews();
    void createRenderPass();
    void createFramebuffers();
    void createDepthResources();
    void createColorResources();
    void createSyncObjects();
    void recreateSwapChain();
    void cleanupSwapChain();

    // Command buffers
    void createCommandBuffers();
    VkCommandBuffer beginSingleTimeCommands();
    void endSingleTimeCommands(VkCommandBuffer commandBuffer);
    void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex);

    // Descriptors
    void createDescriptorPool();
    void createDescriptorSets();
    void updateUniformBuffer(uint32_t currentImage);

    // Voxel rendering
    void createVoxelBuffers();
    void updateVoxelBuffers();
    void createVoxelVertexData(const VoxelData& voxelData);
    void cleanupVoxelBuffers();

    // ImGui
    void initImGui();
    void cleanupImGui();
    void beginImGuiFrame();
    void endImGuiFrame();
    void createImGuiDescriptorPool();
    void cleanupImGuiDescriptorPool();

    // Utility functions
    VkShaderModule createShaderModule(const std::vector<char>& code);
    void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);
    VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats) const;
    VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes) const;
    VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities) const;
    SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device) const;
    VkFormat findSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features);
    VkFormat findDepthFormat();
    bool hasStencilComponent(VkFormat format);
    VkSampleCountFlagBits getMaxUsableSampleCount();
    QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR surface);
    void applyEnabledDeviceFeatures(VkPhysicalDeviceFeatures& features);

    // Member variables
    VulkanHIP::VulkanContext* vulkanContext_;
    VulkanHIP::WindowManager* windowManager_;
    std::unique_ptr<VulkanHIP::VulkanMemoryManager> memoryManager_;
    std::unique_ptr<SaveManager> saveManager_;
    std::unique_ptr<::Camera> camera_;
    std::unique_ptr<::Grid3D> grid_;
    EngineStateMachine stateMachine_;

    // Compute pipeline
    ComputePipelineInfo computePipeline_;

    // Loading state
    float loadingElapsed_ = 0.0f;
    std::future<bool> loadingFuture_;
    bool isLoading_ = false;
    float loadingProgress_ = 0.0f;
    std::string loadingStatus_;
    bool shouldCancelLoading_ = false;
    std::mutex loadingMutex_;

    // Rendering configuration
    int renderMode_ = 0;
    float minLODDistance_ = 10.0f;
    float maxLODDistance_ = 100.0f;

    // Voxel data
    VoxelData loadedVoxelData_;
    VkBuffer voxelInstanceBuffer_ = VK_NULL_HANDLE;
    VmaAllocation voxelInstanceBufferAllocation_ = VK_NULL_HANDLE;
    std::vector<VoxelInstance> voxelInstances_;

    // Timing
    std::chrono::high_resolution_clock::time_point startTime = std::chrono::high_resolution_clock::now();
};

// Resource deleters
struct PipelineDeleter {
    void operator()(VkPipeline pipeline) const {
        if (pipeline != VK_NULL_HANDLE) {
            vkDestroyPipeline(VulkanEngine::getInstance()->getDevice(), pipeline, nullptr);
        }
    }
};

struct PipelineLayoutDeleter {
    void operator()(VkPipelineLayout layout) const {
        if (layout != VK_NULL_HANDLE) {
            vkDestroyPipelineLayout(VulkanEngine::getInstance()->getDevice(), layout, nullptr);
        }
    }
};

struct ShaderModuleDeleter {
    void operator()(VkShaderModule module) const {
        if (module != VK_NULL_HANDLE) {
            vkDestroyShaderModule(VulkanEngine::getInstance()->getDevice(), module, nullptr);
        }
    }
};

} // namespace VulkanHIP
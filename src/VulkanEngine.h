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

namespace VulkanHIP {

// Forward declarations
class VulkanMemoryManager;
class VulkanContext;
class WindowManager;
class DeviceManager;

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
 * @brief Uniform buffer object for shader transformation matrices
 */
struct UniformBufferObject {
    alignas(16) glm::mat4 model;
    alignas(16) glm::mat4 view;
    alignas(16) glm::mat4 proj;
};

/**
 * @brief Details about swap chain capabilities
 */
struct SwapChainSupportDetails {
    VkSurfaceCapabilitiesKHR capabilities;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> presentModes;
};

/**
 * @brief Helper macros for Vulkan error checking
 */
#define VK_CHECK(x) do { \
    VkResult err = x; \
    if (err) { \
        std::cerr << "Vulkan error: " << err << " at " << __FILE__ << ":" << __LINE__ << std::endl; \
        throw std::runtime_error("Vulkan error: " + std::to_string(err)); \
    } \
} while(0)

/**
 * @brief Helper function to check if a Vulkan result is an error
 */
inline bool isVulkanError(VkResult result) {
    return result < 0;
}

/**
 * @brief Helper function to get a string description of a Vulkan result
 */
inline std::string getVulkanResultString(VkResult result) {
    switch (result) {
        case VK_SUCCESS: return "VK_SUCCESS";
        case VK_NOT_READY: return "VK_NOT_READY";
        case VK_TIMEOUT: return "VK_TIMEOUT";
        case VK_EVENT_SET: return "VK_EVENT_SET";
        case VK_EVENT_RESET: return "VK_EVENT_RESET";
        case VK_INCOMPLETE: return "VK_INCOMPLETE";
        case VK_ERROR_OUT_OF_HOST_MEMORY: return "VK_ERROR_OUT_OF_HOST_MEMORY";
        case VK_ERROR_OUT_OF_DEVICE_MEMORY: return "VK_ERROR_OUT_OF_DEVICE_MEMORY";
        case VK_ERROR_INITIALIZATION_FAILED: return "VK_ERROR_INITIALIZATION_FAILED";
        case VK_ERROR_DEVICE_LOST: return "VK_ERROR_DEVICE_LOST";
        case VK_ERROR_MEMORY_MAP_FAILED: return "VK_ERROR_MEMORY_MAP_FAILED";
        case VK_ERROR_LAYER_NOT_PRESENT: return "VK_ERROR_LAYER_NOT_PRESENT";
        case VK_ERROR_EXTENSION_NOT_PRESENT: return "VK_ERROR_EXTENSION_NOT_PRESENT";
        case VK_ERROR_FEATURE_NOT_PRESENT: return "VK_ERROR_FEATURE_NOT_PRESENT";
        case VK_ERROR_INCOMPATIBLE_DRIVER: return "VK_ERROR_INCOMPATIBLE_DRIVER";
        case VK_ERROR_TOO_MANY_OBJECTS: return "VK_ERROR_TOO_MANY_OBJECTS";
        case VK_ERROR_FORMAT_NOT_SUPPORTED: return "VK_ERROR_FORMAT_NOT_SUPPORTED";
        case VK_ERROR_FRAGMENTED_POOL: return "VK_ERROR_FRAGMENTED_POOL";
        case VK_ERROR_OUT_OF_POOL_MEMORY: return "VK_ERROR_OUT_OF_POOL_MEMORY";
        case VK_ERROR_INVALID_EXTERNAL_HANDLE: return "VK_ERROR_INVALID_EXTERNAL_HANDLE";
        case VK_ERROR_FRAGMENTATION: return "VK_ERROR_FRAGMENTATION";
        case VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS: return "VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS";
        case VK_ERROR_SURFACE_LOST_KHR: return "VK_ERROR_SURFACE_LOST_KHR";
        case VK_ERROR_NATIVE_WINDOW_IN_USE_KHR: return "VK_ERROR_NATIVE_WINDOW_IN_USE_KHR";
        case VK_SUBOPTIMAL_KHR: return "VK_SUBOPTIMAL_KHR";
        case VK_ERROR_OUT_OF_DATE_KHR: return "VK_ERROR_OUT_OF_DATE_KHR";
        case VK_ERROR_INCOMPATIBLE_DISPLAY_KHR: return "VK_ERROR_INCOMPATIBLE_DISPLAY_KHR";
        case VK_ERROR_VALIDATION_FAILED_EXT: return "VK_ERROR_VALIDATION_FAILED_EXT";
        case VK_ERROR_INVALID_SHADER_NV: return "VK_ERROR_INVALID_SHADER_NV";
        default: return "Unknown error";
    }
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
 * @brief Main engine class for Vulkan-based rendering
 * 
 * This class manages the Vulkan instance, device, swap chain, and rendering pipeline.
 * It provides a high-level interface for creating and managing Vulkan resources.
 * The engine is designed to be modular, with separate managers for different aspects:
 * - VulkanContext: Core Vulkan instance and device management
 * - VulkanMemoryManager: Buffer and memory management
 * - Future managers for pipelines, swapchain, etc.
 */
class VulkanEngine {
public:
    /** @brief Default window width */
    static constexpr int WIDTH = 800;
    
    /** @brief Default window height */
    static constexpr int HEIGHT = 600;
    
    /** @brief Maximum number of frames in flight */
    static constexpr int MAX_FRAMES_IN_FLIGHT = 2;
    
    /** @brief Default window title */
    static constexpr const char* WINDOW_TITLE = "Vulkan HIP Engine";
    
    /** @brief Whether validation layers are enabled */
    static constexpr bool enableValidationLayers = true;

    /**
     * @brief Construct a new Vulkan Engine object
     */
    VulkanEngine();

    /**
     * @brief Destroy the Vulkan Engine object and clean up resources
     */
    ~VulkanEngine();

    // Delete copy constructor and assignment operator
    VulkanEngine(const VulkanEngine&) = delete;
    VulkanEngine& operator=(const VulkanEngine&) = delete;

    /**
     * @brief Initialize the Vulkan instance and device
     * @throws std::runtime_error if initialization fails
     */
    void init();
    
    /**
     * @brief Run the main application loop
     */
    void run();

    /**
     * @brief Get the singleton instance
     * @return Pointer to the singleton instance
     */
    static VulkanEngine* getInstance() { return instance_; }

    /**
     * @brief Get the Vulkan context
     * @return Pointer to the Vulkan context
     */
    VulkanContext* getVulkanContext() const { return vulkanContext_.get(); }

    /**
     * @brief Get the window manager
     * @return Pointer to the window manager
     */
    WindowManager* getWindowManager() const { return windowManager_.get(); }

    /**
     * @brief Get a compute semaphore
     * @param index Index of the semaphore
     * @return Reference to the semaphore
     */
    VkSemaphore& getComputeSemaphore(uint32_t index) { return computeSemaphores[index]; }
    
    /**
     * @brief Get a compute fence
     * @param index Index of the fence
     * @return Reference to the fence
     */
    VkFence& getComputeFence(uint32_t index) { return computeFences[index]; }

    /**
     * @brief Create a shader module from SPIR-V code
     * @param code Vector containing the SPIR-V code
     * @return The created shader module
     * @throws std::runtime_error if shader module creation fails
     */
    VkShaderModule createShaderModule(const std::vector<char>& code);

    /**
     * @brief Get the current Vulkan instance
     * @return The current Vulkan instance
     */
    VkInstance getVkInstance() const { return vulkanContext_->getInstance(); }
    
    /**
     * @brief Draw a single frame
     */
    void drawFrame();
    
    /**
     * @brief Create a compute pipeline
     * @param shaderPath Path to the compute shader
     * @return The created compute pipeline
     */
    VkPipeline createComputePipeline(const std::string& shaderPath);
    
    /**
     * @brief Destroy a compute pipeline
     * @param pipeline The pipeline to destroy
     */
    void destroyComputePipeline(VkPipeline pipeline);
    
    /**
     * @brief Begin a single-time command buffer
     * @return A command buffer ready for recording
     */
    VkCommandBuffer beginSingleTimeCommands();
    
    /**
     * @brief End and submit a single-time command buffer
     * @param commandBuffer The command buffer to submit
     */
    void endSingleTimeCommands(VkCommandBuffer commandBuffer);

    VulkanMemoryManager* getMemoryManager() const { return vulkanContext_->getMemoryManager(); }

    /**
     * @brief Create an index buffer for the vertices
     */
    void createIndexBuffer();

    /**
     * @brief Callback for window framebuffer resize events
     * @param window The GLFW window
     * @param width New width
     * @param height New height
     */
    static void framebufferResizeCallback(GLFWwindow* window, int width, int height);

    // New public methods for menu system
    void drawMenu();
    void drawSavePicker();
    void newProject();
    void loadLastSave();
    void loadSave(const std::string& filename);
    void saveCurrent();
    void setAppState(App::State newState);

    void drawLoading();

    // Auto-save members
    bool autoSaveEnabled_ = true;
    std::chrono::steady_clock::time_point lastAutoSave_;
    static constexpr auto AUTO_SAVE_INTERVAL = std::chrono::minutes(5);
    static constexpr const char* AUTO_SAVE_PREFIX = "autosave_";

    // Save management members
    static constexpr size_t MAX_AUTO_SAVES = 5;  // Keep last 5 auto-saves
    static constexpr const char* MANUAL_SAVE_PREFIX = "save_";
    std::mutex saveMutex_;  // Protect save operations

    // Helper methods
    void performAutoSave();
    void updateLoadingState(const std::string& status, float progress);
    bool isAutoSaveDue() const;
    void cleanupOldAutoSaves();
    void performManualSave();
    std::string generateSaveFileName(const char* prefix) const;

    // Loading screen members
    bool isLoading_ = false;
    float loadingElapsed_ = 0.0f;
    std::future<bool> loadingFuture_;
    VoxelData loadedVoxelData_;
    std::atomic<float> loadingProgress_{0.0f};
    std::string loadingStatus_{"Loading..."};
    std::mutex loadingMutex_;  // Protect loading state access
    std::atomic<bool> shouldCancelLoading_{false};  // Flag for cancellation

    // Helper methods
    void cancelLoading();
    bool isCancelling() const { return shouldCancelLoading_; }

    /**
     * @brief Initialize ImGui
     */
    void initImGui();

    /**
     * @brief Cleanup ImGui resources
     */
    void cleanupImGui();

    /**
     * @brief Begin ImGui frame
     */
    void beginImGuiFrame();

    /**
     * @brief End ImGui frame
     */
    void endImGuiFrame();

    /**
     * @brief Create ImGui descriptor pool
     */
    void createImGuiDescriptorPool();

    /**
     * @brief Cleanup ImGui descriptor pool
     */
    void cleanupImGuiDescriptorPool();

private:
    static VulkanEngine* instance_;
    std::shared_ptr<WindowManager> windowManager_;
    VkSurfaceKHR surface_;
    std::unique_ptr<VulkanContext> vulkanContext_;
    VkInstance vkInstance_;
    VkPhysicalDevice physicalDevice_;
    VkDevice device_;
    VkQueue graphicsQueue_;
    VkQueue presentQueue_;
    VkQueue computeQueue_;
    VkPipelineLayout pipelineLayout_;
    VkPipeline graphicsPipeline_;

    // Command pools
    VkCommandPool graphicsCommandPool_ = VK_NULL_HANDLE;
    VkCommandPool computeCommandPool_ = VK_NULL_HANDLE;

    // Swap chain resources
    VkSwapchainKHR swapChain_ = VK_NULL_HANDLE;
    std::vector<VkImage> swapChainImages_;
    std::vector<VkImageView> swapChainImageViews_;
    std::vector<VkFramebuffer> swapChainFramebuffers_;
    VkFormat swapChainImageFormat_;
    VkExtent2D swapChainExtent_;
    VkRenderPass renderPass_ = VK_NULL_HANDLE;

    // Depth and color resources
    VkImage depthImage_ = VK_NULL_HANDLE;
    VmaAllocation depthImageAllocation_ = VK_NULL_HANDLE;
    VkImageView depthImageView_ = VK_NULL_HANDLE;
    VkImage colorImage_ = VK_NULL_HANDLE;
    VmaAllocation colorImageAllocation_ = VK_NULL_HANDLE;
    VkImageView colorImageView_ = VK_NULL_HANDLE;

    // Command buffers and synchronization
    std::vector<VkCommandBuffer> commandBuffers_;
    std::vector<VkSemaphore> imageAvailableSemaphores_;
    std::vector<VkSemaphore> renderFinishedSemaphores_;
    std::vector<VkFence> inFlightFences_;
    uint32_t currentFrame_ = 0;
    bool framebufferResized_ = false;

    // Descriptor resources
    VkDescriptorSetLayout descriptorSetLayout_ = VK_NULL_HANDLE;
    VkDescriptorPool descriptorPool_ = VK_NULL_HANDLE;
    std::vector<VkDescriptorSet> descriptorSets_;

    // Texture resources
    VkImageView textureImageView_ = VK_NULL_HANDLE;
    VkSampler textureSampler_ = VK_NULL_HANDLE;

    // Vulkan handles
    VkPipelineLayout pipelineLayout;
    VkPipeline graphicsPipeline;
    VkDescriptorSetLayout descriptorSetLayout;
    std::vector<VkShaderModule> shaderModules;

    VkCommandPool graphicsCommandPool;
    VkCommandPool computeCommandPool;
    std::vector<VkCommandBuffer> graphicsCommandBuffers;
    std::vector<VkCommandBuffer> computeCommandBuffers;

    std::vector<VkSemaphore> computeSemaphores;
    std::vector<VkFence> computeFences;

    VkPhysicalDeviceFeatures enabledFeatures_{};
    const std::vector<const char*> deviceExtensions_ = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        VK_KHR_MAINTENANCE1_EXTENSION_NAME,
        VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME,
        VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME
    };
    const std::vector<const char*> validationLayers_ = {
        "VK_LAYER_KHRONOS_validation"
    };
    const std::vector<const char*> instanceExtensions_ = {};

    QueueFamilyIndices queueFamilyIndices_;

    // Buffers and resources
    std::vector<VkBuffer> uniformBuffers_;
    std::vector<VmaAllocation> uniformBufferAllocations_;
    std::vector<void*> uniformBuffersMapped_;
    VkBuffer vertexBuffer_;
    VmaAllocation vertexBufferAllocation_;
    VkBuffer indexBuffer_;
    VmaAllocation indexBufferAllocation_;
    std::vector<Vertex> vertices_;
    std::vector<uint32_t> indices_;

    // New private members for menu system
    App::State currentState_ = App::State::Menu;
    App::MenuState menuState_;
    std::unique_ptr<SaveManager> saveManager_;
    bool showSavePicker_ = false;
    bool showNewProjectDialog_ = false;
    bool showSettings_ = false;
    std::vector<App::SaveInfo> saveFiles_;
    int selectedSaveIndex_ = -1;

    // Add new member for compute pipeline layout tracking
    struct ComputePipelineInfo {
        VkPipeline pipeline;
        VkPipelineLayout layout;
    };
    std::vector<ComputePipelineInfo> computePipelines_;

    EngineStateMachine stateMachine_;
    void initializeStateMachine();

    /**
     * @brief Create command pools for graphics and compute operations
     * @throws std::runtime_error if command pool creation fails
     */
    void createCommandPools();

    /**
     * @brief Create the descriptor set layout for shader resources
     * @throws std::runtime_error if descriptor set layout creation fails
     */
    void createDescriptorSetLayout();

    /**
     * @brief Create the graphics pipeline
     * @throws std::runtime_error if pipeline creation fails
     */
    void createGraphicsPipeline();

    /**
     * @brief Read a file into a vector of chars
     * @param filename Path to the file to read
     * @return Vector containing the file contents
     * @throws std::runtime_error if file cannot be opened
     */
    std::vector<char> readFile(const std::string& filename);

    /**
     * @brief Create shader stages for the graphics pipeline
     * @param vertPath Path to the vertex shader
     * @param fragPath Path to the fragment shader
     * @param vertStageInfo Output parameter for vertex shader stage info
     * @param fragStageInfo Output parameter for fragment shader stage info
     * @throws std::runtime_error if shader creation fails
     */
    void createShaderStages(const std::string& vertPath, const std::string& fragPath,
                           VkPipelineShaderStageCreateInfo& vertStageInfo,
                           VkPipelineShaderStageCreateInfo& fragStageInfo);

    /**
     * @brief Wait for compute operations to complete
     */
    void waitForComputeCompletion();

    /**
     * @brief Copy data between buffers
     * @param srcBuffer Source buffer
     * @param dstBuffer Destination buffer
     * @param size Size of data to copy
     */
    void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);

    /**
     * @brief Record commands for drawing a frame
     * @param commandBuffer Command buffer to record into
     * @param imageIndex Index of the swap chain image to render to
     */
    void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex);

    /**
     * @brief Clean up all Vulkan resources
     */
    void cleanup();

    void createSwapChain();
    void createImageViews();
    void createRenderPass();
    void createFramebuffers();
    void createVertexBuffer();
    void createDepthResources();
    void createColorResources();
    void createSyncObjects();
    void recreateSwapChain();
    void cleanupSwapChain();
    void updateUniformBuffer(uint32_t currentImage);
    void createCommandBuffers();
    void createUniformBuffers();
    void createDescriptorPool();
    void createDescriptorSets();
    VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats) const;
    VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes) const;
    VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities) const;
    SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device) const;
    VkFormat findSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features);
    VkFormat findDepthFormat();
    bool hasStencilComponent(VkFormat format);
    VkSampleCountFlagBits getMaxUsableSampleCount();

    std::vector<const char*> getRequiredInstanceExtensions();
    void applyEnabledDeviceFeatures(VkPhysicalDeviceFeatures& features);

    // Voxel rendering resources
    VkBuffer voxelVertexBuffer_ = VK_NULL_HANDLE;
    VmaAllocation voxelVertexBufferAllocation_ = VK_NULL_HANDLE;
    VkBuffer voxelIndexBuffer_ = VK_NULL_HANDLE;
    VmaAllocation voxelIndexBufferAllocation_ = VK_NULL_HANDLE;
    std::vector<Vertex> voxelVertices_;
    std::vector<uint32_t> voxelIndices_;

    // Helper methods
    void createVoxelBuffers();
    void updateVoxelBuffers();
    void createVoxelVertexData(const VoxelData& voxelData);
    void cleanupVoxelBuffers();

    // ImGui members
    VkDescriptorPool imguiDescriptorPool_;
    bool imguiInitialized_ = false;

#ifdef _MSC_VER
#pragma message("If you see an error about vk_mem_alloc.h, ensure VMA is installed via vcpkg and your includePath is set to vcpkg/installed/<triplet>/include.")
#endif
};

// Resource deleters
struct PipelineDeleter {
    void operator()(VkPipeline pipeline) const {
        if (pipeline != VK_NULL_HANDLE) {
            vkDestroyPipeline(VulkanEngine::getInstance()->getVulkanContext()->getDevice(), pipeline, nullptr);
        }
    }
};

struct PipelineLayoutDeleter {
    void operator()(VkPipelineLayout layout) const {
        if (layout != VK_NULL_HANDLE) {
            vkDestroyPipelineLayout(VulkanEngine::getInstance()->getVulkanContext()->getDevice(), layout, nullptr);
        }
    }
};

struct ShaderModuleDeleter {
    void operator()(VkShaderModule module) const {
        if (module != VK_NULL_HANDLE) {
            vkDestroyShaderModule(VulkanEngine::getInstance()->getVulkanContext()->getDevice(), module, nullptr);
        }
    }
};

} // namespace VulkanHIP 
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

// Project includes
#include "Vertex.h"
#include "VulkanContext.h"
#include "WindowManager.h"
#include "VulkanMemoryManager.h"

// Forward declarations
class VulkanMemoryManager;

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

private:
    static VulkanEngine* instance_;
    std::unique_ptr<WindowManager> windowManager_;
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

#ifdef _MSC_VER
#pragma message("If you see an error about vk_mem_alloc.h, ensure VMA is installed via vcpkg and your includePath is set to vcpkg/installed/<triplet>/include.")
#endif
}; 
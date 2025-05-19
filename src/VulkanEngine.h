#pragma once

#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>
#include <vector>
#include <string>
#include <stdexcept>
#include <optional>
#include <array>
#include <memory>
#include <set>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

// Project includes
#include "Vertex.h"

// Forward declarations
class MemoryPool;

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
 * @brief Indices for queue families required by the application
 */
struct QueueFamilyIndices {
    std::optional<uint32_t> graphicsFamily;
    std::optional<uint32_t> presentFamily;
    std::optional<uint32_t> computeFamily;

    /**
     * @brief Check if all required queues were found
     * @return True if all required queues are available
     */
    bool isComplete() const {
        return graphicsFamily.has_value() && presentFamily.has_value();
    }
};

/**
 * @brief Memory pool for efficient Vulkan resource management
 */
class MemoryPool {
public:
    /**
     * @brief Represents an allocated buffer in the pool
     */
    struct BufferAllocation {
        VkBuffer buffer;
        VkDeviceMemory memory;
        VkDeviceSize size;
        VkBufferUsageFlags usage;
        VkMemoryPropertyFlags properties;
        bool inUse;
    };

    /**
     * @brief Represents a temporary staging buffer
     */
    struct StagingBuffer {
        VkBuffer buffer;
        VkDeviceMemory memory;
        VkDeviceSize size;
        bool inUse;
    };

    /**
     * @brief Create a memory pool
     * @param device The Vulkan logical device
     */
    explicit MemoryPool(VkDevice device);
    
    /**
     * @brief Destroy the memory pool and free all resources
     */
    ~MemoryPool();

    // Delete copy constructor and assignment operator
    MemoryPool(const MemoryPool&) = delete;
    MemoryPool& operator=(const MemoryPool&) = delete;

    /**
     * @brief Allocate a buffer from the pool
     * @param size Size of the buffer in bytes
     * @param usage Buffer usage flags
     * @param properties Memory property flags
     * @return A buffer allocation
     */
    BufferAllocation allocateBuffer(VkDeviceSize size, VkBufferUsageFlags usage, 
                                   VkMemoryPropertyFlags properties);
    
    /**
     * @brief Return a buffer to the pool
     * @param allocation The buffer allocation to free
     */
    void freeBuffer(const BufferAllocation& allocation);
    
    /**
     * @brief Get a staging buffer for temporary use
     * @param size Size of the staging buffer in bytes
     * @return A staging buffer
     */
    StagingBuffer getStagingBuffer(VkDeviceSize size);
    
    /**
     * @brief Return a staging buffer to the pool
     * @param buffer The staging buffer to return
     */
    void returnStagingBuffer(const StagingBuffer& buffer);

private:
    VkDevice device;
    std::vector<BufferAllocation> bufferPool;
    std::vector<StagingBuffer> stagingPool;
    VkDeviceSize maxStagingSize;
    
    /**
     * @brief Find a suitable memory type
     * @param typeFilter Filter for memory type bits
     * @param properties Required memory properties
     * @return Index of suitable memory type
     */
    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
};

/**
 * @brief Main engine class for Vulkan-based rendering
 * 
 * This class manages the Vulkan instance, device, swap chain, and rendering pipeline.
 * It provides a high-level interface for creating and managing Vulkan resources.
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
    static VulkanEngine* getInstance() { return instance; }

    /**
     * @brief Get the memory pool
     * @return Reference to the memory pool
     */
    MemoryPool& getMemoryPool() { return *memoryPool; }

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
     * @brief Get the Vulkan logical device
     * @return The Vulkan logical device
     */
    static VkDevice getDevice() { return instance ? instance->device : VK_NULL_HANDLE; }
    
    /**
     * @brief Find a suitable memory type
     * @param typeFilter Filter for memory type bits
     * @param properties Required memory properties
     * @return Index of suitable memory type
     */
    static uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
    
    /**
     * @brief Get the physical device
     * @return The Vulkan physical device
     */
    static VkPhysicalDevice getPhysicalDevice() { return instance ? instance->physicalDevice : VK_NULL_HANDLE; }
    
    /**
     * @brief Get the compute queue
     * @return The compute queue
     */
    static VkQueue getComputeQueue() { return instance ? instance->computeQueue : VK_NULL_HANDLE; }

    /**
     * @brief Create a buffer with the specified parameters
     * @param size Size of the buffer in bytes
     * @param usage Buffer usage flags
     * @param properties Memory property flags
     * @param buffer Output parameter for the created buffer
     * @param bufferMemory Output parameter for the buffer memory
     * @throws std::runtime_error if buffer creation fails
     */
    void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, 
                     VkMemoryPropertyFlags properties, VkBuffer& buffer, 
                     VkDeviceMemory& bufferMemory);

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
    VkInstance getVkInstance() const { return vkInstance; }
    
    /**
     * @brief Get the window
     * @return The GLFW window
     */
    GLFWwindow* getWindow() const { return window; }
    
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

private:
    // Static instance pointer
    static VulkanEngine* instance;

    // Window and instance
    GLFWwindow* window;
    VkInstance vkInstance;
    VkDebugUtilsMessengerEXT debugMessenger;

    // Device and queues
    VkPhysicalDevice physicalDevice;
    VkDevice device;
    VkQueue graphicsQueue;
    VkQueue presentQueue;
    VkQueue computeQueue;
    QueueFamilyIndices queueFamilyIndices;

    // Surface
    VkSurfaceKHR surface;

    // Pipeline
    VkPipelineLayout pipelineLayout;
    VkPipeline graphicsPipeline;
    VkDescriptorSetLayout descriptorSetLayout;
    std::vector<VkShaderModule> shaderModules;

    // Command pools and buffers
    VkCommandPool graphicsCommandPool;
    VkCommandPool computeCommandPool;
    std::vector<VkCommandBuffer> graphicsCommandBuffers;
    std::vector<VkCommandBuffer> computeCommandBuffers;

    // Memory management
    std::unique_ptr<MemoryPool> memoryPool;

    // Synchronization objects
    std::vector<VkSemaphore> computeSemaphores;
    std::vector<VkFence> computeFences;

    // Device features and extensions
    struct {
        VkPhysicalDeviceProperties properties{};
        VkPhysicalDeviceMemoryProperties memoryProperties{};
    } deviceInfo;
    VkPhysicalDeviceFeatures enabledFeatures{};
    
    // List of required device extensions
    const std::vector<const char*> deviceExtensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        VK_KHR_MAINTENANCE1_EXTENSION_NAME,  // For better performance
        VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME,  // For better resource management
        VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME  // For better memory access
    };
    
    // List of validation layers to enable
    std::vector<const char*> validationLayers = {
        "VK_LAYER_KHRONOS_validation"
    };

    // Swap chain
    VkSwapchainKHR swapChain;
    std::vector<VkImage> swapChainImages;
    VkFormat swapChainImageFormat;
    VkExtent2D swapChainExtent;
    std::vector<VkImageView> swapChainImageViews;
    std::vector<VkFramebuffer> swapChainFramebuffers;

    // Render pass
    VkRenderPass renderPass;

    // Command buffers and synchronization
    std::vector<VkCommandBuffer> commandBuffers;
    std::vector<VkSemaphore> imageAvailableSemaphores;
    std::vector<VkSemaphore> renderFinishedSemaphores;
    std::vector<VkFence> inFlightFences;
    std::vector<VkFence> imagesInFlight;
    size_t currentFrame = 0;
    bool framebufferResized = false;

    // Vertex buffer
    VkBuffer vertexBuffer;
    VkDeviceMemory vertexBufferMemory;
    std::vector<Vertex> vertices;

    // Uniform buffers
    std::vector<VkBuffer> uniformBuffers;
    std::vector<VkDeviceMemory> uniformBuffersMemory;

    // Descriptor pool and sets
    VkDescriptorPool descriptorPool;
    std::vector<VkDescriptorSet> descriptorSets;

    // Depth buffer
    VkImage depthImage;
    VkDeviceMemory depthImageMemory;
    VkImageView depthImageView;

    // MSAA
    VkSampleCountFlagBits msaaSamples = VK_SAMPLE_COUNT_1_BIT;
    VkImage colorImage;
    VkDeviceMemory colorImageMemory;
    VkImageView colorImageView;

    // Initialization methods
    void initWindow();
    void initVulkan();
    void createInstance();
    void setupDebugMessenger();
    void createSurface();
    void pickPhysicalDevice();
    void createLogicalDevice();
    void createCommandPools();
    void createDescriptorSetLayout();
    void createGraphicsPipeline();

    // Helper methods
    bool checkValidationLayerSupport();
    std::vector<const char*> getRequiredExtensions();
    bool isDeviceSuitable(VkPhysicalDevice device) const;
    QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device) const;
    bool checkDeviceExtensionSupport(VkPhysicalDevice device) const;
    void enableDeviceFeatures();

    // Shader methods
    std::vector<char> readFile(const std::string& filename);
    void createShaderStages(const std::string& vertPath, const std::string& fragPath,
                           VkPipelineShaderStageCreateInfo& vertStageInfo,
                           VkPipelineShaderStageCreateInfo& fragStageInfo);

    // Compute pipeline methods
    void waitForComputeCompletion();

    // Memory management methods
    void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);

    // Command buffer methods
    void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex);

    // Cleanup methods
    void cleanup();
    void cleanupSurface();
    void destroySyncObjects();
    void destroyDebugMessenger();

    // Debug messenger methods
    void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo);
    static VkResult CreateDebugUtilsMessengerEXT(VkInstance instance,
        const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
        const VkAllocationCallbacks* pAllocator,
        VkDebugUtilsMessengerEXT* pDebugMessenger);
    static void DestroyDebugUtilsMessengerEXT(VkInstance instance,
        VkDebugUtilsMessengerEXT debugMessenger,
        const VkAllocationCallbacks* pAllocator);

    // Callback functions
    static void framebufferResizeCallback(GLFWwindow* window, int width, int height);
    static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
        VkDebugUtilsMessageTypeFlagsEXT messageType,
        const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
        void* pUserData);

    // Additional methods
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
    void createImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling,
                    VkImageUsageFlags usage, VkMemoryPropertyFlags properties,
                    VkImage& image, VkDeviceMemory& imageMemory);
    void createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags,
                        VkImageView& imageView);
    void transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout,
                             VkImageLayout newLayout);
    void copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);
    void generateMipmaps(VkImage image, VkFormat imageFormat, int32_t texWidth, int32_t texHeight, uint32_t mipLevels);
}; 
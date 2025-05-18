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

// Forward declarations
class MemoryPool;

struct SwapChainSupportDetails {
    VkSurfaceCapabilitiesKHR capabilities;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> presentModes;
};

struct QueueFamilyIndices {
    std::optional<uint32_t> graphicsFamily;
    std::optional<uint32_t> presentFamily;
    std::optional<uint32_t> computeFamily;  // For HIP compute operations

    bool isComplete() const {
        return graphicsFamily.has_value() && presentFamily.has_value();
    }
};

class MemoryPool {
public:
    struct BufferAllocation {
        VkBuffer buffer;
        VkDeviceMemory memory;
        VkDeviceSize size;
        VkBufferUsageFlags usage;
        VkMemoryPropertyFlags properties;
        bool inUse;
    };

    struct StagingBuffer {
        VkBuffer buffer;
        VkDeviceMemory memory;
        VkDeviceSize size;
        bool inUse;
    };

    MemoryPool(VkDevice device);
    ~MemoryPool();

    BufferAllocation allocateBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties);
    void freeBuffer(const BufferAllocation& allocation);
    StagingBuffer getStagingBuffer(VkDeviceSize size);
    void returnStagingBuffer(const StagingBuffer& buffer);

private:
    VkDevice device;
    std::vector<BufferAllocation> bufferPool;
    std::vector<StagingBuffer> stagingPool;
    VkDeviceSize maxStagingSize;
    
    // Add findMemoryType as a private method
    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
};

class VulkanEngine {
public:
    static constexpr int WIDTH = 800;
    static constexpr int HEIGHT = 600;
    static constexpr int MAX_FRAMES_IN_FLIGHT = 2;
    static constexpr const char* WINDOW_TITLE = "Vulkan HIP Engine";

    VulkanEngine();
    ~VulkanEngine();

    // Delete copy constructor and assignment operator
    VulkanEngine(const VulkanEngine&) = delete;
    VulkanEngine& operator=(const VulkanEngine&) = delete;

    void init();
    void run();

    // Static instance getter
    static VulkanEngine* getInstance() { return instance; }

    // Add new public accessors
    MemoryPool& getMemoryPool() { return *memoryPool; }
    VkSemaphore& getComputeSemaphore(uint32_t index) { return computeSemaphores[index]; }
    VkFence& getComputeFence(uint32_t index) { return computeFences[index]; }

    // Static accessors for device and memory management
    static VkDevice getDevice() { return instance ? instance->device : VK_NULL_HANDLE; }
    static uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
    static VkPhysicalDevice getPhysicalDevice() { return instance ? instance->physicalDevice : VK_NULL_HANDLE; }

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
    const std::vector<const char*> deviceExtensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        VK_KHR_MAINTENANCE1_EXTENSION_NAME,  // For better performance
        VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME,  // For better resource management
        VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME  // For better memory access
    };
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
    void createInstance();
    void setupDebugMessenger();
    void createSurface();
    void pickPhysicalDevice();
    void createLogicalDevice();
    void createCommandPools();
    void createCommandBuffers();
    void createDescriptorSetLayout();
    void createSyncObjects();
    void createGraphicsPipeline();

    // Helper methods
    bool checkValidationLayerSupport();
    std::vector<const char*> getRequiredExtensions();
    bool isDeviceSuitable(VkPhysicalDevice device);
    QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device);
    bool checkDeviceExtensionSupport(VkPhysicalDevice device);
    void enableDeviceFeatures();

    // Shader methods
    VkShaderModule createShaderModule(const std::vector<char>& code);
    std::vector<char> readFile(const std::string& filename);
    void createShaderStages(const std::string& vertPath, const std::string& fragPath,
                           VkPipelineShaderStageCreateInfo& vertStageInfo,
                           VkPipelineShaderStageCreateInfo& fragStageInfo);

    // Compute pipeline methods
    VkPipeline createComputePipeline(const std::string& shaderPath);
    void destroyComputePipeline(VkPipeline pipeline);
    void waitForComputeCompletion();

    // Memory management methods
    VkBuffer createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties);
    void destroyBuffer(VkBuffer buffer, VkDeviceMemory memory);

    // Command buffer methods
    VkCommandBuffer beginSingleTimeCommands();
    void endSingleTimeCommands(VkCommandBuffer commandBuffer);
    void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);
    void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex);

    // Cleanup methods
    void cleanup();
    void cleanupSurface();
    void destroySyncObjects();
    void destroyDebugMessenger();

    // Callback functions
    static void framebufferResizeCallback(GLFWwindow* window, int width, int height);
    static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
        VkDebugUtilsMessageTypeFlagsEXT messageType,
        const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
        void* pUserData);

    // New private methods
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
    void drawFrame();
    void createCommandBuffers();
    void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex);
    VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
    VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);
    VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities);
    SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device);
    VkFormat findSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features);
    VkFormat findDepthFormat();
    bool hasStencilComponent(VkFormat format);
    VkSampleCountFlagBits getMaxUsableSampleCount();
    void createImage(uint32_t width, uint32_t height, uint32_t mipLevels, VkSampleCountFlagBits numSamples,
                    VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage,
                    VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& imageMemory);
    void createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags, uint32_t mipLevels, VkImageView& imageView);
    void transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout, uint32_t mipLevels);
    void copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);
    void generateMipmaps(VkImage image, VkFormat imageFormat, int32_t texWidth, int32_t texHeight, uint32_t mipLevels);
}; 
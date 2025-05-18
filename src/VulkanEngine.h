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
    // Add accessor for physicalDevice
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
    VkPhysicalDeviceFeatures deviceInfo{};
    const std::vector<const char*> deviceExtensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
    };
    const std::vector<const char*> validationLayers = {
        "VK_LAYER_KHRONOS_validation"
    };

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
    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
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
}; 
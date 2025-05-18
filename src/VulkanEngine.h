#pragma once

#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>
#include <vector>
#include <string>
#include <stdexcept>
#include <optional>
#include <array>

struct QueueFamilyIndices {
    std::optional<uint32_t> graphicsFamily;
    std::optional<uint32_t> presentFamily;
    std::optional<uint32_t> computeFamily;  // For HIP compute operations

    bool isComplete() {
        return graphicsFamily.has_value() && presentFamily.has_value();
    }
};

class VulkanEngine {
public:
    VulkanEngine();
    ~VulkanEngine();

    void init();
    void cleanup();
    void run();

    // Static accessors for device and memory management
    static VkDevice getDevice() { return device; }
    static uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);

private:
    // Window
    GLFWwindow* window;
    const uint32_t WIDTH = 800;
    const uint32_t HEIGHT = 600;
    const char* WINDOW_TITLE = "Vulkan HIP Engine";

    // Vulkan instance and device
    VkInstance instance;
    VkPhysicalDevice physicalDevice;
    VkDevice device;
    VkQueue graphicsQueue;
    VkQueue presentQueue;
    VkQueue computeQueue;  // For HIP compute operations
    
    // Surface
    VkSurfaceKHR surface;
    
    // Pipeline components
    VkPipelineLayout pipelineLayout;
    VkPipeline graphicsPipeline;
    
    // Command pool and buffers
    VkCommandPool graphicsCommandPool;
    VkCommandPool computeCommandPool;  // Separate pool for compute operations
    std::vector<VkCommandBuffer> graphicsCommandBuffers;
    std::vector<VkCommandBuffer> computeCommandBuffers;
    
    // Performance optimization structures
    struct {
        VkPhysicalDeviceFeatures features{};
        VkPhysicalDeviceProperties properties{};
        VkPhysicalDeviceMemoryProperties memoryProperties{};
    } deviceInfo;

    // Queue family indices
    QueueFamilyIndices queueFamilyIndices;

    // Private helper methods
    void createInstance();
    void pickPhysicalDevice();
    void createLogicalDevice();
    void createCommandPools();
    void createGraphicsPipeline();
    
    // Window and surface methods
    void initWindow();
    void createSurface();
    void cleanupSurface();
    
    // Queue family methods
    QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device);
    
    // Performance optimization methods
    void enableDeviceFeatures();
    void createCommandBuffers();
    void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex);
    
    // Validation layers
    const std::vector<const char*> validationLayers = {
        "VK_LAYER_KHRONOS_validation"
    };
    
    // Device extensions
    const std::vector<const char*> deviceExtensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        VK_KHR_MAINTENANCE1_EXTENSION_NAME,  // For better performance
        VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME,  // For better resource management
        VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME  // For better memory access
    };

    // Helper functions
    bool checkValidationLayerSupport();
    std::vector<const char*> getRequiredExtensions();
    bool isDeviceSuitable(VkPhysicalDevice device);
    bool checkDeviceExtensionSupport(VkPhysicalDevice device);

    // Static callback functions
    static void framebufferResizeCallback(GLFWwindow* window, int width, int height);

    // Compute pipeline support
    VkShaderModule createShaderModule(const std::vector<char>& code);
    VkPipeline createComputePipeline(const std::string& shaderPath);
    void destroyComputePipeline(VkPipeline pipeline);
    
    // Memory management
    VkBuffer createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties);
    void destroyBuffer(VkBuffer buffer, VkDeviceMemory memory);
    void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);
    
    // Command buffer management
    VkCommandBuffer beginSingleTimeCommands();
    void endSingleTimeCommands(VkCommandBuffer commandBuffer);
}; 
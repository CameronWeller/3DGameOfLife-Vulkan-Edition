#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include <string>
#include <memory>
#include <mutex>
#include "DeviceManager.h"
#include "WindowManager.h"
// #include "VulkanMemoryManager.h"  // Temporarily disabled for minimal build
#include "Logger.h"
#include "QueueFamilyIndices.h"
#include "SwapChainSupportDetails.h"
#include <optional>

// Forward declaration for PFN_vkCreateDebugUtilsMessengerEXT and PFN_vkDestroyDebugUtilsMessengerEXT
// Or include vulkan_core.h if it's guaranteed to have them, but usually it's safer to load them explicitly.
// typedef VkResult (VKAPI_PTR *PFN_vkCreateDebugUtilsMessengerEXT)(VkInstance, const VkDebugUtilsMessengerCreateInfoEXT*, const VkAllocationCallbacks*, VkDebugUtilsMessengerEXT*);
// typedef void (VKAPI_PTR *PFN_vkDestroyDebugUtilsMessengerEXT)(VkInstance, VkDebugUtilsMessengerEXT, const VkAllocationCallbacks*);

// Forward declarations
struct ValidationLayerConfig;

namespace VulkanHIP {

// Validation layer configuration structure
struct ValidationLayerConfig {
    bool enabled = true;
    std::vector<const char*> layers = { "VK_LAYER_KHRONOS_validation" };
    
    // Message severities
    bool verboseMessages = false;
    bool infoMessages = true;
    bool warningMessages = true;
    bool errorMessages = true;
    
    // Message types
    bool generalMessages = true;
    bool validationMessages = true;
    bool performanceMessages = true;
    
    // Validation features
    bool gpuAssistedValidation = true;
    bool gpuAssistedReserveBindingSlot = true;
    bool bestPracticesValidation = true;
    bool debugPrintf = false;
    bool synchronizationValidation = true;
    
    // Cache settings
    bool enableCache = true;
    std::string cachePath = "validation_cache.bin";
    size_t maxCacheSizeMB = 100;
};

class VulkanContext {
public:
    static VulkanContext& getInstance() {
        static VulkanContext instance;
        return instance;
    }

    void init(const std::vector<const char*>& requiredExtensions);
    void cleanup();

    // Getters
    VkInstance getVkInstance() const { return vkInstance_; }
    VkPhysicalDevice getPhysicalDevice() const { return physicalDevice_; }
    VkDevice getDevice() const { return device_; }
    VkSurfaceKHR getSurface() const { return surface_; }
    VkQueue getGraphicsQueue() const { return graphicsQueue_; }
    VkQueue getPresentQueue() const { return presentQueue_; }
    VkQueue getComputeQueue() const { return computeQueue_; }
    QueueFamilyIndices getQueueFamilyIndices() const { return queueFamilyIndices_; }
    // VulkanMemoryManager& getMemoryManager() { return *memoryManager_; }  // Temporarily disabled
    VkCommandPool getGraphicsCommandPool() const { return graphicsCommandPool_; }
    VkCommandPool getComputeCommandPool() const { return computeCommandPool_; }

    // Command buffer utilities
    VkCommandBuffer beginSingleTimeCommands() {
        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandPool = graphicsCommandPool_;
        allocInfo.commandBufferCount = 1;

        VkCommandBuffer commandBuffer;
        vkAllocateCommandBuffers(device_, &allocInfo, &commandBuffer);

        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        vkBeginCommandBuffer(commandBuffer, &beginInfo);

        return commandBuffer;
    }

    void endSingleTimeCommands(VkCommandBuffer commandBuffer) {
        vkEndCommandBuffer(commandBuffer);

        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffer;

        vkQueueSubmit(graphicsQueue_, 1, &submitInfo, VK_NULL_HANDLE);
        vkQueueWaitIdle(graphicsQueue_);

        vkFreeCommandBuffers(device_, graphicsCommandPool_, 1, &commandBuffer);
    }

    SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device) const;
    void createSurface();
    void destroySurface();
    VkSurfaceKHR createWindowSurface() const;

    static uint32_t findMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags properties);

    QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device) const;
    
    // Validation layer configuration
    void setValidationLayerConfig(const ValidationLayerConfig& config);
    bool areValidationLayersEnabled() const { return enableValidationLayers_; }

private:
    VulkanContext();
    ~VulkanContext();

    void createInstance(const std::vector<const char*>& requiredExtensions);
    void setupDebugMessenger();
    void pickPhysicalDevice();
    void createLogicalDevice();
    void createCommandPools();
    bool checkValidationLayerSupport() const;
    bool checkDeviceExtensionSupport(VkPhysicalDevice device) const;
    bool isDeviceSuitable(VkPhysicalDevice device) const;
    void saveValidationCache();
    void loadValidationFeatures();
    void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo);

    static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
        VkDebugUtilsMessageTypeFlagsEXT messageType,
        const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
        void* pUserData);

    mutable std::mutex contextMutex_;
    VkInstance vkInstance_ = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT debugMessenger_ = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice_ = VK_NULL_HANDLE;
    VkDevice device_ = VK_NULL_HANDLE;
    VkSurfaceKHR surface_ = VK_NULL_HANDLE;
    VkQueue graphicsQueue_ = VK_NULL_HANDLE;
    VkQueue presentQueue_ = VK_NULL_HANDLE;
    VkQueue computeQueue_ = VK_NULL_HANDLE;
    VkCommandPool graphicsCommandPool_ = VK_NULL_HANDLE;
    VkCommandPool computeCommandPool_ = VK_NULL_HANDLE;
    QueueFamilyIndices queueFamilyIndices_;
    
    // Validation layer configuration
    bool enableValidationLayers_ = true;
    std::vector<const char*> validationLayers_ = { "VK_LAYER_KHRONOS_validation" };
    VkValidationCacheEXT validationCache_ = VK_NULL_HANDLE;
    VkSemaphore graphicsComputeSemaphore_ = VK_NULL_HANDLE;
    VkSemaphore computePresentSemaphore_ = VK_NULL_HANDLE;
    
    // Validation features
    std::vector<VkValidationFeatureEnableEXT> enabledValidationFeatures_;
    std::vector<VkValidationFeatureDisableEXT> disabledValidationFeatures_;
    ValidationLayerConfig validationConfig_;
    
    mutable std::vector<const char*> deviceExtensions_ = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
    // std::unique_ptr<VulkanMemoryManager> memoryManager_;  // Temporarily disabled
    std::unique_ptr<DeviceManager> deviceManager_;
};

} // namespace VulkanHIP 
#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include <string>
#include <memory>
#include <mutex>
#include "DeviceManager.h"
#include "WindowManager.h"
#include "VulkanMemoryManager.h"
#include "Logger.h"
#include "QueueFamilyIndices.h"
#include "SwapChainSupportDetails.h"
#include <optional>

// Forward declaration for PFN_vkCreateDebugUtilsMessengerEXT and PFN_vkDestroyDebugUtilsMessengerEXT
// Or include vulkan_core.h if it's guaranteed to have them, but usually it's safer to load them explicitly.
// typedef VkResult (VKAPI_PTR *PFN_vkCreateDebugUtilsMessengerEXT)(VkInstance, const VkDebugUtilsMessengerCreateInfoEXT*, const VkAllocationCallbacks*, VkDebugUtilsMessengerEXT*);
// typedef void (VKAPI_PTR *PFN_vkDestroyDebugUtilsMessengerEXT)(VkInstance, VkDebugUtilsMessengerEXT, const VkAllocationCallbacks*);

namespace VulkanHIP {

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
    VulkanMemoryManager& getMemoryManager() { return *memoryManager_; }

    SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device) const;
    void createSurface();
    void destroySurface();
    VkSurfaceKHR createWindowSurface() const;

    static uint32_t findMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags properties);

    QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device) const;

private:
    VulkanContext();
    ~VulkanContext();

    void createInstance(const std::vector<const char*>& requiredExtensions);
    void setupDebugMessenger();
    void pickPhysicalDevice();
    void createLogicalDevice();
    bool checkValidationLayerSupport() const;
    bool checkDeviceExtensionSupport(VkPhysicalDevice device) const;
    bool isDeviceSuitable(VkPhysicalDevice device) const;
    void initDeviceManager();

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
    QueueFamilyIndices queueFamilyIndices_;
    std::vector<const char*> validationLayers_ = { "VK_LAYER_KHRONOS_validation" };
    std::vector<const char*> deviceExtensions_ = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
    std::unique_ptr<VulkanMemoryManager> memoryManager_;
    std::unique_ptr<DeviceManager> deviceManager_;
};

} // namespace VulkanHIP

// ... existing code ... 
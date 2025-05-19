#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include <string>
#include <memory>
#include "DeviceManager.h"
#include "WindowManager.h"
#include "VulkanMemoryManager.h"

// Forward declaration for PFN_vkCreateDebugUtilsMessengerEXT and PFN_vkDestroyDebugUtilsMessengerEXT
// Or include vulkan_core.h if it's guaranteed to have them, but usually it's safer to load them explicitly.
// typedef VkResult (VKAPI_PTR *PFN_vkCreateDebugUtilsMessengerEXT)(VkInstance, const VkDebugUtilsMessengerCreateInfoEXT*, const VkAllocationCallbacks*, VkDebugUtilsMessengerEXT*);
// typedef void (VKAPI_PTR *PFN_vkDestroyDebugUtilsMessengerEXT)(VkInstance, VkDebugUtilsMessengerEXT, const VkAllocationCallbacks*);

class VulkanContext {
public:
    VulkanContext(std::shared_ptr<WindowManager> windowManager, 
                 const std::vector<const char*>& requiredInstanceExtensions,
                 const std::vector<const char*>& validationLayers);
    ~VulkanContext();

    VulkanContext(const VulkanContext&) = delete;
    VulkanContext& operator=(const VulkanContext&) = delete;

    void initDeviceManager(const std::vector<const char*>& deviceExtensions, const VkPhysicalDeviceFeatures& enabledFeatures);

    VkInstance getInstance() const { return vkInstance_; }
    VkSurfaceKHR getSurface() const { return surface_; }
    DeviceManager* getDeviceManager() const { return deviceManager_.get(); }
    VkDevice getDevice() const { return deviceManager_ ? deviceManager_->getDevice() : VK_NULL_HANDLE; }
    VkPhysicalDevice getPhysicalDevice() const { return deviceManager_ ? deviceManager_->getPhysicalDevice() : VK_NULL_HANDLE; }
    VkQueue getGraphicsQueue() const { return deviceManager_ ? deviceManager_->getGraphicsQueue() : VK_NULL_HANDLE; }
    VkQueue getPresentQueue() const { return deviceManager_ ? deviceManager_->getPresentQueue() : VK_NULL_HANDLE; }
    VkQueue getComputeQueue() const { return deviceManager_ ? deviceManager_->getComputeQueue() : VK_NULL_HANDLE; }
    const QueueFamilyIndices& getQueueFamilyIndices() const;
    VulkanMemoryManager* getMemoryManager() const { return memoryManager_.get(); }

    const std::vector<const char*>& getValidationLayers() const { return validationLayers_; }
    bool areValidationLayersEnabled() const { return !validationLayers_.empty(); }

    static uint32_t findMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags properties);

private:
    void createInstance();
    void setupDebugMessenger();
    void createSurface();
    void cleanupSurface();
    void cleanupDebugMessenger();

    bool checkValidationLayerSupport() const;

    // From VulkanEngine, made static or member of VulkanContext
    static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
        VkDebugUtilsMessageTypeFlagsEXT messageType,
        const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
        void* pUserData);
    
    // Helper functions for loading debug messenger functions (moved from VulkanEngine.cpp)
    static VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger);
    static void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator);

    std::shared_ptr<WindowManager> windowManager_;
    VkInstance vkInstance_ = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT debugMessenger_ = VK_NULL_HANDLE;
    VkSurfaceKHR surface_ = VK_NULL_HANDLE;
    
    std::unique_ptr<DeviceManager> deviceManager_;
    std::unique_ptr<VulkanMemoryManager> memoryManager_;

    std::vector<const char*> requiredInstanceExtensions_;
    std::vector<const char*> validationLayers_;
}; 
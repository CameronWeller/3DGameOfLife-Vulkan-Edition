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
#include <optional>

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

    // Thread-safe getters
    VkInstance getInstance() const { 
        std::lock_guard<std::mutex> lock(contextMutex_);
        return vkInstance_; 
    }
    VkSurfaceKHR getSurface() const { 
        std::lock_guard<std::mutex> lock(contextMutex_);
        return surface_; 
    }
    DeviceManager* getDeviceManager() const { 
        std::lock_guard<std::mutex> lock(contextMutex_);
        return deviceManager_.get(); 
    }
    VkDevice getDevice() const { 
        std::lock_guard<std::mutex> lock(contextMutex_);
        return deviceManager_ ? deviceManager_->getDevice() : VK_NULL_HANDLE; 
    }
    VkPhysicalDevice getPhysicalDevice() const { 
        std::lock_guard<std::mutex> lock(contextMutex_);
        return deviceManager_ ? deviceManager_->getPhysicalDevice() : VK_NULL_HANDLE; 
    }
    VkQueue getGraphicsQueue() const { 
        std::lock_guard<std::mutex> lock(contextMutex_);
        return deviceManager_ ? deviceManager_->getGraphicsQueue() : VK_NULL_HANDLE; 
    }
    VkQueue getPresentQueue() const { 
        std::lock_guard<std::mutex> lock(contextMutex_);
        return deviceManager_ ? deviceManager_->getPresentQueue() : VK_NULL_HANDLE; 
    }
    VkQueue getComputeQueue() const { 
        std::lock_guard<std::mutex> lock(contextMutex_);
        return deviceManager_ ? deviceManager_->getComputeQueue() : VK_NULL_HANDLE; 
    }
    const QueueFamilyIndices& getQueueFamilyIndices() const;
    VulkanMemoryManager* getMemoryManager() const { 
        std::lock_guard<std::mutex> lock(contextMutex_);
        return memoryManager_.get(); 
    }

    const std::vector<const char*>& getValidationLayers() const { 
        std::lock_guard<std::mutex> lock(contextMutex_);
        return validationLayers_; 
    }
    bool areValidationLayersEnabled() const { 
        std::lock_guard<std::mutex> lock(contextMutex_);
        return !validationLayers_.empty(); 
    }

    static uint32_t findMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags properties);

    void initVulkan(GLFWwindow* window);
    void cleanup();

    VkCommandPool getCommandPool() const { return commandPool_; }

    QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device) const;
    SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device) const;

private:
    void createInstance();
    void setupDebugMessenger();
    void createSurface(GLFWwindow* window);
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

    mutable std::mutex contextMutex_;
    std::shared_ptr<WindowManager> windowManager_;
    VkInstance vkInstance_ = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT debugMessenger_ = VK_NULL_HANDLE;
    VkSurfaceKHR surface_ = VK_NULL_HANDLE;
    
    std::unique_ptr<DeviceManager> deviceManager_;
    std::unique_ptr<VulkanMemoryManager> memoryManager_;

    std::vector<const char*> requiredInstanceExtensions_;
    std::vector<const char*> validationLayers_;

    VkPhysicalDevice physicalDevice_ = VK_NULL_HANDLE;
    VkDevice device_;
    VkQueue graphicsQueue_;
    VkQueue presentQueue_;
    VkCommandPool commandPool_;
};

struct QueueFamilyIndices {
    std::optional<uint32_t> graphicsFamily;
    std::optional<uint32_t> presentFamily;

    bool isComplete() const {
        return graphicsFamily.has_value() && presentFamily.has_value();
    }
};

struct SwapChainSupportDetails {
    VkSurfaceCapabilitiesKHR capabilities;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> presentModes;
}; 
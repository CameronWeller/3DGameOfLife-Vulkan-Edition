#include "VulkanContext.h"
#include <stdexcept>
#include <iostream> // For logging, to be replaced
#include <vector>
#include <cstring> // For strcmp
#include <GLFW/glfw3.h> // Required for glfwGetRequiredInstanceExtensions

// Static callback function needs to be defined outside the class or be static member
VKAPI_ATTR VkBool32 VKAPI_CALL VulkanContext::debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData) {
    std::cerr << "Validation layer: " << pCallbackData->pMessage << std::endl;
    return VK_FALSE;
}

VkResult VulkanContext::CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger) {
    auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
    if (func != nullptr) {
        return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
    } else {
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }
}

void VulkanContext::DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator) {
    auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
    if (func != nullptr) {
        func(instance, debugMessenger, pAllocator);
    }
}

VulkanContext::VulkanContext(std::shared_ptr<WindowManager> windowManager,
                           const std::vector<const char*>& requiredInstanceExtensions,
                           const std::vector<const char*>& validationLayers)
    : windowManager_(windowManager),
      requiredInstanceExtensions_(requiredInstanceExtensions),
      validationLayers_(validationLayers) {
    if (!windowManager_) {
        throw std::runtime_error("VulkanContext: WindowManager cannot be null.");
    }
    createInstance();
    if (areValidationLayersEnabled()) {
       setupDebugMessenger();
    }
    createSurface();
}

VulkanContext::~VulkanContext() {
    // Clean up in reverse order of creation
    memoryManager_.reset();  // This will call VulkanMemoryManager's destructor
    deviceManager_.reset();  // This will call DeviceManager's destructor
    cleanupSurface();
    cleanupDebugMessenger();
    vkDestroyInstance(vkInstance_, nullptr);
}

void VulkanContext::cleanup() {
    if (deviceManager_) {
        deviceManager_.reset(); // Destroys DeviceManager, which destroys logical device
    }
    cleanupSurface(); // Must be before instance destruction
    cleanupDebugMessenger();
    if (vkInstance_ != VK_NULL_HANDLE) {
        vkDestroyInstance(vkInstance_, nullptr);
        vkInstance_ = VK_NULL_HANDLE;
    }
}

void VulkanContext::initDeviceManager(const std::vector<const char*>& deviceExtensions, const VkPhysicalDeviceFeatures& enabledFeatures) {
    if (vkInstance_ == VK_NULL_HANDLE) {
        throw std::runtime_error("Cannot init DeviceManager: Vulkan instance is not initialized.");
    }
    if (surface_ == VK_NULL_HANDLE && windowManager_) {
        std::cout << "Warning: Initializing DeviceManager without a VkSurfaceKHR, graphics/present queues may not be available." << std::endl;
    }
    deviceManager_ = std::make_unique<DeviceManager>(vkInstance_, surface_, deviceExtensions, enabledFeatures);
    
    // Initialize memory manager after device is created
    memoryManager_ = std::make_unique<VulkanMemoryManager>(getDevice(), getPhysicalDevice());
}

const QueueFamilyIndices& VulkanContext::getQueueFamilyIndices() const {
    if (!deviceManager_) {
        throw std::runtime_error("Queue families not available: DeviceManager not initialized.");
    }
    return deviceManager_->getQueueFamilyIndices();
}

void VulkanContext::createInstance() {
    if (areValidationLayersEnabled() && !checkValidationLayerSupport()) {
        throw std::runtime_error("Validation layers requested, but not available!");
    }

    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "Vulkan HIP Engine";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "ModularEngine";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_2;

    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;

    // Get extensions required by GLFW and add debug utils if validation is on
    uint32_t glfwExtensionCount = 0;
    const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
    std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);
    extensions.insert(extensions.end(), requiredInstanceExtensions_.begin(), requiredInstanceExtensions_.end());

    if (areValidationLayersEnabled()) {
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }
    std::sort(extensions.begin(), extensions.end());
    extensions.erase(std::unique(extensions.begin(), extensions.end()), extensions.end());

    createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();

    VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
    if (areValidationLayersEnabled()) {
        createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers_.size());
        createInfo.ppEnabledLayerNames = validationLayers_.data();
        
        debugCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        debugCreateInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                                       VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                                       VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        debugCreateInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                                   VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                                   VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        debugCreateInfo.pfnUserCallback = debugCallback;
        createInfo.pNext = &debugCreateInfo;
    } else {
        createInfo.enabledLayerCount = 0;
        createInfo.pNext = nullptr;
    }

    if (vkCreateInstance(&createInfo, nullptr, &vkInstance_) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create Vulkan instance!");
    }
}

void VulkanContext::setupDebugMessenger() {
    if (!areValidationLayersEnabled() || vkInstance_ == VK_NULL_HANDLE) return;

    VkDebugUtilsMessengerCreateInfoEXT createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                               VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                               VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    createInfo.pfnUserCallback = debugCallback;
    createInfo.pUserData = nullptr;

    if (CreateDebugUtilsMessengerEXT(vkInstance_, &createInfo, nullptr, &debugMessenger_) != VK_SUCCESS) {
        throw std::runtime_error("Failed to set up debug messenger!");
    }
}

void VulkanContext::createSurface() {
    if (!windowManager_) {
        surface_ = VK_NULL_HANDLE;
        std::cout << "No WindowManager provided, skipping VkSurfaceKHR creation (compute-only context?)." << std::endl;
        return;
    }
    surface_ = windowManager_->createSurface(vkInstance_);
}

void VulkanContext::cleanupSurface() {
    if (surface_ != VK_NULL_HANDLE && windowManager_) {
        windowManager_->destroySurface(vkInstance_, surface_);
        surface_ = VK_NULL_HANDLE;
    }
}

void VulkanContext::cleanupDebugMessenger() {
    if (debugMessenger_ != VK_NULL_HANDLE) {
        DestroyDebugUtilsMessengerEXT(vkInstance_, debugMessenger_, nullptr);
        debugMessenger_ = VK_NULL_HANDLE;
    }
}

bool VulkanContext::checkValidationLayerSupport() const {
    uint32_t layerCount;
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
    std::vector<VkLayerProperties> availableLayers(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

    for (const char* layerName : validationLayers_) {
        bool layerFound = false;
        for (const auto& layerProperties : availableLayers) {
            if (strcmp(layerName, layerProperties.layerName) == 0) {
                layerFound = true;
                break;
            }
        }
        if (!layerFound) {
            std::cerr << "Validation layer not found: " << layerName << std::endl;
            return false;
        }
    }
    return true;
}

uint32_t VulkanContext::findMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags properties) {
    if (physicalDevice == VK_NULL_HANDLE) {
        throw std::runtime_error("VulkanContext::findMemoryType: Physical device is null.");
    }
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);
    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    throw std::runtime_error("VulkanContext: Failed to find suitable memory type!");
} 
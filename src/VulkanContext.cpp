#include "VulkanContext.h"
#include "WindowManager.h"
#include <stdexcept>
#include <iostream> // For logging, to be replaced
#include <vector>
#include <cstring> // For strcmp
#include <GLFW/glfw3.h> // Required for glfwGetRequiredInstanceExtensions
#include <algorithm>
#include <fstream>
#include <chrono>
#include <iomanip>
#include <mutex>
#include <set>
#include <sstream>
#include <unordered_map>

// Add after the includes
static std::ofstream validationLogFile;
static bool validationLogInitialized = false;
static std::mutex validationLogMutex;

static void initializeValidationLog() {
    std::lock_guard<std::mutex> lock(validationLogMutex);
    if (!validationLogInitialized) {
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        std::stringstream ss;
        ss << "vulkan_validation_" << std::put_time(std::localtime(&time), "%Y%m%d_%H%M%S") << ".log";
        validationLogFile.open(ss.str(), std::ios::out | std::ios::app);
        if (!validationLogFile.is_open()) {
            throw std::runtime_error("Failed to open validation log file!");
        }
        validationLogInitialized = true;
    }
}

static void logValidationMessage(const std::string& message, VkDebugUtilsMessageSeverityFlagBitsEXT severity) {
    std::lock_guard<std::mutex> lock(validationLogMutex);
    if (validationLogFile.is_open()) {
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        validationLogFile << std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M:%S") << " [";
        
        switch (severity) {
            case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
                validationLogFile << "VERBOSE";
                break;
            case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
                validationLogFile << "INFO";
                break;
            case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
                validationLogFile << "WARNING";
                break;
            case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
                validationLogFile << "ERROR";
                break;
            default:
                validationLogFile << "UNKNOWN";
        }
        
        validationLogFile << "] " << message << std::endl;
        validationLogFile.flush();
    }
}

namespace VulkanHIP {

// Static callback function needs to be defined outside the class or be static member
VKAPI_ATTR VkBool32 VKAPI_CALL VulkanContext::debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData) {
    
    std::stringstream message;
    message << "Validation layer: " << pCallbackData->pMessage;
    
    if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
        std::cerr << message.str() << std::endl;
    }
    
    logValidationMessage(message.str(), messageSeverity);
    
    return VK_FALSE;
}

VulkanContext& VulkanContext::getInstance() {
    static VulkanContext instance;
    return instance;
}

VulkanContext::VulkanContext() {
    // Initialize Vulkan objects to null
    vkInstance_ = VK_NULL_HANDLE;
    debugMessenger_ = VK_NULL_HANDLE;
    physicalDevice_ = VK_NULL_HANDLE;
    device_ = VK_NULL_HANDLE;
    surface_ = VK_NULL_HANDLE;
    graphicsQueue_ = VK_NULL_HANDLE;
    presentQueue_ = VK_NULL_HANDLE;
    computeQueue_ = VK_NULL_HANDLE;
}

VulkanContext::~VulkanContext() {
    cleanup();
}

void VulkanContext::init(const std::vector<const char*>& requiredExtensions) {
    createInstance(requiredExtensions);
    setupDebugMessenger();
    pickPhysicalDevice();
    createLogicalDevice();
    createSurface();
}

void VulkanContext::cleanup() {
    std::lock_guard<std::mutex> lock(contextMutex_);

    // Wait for device to finish operations
    if (device_ != VK_NULL_HANDLE) {
        VkResult result = vkDeviceWaitIdle(device_);
        if (result != VK_SUCCESS) {
            std::cerr << "Warning: Device wait idle failed during cleanup: " << getVulkanResultString(result) << std::endl;
        }
    }

    // Clean up synchronization primitives
    if (device_ != VK_NULL_HANDLE) {
        if (graphicsComputeSemaphore_ != VK_NULL_HANDLE) {
            vkDestroySemaphore(device_, graphicsComputeSemaphore_, nullptr);
            graphicsComputeSemaphore_ = VK_NULL_HANDLE;
        }
        if (computePresentSemaphore_ != VK_NULL_HANDLE) {
            vkDestroySemaphore(device_, computePresentSemaphore_, nullptr);
            computePresentSemaphore_ = VK_NULL_HANDLE;
        }
    }

    // Clean up validation cache
    if (validationCache_ != VK_NULL_HANDLE) {
        auto func = (PFN_vkDestroyValidationCacheEXT)vkGetDeviceProcAddr(device_, "vkDestroyValidationCacheEXT");
        if (func != nullptr) {
            func(device_, validationCache_, nullptr);
        }
        validationCache_ = VK_NULL_HANDLE;
    }

    // Clean up debug messenger
    if (debugMessenger_ != VK_NULL_HANDLE) {
        auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(vkInstance_, "vkDestroyDebugUtilsMessengerEXT");
        if (func != nullptr) {
            func(vkInstance_, debugMessenger_, nullptr);
        }
        debugMessenger_ = VK_NULL_HANDLE;
    }

    // Clean up device
    if (device_ != VK_NULL_HANDLE) {
        vkDestroyDevice(device_, nullptr);
        device_ = VK_NULL_HANDLE;
    }

    // Clean up surface
    if (surface_ != VK_NULL_HANDLE) {
        vkDestroySurfaceKHR(vkInstance_, surface_, nullptr);
        surface_ = VK_NULL_HANDLE;
    }

    // Clean up instance
    if (vkInstance_ != VK_NULL_HANDLE) {
        vkDestroyInstance(vkInstance_, nullptr);
        vkInstance_ = VK_NULL_HANDLE;
    }

    // Close validation log file
    if (validationLogFile.is_open()) {
        validationLogFile.close();
    }
}

void VulkanContext::createInstance(const std::vector<const char*>& requiredExtensions) {
    std::lock_guard<std::mutex> lock(contextMutex_);

    if (!checkValidationLayerSupport()) {
        throw ValidationError("Validation layers requested, but not available!");
    }

    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "Vulkan Engine";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "No Engine";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_3;  // Use Vulkan 1.3 for better feature support

    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;

    // Get required extensions
    std::vector<const char*> extensions = requiredExtensions;
    extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

    // Add optional instance extensions
    std::vector<const char*> optionalInstanceExtensions = {
        VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME,
        VK_KHR_DEVICE_GROUP_CREATION_EXTENSION_NAME,
        VK_KHR_EXTERNAL_MEMORY_CAPABILITIES_EXTENSION_NAME,
        VK_KHR_EXTERNAL_SEMAPHORE_CAPABILITIES_EXTENSION_NAME
    };

    // Check which optional extensions are available
    uint32_t availableExtensionCount = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &availableExtensionCount, nullptr);
    std::vector<VkExtensionProperties> availableExtensions(availableExtensionCount);
    vkEnumerateInstanceExtensionProperties(nullptr, &availableExtensionCount, availableExtensions.data());

    std::unordered_map<std::string, VkExtensionProperties> availableExtensionsMap;
    for (const auto& extension : availableExtensions) {
        availableExtensionsMap[extension.extensionName] = extension;
    }

    for (const auto& optionalExtension : optionalInstanceExtensions) {
        if (availableExtensionsMap.find(optionalExtension) != availableExtensionsMap.end()) {
            extensions.push_back(optionalExtension);
        }
    }

    createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();

    // Enable validation layers
    createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers_.size());
    createInfo.ppEnabledLayerNames = validationLayers_.data();

    VkResult result = vkCreateInstance(&createInfo, nullptr, &vkInstance_);
    if (result != VK_SUCCESS) {
        throw VulkanError(result, "Failed to create Vulkan instance!");
    }
}

void VulkanContext::setupDebugMessenger() {
    initializeValidationLog();
    
    VkDebugUtilsMessengerCreateInfoEXT createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                                VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
                                VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                                VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                            VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                            VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    createInfo.pfnUserCallback = debugCallback;
    createInfo.pUserData = nullptr;

    // Add validation cache support
    VkValidationCacheCreateInfoEXT cacheCreateInfo{};
    cacheCreateInfo.sType = VK_STRUCTURE_TYPE_VALIDATION_CACHE_CREATE_INFO_EXT;
    
    // Try to load existing validation cache
    std::vector<uint8_t> cacheData;
    std::ifstream cacheFile("validation_cache.bin", std::ios::binary);
    if (cacheFile.is_open()) {
        cacheData = std::vector<uint8_t>(
            std::istreambuf_iterator<char>(cacheFile),
            std::istreambuf_iterator<char>()
        );
        cacheFile.close();
    }

    if (!cacheData.empty()) {
        cacheCreateInfo.initialDataSize = cacheData.size();
        cacheCreateInfo.pInitialData = cacheData.data();
    }

    createInfo.pNext = &cacheCreateInfo;

    auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(vkInstance_, "vkCreateDebugUtilsMessengerEXT");
    if (func == nullptr || func(vkInstance_, &createInfo, nullptr, &debugMessenger_) != VK_SUCCESS) {
        throw std::runtime_error("Failed to set up debug messenger!");
    }

    // Create validation cache after device creation
    if (device_ != VK_NULL_HANDLE) {
        auto createCacheFunc = (PFN_vkCreateValidationCacheEXT)vkGetDeviceProcAddr(device_, "vkCreateValidationCacheEXT");
        if (createCacheFunc != nullptr) {
            VkResult result = createCacheFunc(device_, &cacheCreateInfo, nullptr, &validationCache_);
            if (result != VK_SUCCESS) {
                std::cerr << "Warning: Failed to create validation cache: " << getVulkanResultString(result) << std::endl;
            }
        }
    }
}

void VulkanContext::pickPhysicalDevice() {
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(vkInstance_, &deviceCount, nullptr);

    if (deviceCount == 0) {
        throw std::runtime_error("Failed to find GPUs with Vulkan support!");
    }

    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(vkInstance_, &deviceCount, devices.data());

    for (const auto& device : devices) {
        if (isDeviceSuitable(device)) {
            physicalDevice_ = device;
            break;
        }
    }

    if (physicalDevice_ == VK_NULL_HANDLE) {
        throw std::runtime_error("Failed to find a suitable GPU!");
    }
}

void VulkanContext::createLogicalDevice() {
    QueueFamilyIndices indices = findQueueFamilies(physicalDevice_);

    // Verify queue family indices are valid
    if (!indices.isComplete()) {
        throw ValidationError("Failed to find all required queue families!");
    }

    // Verify queue capabilities
    VkQueueFamilyProperties queueFamilyProps;
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice_, indices.graphicsFamily.value(), &queueFamilyProps);
    if (!(queueFamilyProps.queueFlags & VK_QUEUE_GRAPHICS_BIT)) {
        throw ValidationError("Graphics queue family does not support graphics operations!");
    }

    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice_, indices.computeFamily.value(), &queueFamilyProps);
    if (!(queueFamilyProps.queueFlags & VK_QUEUE_COMPUTE_BIT)) {
        throw ValidationError("Compute queue family does not support compute operations!");
    }

    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    std::set<uint32_t> uniqueQueueFamilies = {
        indices.graphicsFamily.value(),
        indices.presentFamily.value(),
        indices.computeFamily.value()
    };

    float queuePriority = 1.0f;
    for (uint32_t queueFamily : uniqueQueueFamilies) {
        VkDeviceQueueCreateInfo queueCreateInfo{};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = queueFamily;
        queueCreateInfo.queueCount = 1;
        queueCreateInfo.pQueuePriorities = &queuePriority;
        queueCreateInfos.push_back(queueCreateInfo);
    }

    // Load device features from config
    VkPhysicalDeviceFeatures2 deviceFeatures2{};
    deviceFeatures2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    vkGetPhysicalDeviceFeatures2(physicalDevice_, &deviceFeatures2);

    VkDeviceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.pNext = &deviceFeatures2;
    createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
    createInfo.pQueueCreateInfos = queueCreateInfos.data();
    createInfo.pEnabledFeatures = &deviceFeatures2.features;

    // Add synchronization extensions
    std::vector<const char*> syncExtensions = deviceExtensions_;
    syncExtensions.push_back(VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME);
    syncExtensions.push_back(VK_KHR_TIMELINE_SEMAPHORE_EXTENSION_NAME);

    createInfo.enabledExtensionCount = static_cast<uint32_t>(syncExtensions.size());
    createInfo.ppEnabledExtensionNames = syncExtensions.data();

    VkResult result = vkCreateDevice(physicalDevice_, &createInfo, nullptr, &device_);
    if (result != VK_SUCCESS) {
        throw VulkanError(result, "Failed to create logical device!");
    }

    // Get queue handles with validation
    VkQueue tempQueue;
    vkGetDeviceQueue(device_, indices.graphicsFamily.value(), 0, &tempQueue);
    if (tempQueue == VK_NULL_HANDLE) {
        throw ValidationError("Failed to get graphics queue handle!");
    }
    graphicsQueue_ = tempQueue;

    vkGetDeviceQueue(device_, indices.presentFamily.value(), 0, &tempQueue);
    if (tempQueue == VK_NULL_HANDLE) {
        throw ValidationError("Failed to get present queue handle!");
    }
    presentQueue_ = tempQueue;

    vkGetDeviceQueue(device_, indices.computeFamily.value(), 0, &tempQueue);
    if (tempQueue == VK_NULL_HANDLE) {
        throw ValidationError("Failed to get compute queue handle!");
    }
    computeQueue_ = tempQueue;

    // Create timeline semaphores for synchronization
    VkSemaphoreTypeCreateInfo timelineCreateInfo{};
    timelineCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO;
    timelineCreateInfo.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
    timelineCreateInfo.initialValue = 0;

    VkSemaphoreCreateInfo semaphoreCreateInfo{};
    semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    semaphoreCreateInfo.pNext = &timelineCreateInfo;

    // Create graphics-compute synchronization semaphore
    result = vkCreateSemaphore(device_, &semaphoreCreateInfo, nullptr, &graphicsComputeSemaphore_);
    if (result != VK_SUCCESS) {
        throw VulkanError(result, "Failed to create graphics-compute synchronization semaphore!");
    }

    // Create compute-present synchronization semaphore
    result = vkCreateSemaphore(device_, &semaphoreCreateInfo, nullptr, &computePresentSemaphore_);
    if (result != VK_SUCCESS) {
        throw VulkanError(result, "Failed to create compute-present synchronization semaphore!");
    }
}

void VulkanContext::createSurface() {
    auto windowManager = WindowManager::getInstance();
    if (windowManager.createWindowSurface(vkInstance_, &surface_) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create window surface!");
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
            return false;
        }
    }

    return true;
}

QueueFamilyIndices VulkanContext::findQueueFamilies(VkPhysicalDevice device) const {
    QueueFamilyIndices indices;

    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

    int i = 0;
    for (const auto& queueFamily : queueFamilies) {
        if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            indices.graphicsFamily = i;
        }

        if (queueFamily.queueFlags & VK_QUEUE_COMPUTE_BIT) {
            indices.computeFamily = i;
        }

        VkBool32 presentSupport = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface_, &presentSupport);

        if (presentSupport) {
            indices.presentFamily = i;
        }

        if (indices.isComplete()) {
            break;
        }

        i++;
    }

    return indices;
}

SwapChainSupportDetails VulkanContext::querySwapChainSupport(VkPhysicalDevice device) const {
    SwapChainSupportDetails details;

    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface_, &details.capabilities);

    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface_, &formatCount, nullptr);

    if (formatCount != 0) {
        details.formats.resize(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface_, &formatCount, details.formats.data());
    }

    uint32_t presentModeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface_, &presentModeCount, nullptr);

    if (presentModeCount != 0) {
        details.presentModes.resize(presentModeCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface_, &presentModeCount, details.presentModes.data());
    }

    return details;
}

bool VulkanContext::isDeviceSuitable(VkPhysicalDevice device) const {
    QueueFamilyIndices indices = findQueueFamilies(device);

    bool extensionsSupported = checkDeviceExtensionSupport(device);

    bool swapChainAdequate = false;
    if (extensionsSupported) {
        SwapChainSupportDetails swapChainSupport = querySwapChainSupport(device);
        swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
    }

    VkPhysicalDeviceFeatures supportedFeatures;
    vkGetPhysicalDeviceFeatures(device, &supportedFeatures);

    return indices.isComplete() && extensionsSupported && swapChainAdequate && supportedFeatures.samplerAnisotropy;
}

bool VulkanContext::checkDeviceExtensionSupport(VkPhysicalDevice device) const {
    uint32_t extensionCount;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

    std::vector<VkExtensionProperties> availableExtensions(extensionCount);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

    // Create a map of available extensions for faster lookup
    std::unordered_map<std::string, VkExtensionProperties> availableExtensionsMap;
    for (const auto& extension : availableExtensions) {
        availableExtensionsMap[extension.extensionName] = extension;
    }

    // Check required extensions
    std::vector<std::string> missingExtensions;
    for (const auto& requiredExtension : deviceExtensions_) {
        if (availableExtensionsMap.find(requiredExtension) == availableExtensionsMap.end()) {
            missingExtensions.push_back(requiredExtension);
        }
    }

    // Check optional extensions and add them if available
    std::vector<const char*> optionalExtensions = {
        VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME,
        VK_KHR_TIMELINE_SEMAPHORE_EXTENSION_NAME,
        VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME,
        VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME,
        VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
        VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME
    };

    std::vector<const char*> enabledOptionalExtensions;
    for (const auto& optionalExtension : optionalExtensions) {
        if (availableExtensionsMap.find(optionalExtension) != availableExtensionsMap.end()) {
            enabledOptionalExtensions.push_back(optionalExtension);
        }
    }

    // If any required extensions are missing, throw an error
    if (!missingExtensions.empty()) {
        std::string errorMsg = "Device does not support required extensions:\n";
        for (const auto& ext : missingExtensions) {
            errorMsg += "  - " + ext + "\n";
        }
        throw ValidationError(errorMsg);
    }

    // Add enabled optional extensions to the device extensions list
    deviceExtensions_.insert(deviceExtensions_.end(), enabledOptionalExtensions.begin(), enabledOptionalExtensions.end());

    return true;
}

uint32_t VulkanContext::findMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }

    throw std::runtime_error("Failed to find suitable memory type!");
}

} // namespace VulkanHIP

// ... existing code ... 
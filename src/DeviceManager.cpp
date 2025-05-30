#include "DeviceManager.h"
#include <set>
#include <stdexcept>
#include <iostream> // For temporary logging, remove later
#include <cstring> // For strcmp

namespace VulkanHIP {

DeviceManager::DeviceManager(VkInstance instance, VkSurfaceKHR surface, const std::vector<const char*>& requiredDeviceExtensions)
    : instance_(instance),
      surface_(surface),
      requiredDeviceExtensions_(requiredDeviceExtensions) {
    if (instance_ == VK_NULL_HANDLE) {
        throw std::runtime_error("DeviceManager: Vulkan instance cannot be null.");
    }
    // Surface can be null if we are not doing graphics, but findQueueFamilies will need to handle it
}

DeviceManager::~DeviceManager() {
    if (device_ != VK_NULL_HANDLE) {
        vkDestroyDevice(device_, nullptr);
        device_ = VK_NULL_HANDLE;
    }
}

void DeviceManager::pickPhysicalDevice() {
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(instance_, &deviceCount, nullptr);

    if (deviceCount == 0) {
        throw std::runtime_error("Failed to find GPUs with Vulkan support!");
    }

    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(instance_, &deviceCount, devices.data());

    std::cout << "Found " << deviceCount << " Vulkan-capable device(s)." << std::endl;

    for (const auto& device : devices) {
        queryPhysicalDeviceProperties(device); // Store properties and features
        if (isDeviceSuitable(device)) {
            physicalDevice_ = device;
            std::cout << "Selected device: " << deviceProperties_.deviceName << std::endl;
            // Store supported features of the selected device
            vkGetPhysicalDeviceFeatures(physicalDevice_, &supportedFeatures_);
            break;
        }
    }

    if (physicalDevice_ == VK_NULL_HANDLE) {
        throw std::runtime_error("Failed to find a suitable GPU!");
    }
}

void DeviceManager::queryPhysicalDeviceProperties(VkPhysicalDevice device) {
    vkGetPhysicalDeviceProperties(device, &deviceProperties_);
    // We might query features here too if needed by isDeviceSuitable before selection
    // vkGetPhysicalDeviceFeatures(device, &supportedFeatures_); 
}

bool DeviceManager::isDeviceSuitable(VkPhysicalDevice device) const {
    QueueFamilyIndices indices = findQueueFamilies(device);
    bool extensionsSupported = checkDeviceExtensionSupport(device);

    bool swapChainAdequate = false;
    if (extensionsSupported && surface_ != VK_NULL_HANDLE) { // Only check swapchain if surface exists
        uint32_t formatCount;
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface_, &formatCount, nullptr);
        uint32_t presentModeCount;
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface_, &presentModeCount, nullptr);
        swapChainAdequate = formatCount > 0 && presentModeCount > 0;
    } else if (extensionsSupported && surface_ == VK_NULL_HANDLE) {
        // If no surface, swapchain adequacy is not a criterion for suitability (e.g. compute-only)
        swapChainAdequate = true; 
    }
    
    VkPhysicalDeviceFeatures deviceFeatures;
    vkGetPhysicalDeviceFeatures(device, &deviceFeatures); // Get features for current device check

    // Example: Check for a specific feature if needed
    // bool featuresSupported = deviceFeatures.samplerAnisotropy;

    return indices.isComplete() && extensionsSupported && swapChainAdequate; // && featuresSupported (if checking specific features)
}

QueueFamilyIndices DeviceManager::findQueueFamilies(VkPhysicalDevice device) const {
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

        if (surface_ != VK_NULL_HANDLE) { // Only check for present support if surface exists
            VkBool32 presentSupport = false;
            vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface_, &presentSupport);
            if (presentSupport) {
                indices.presentFamily = i;
            }
        }
        
        // If no surface, we might not need a present family. 
        // isComplete might need adjustment or usage context.
        if (surface_ != VK_NULL_HANDLE && indices.isComplete()) {
            if (indices.hasCompute() || !(queueFamily.queueFlags & VK_QUEUE_COMPUTE_BIT)) { // ensure compute is also found if sought
                 break;
            }
        } else if (surface_ == VK_NULL_HANDLE && indices.graphicsFamily.has_value() && indices.computeFamily.has_value()) {
             break; // For compute-only, graphics and compute are enough
        }
        i++;
    }
    return indices;
}

bool DeviceManager::checkDeviceExtensionSupport(VkPhysicalDevice device) const {
    uint32_t extensionCount;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

    std::vector<VkExtensionProperties> availableExtensions(extensionCount);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

    std::set<std::string> requiredExtensions(requiredDeviceExtensions_.begin(), requiredDeviceExtensions_.end());

    for (const auto& extension : availableExtensions) {
        requiredExtensions.erase(extension.extensionName);
    }
    return requiredExtensions.empty();
}

void DeviceManager::createLogicalDevice(const std::vector<const char*>& validationLayers, const VkPhysicalDeviceFeatures& enabledFeatures) {
    if (physicalDevice_ == VK_NULL_HANDLE) {
        throw std::runtime_error("Cannot create logical device: Physical device not picked.");
    }

    queueFamilyIndices_ = findQueueFamilies(physicalDevice_); 
    // Re-evaluate queue families for the chosen physical device
    // as the surface might influence this if it wasn't available during initial suitability check.

    if (surface_ != VK_NULL_HANDLE && !queueFamilyIndices_.isComplete()) {
        throw std::runtime_error("Failed to find required queue families on selected physical device!");
    } else if (surface_ == VK_NULL_HANDLE && !queueFamilyIndices_.graphicsFamily.has_value()) {
        // For compute-only, we at least need a graphics or compute queue.
        // This logic might need refinement based on whether compute-only is truly graphics-agnostic.
        if (!queueFamilyIndices_.computeFamily.has_value()) {
             throw std::runtime_error("Failed to find graphics or compute queue family for non-graphics setup!");
        }
    }

    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    std::set<uint32_t> uniqueQueueFamilies = queueFamilyIndices_.getUniqueFamilies();

    float queuePriority = 1.0f;
    for (uint32_t queueFamilyIndex : uniqueQueueFamilies) {
        VkDeviceQueueCreateInfo queueCreateInfo{};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = queueFamilyIndex;
        queueCreateInfo.queueCount = 1;
        queueCreateInfo.pQueuePriorities = &queuePriority;
        queueCreateInfos.push_back(queueCreateInfo);
    }

    VkDeviceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
    createInfo.pQueueCreateInfos = queueCreateInfos.data();
    createInfo.pEnabledFeatures = &enabledFeatures; // Use the passed-in enabled features
    createInfo.enabledExtensionCount = static_cast<uint32_t>(requiredDeviceExtensions_.size());
    createInfo.ppEnabledExtensionNames = requiredDeviceExtensions_.data();

    if (!validationLayers.empty()) {
        createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
        createInfo.ppEnabledLayerNames = validationLayers.data();
    } else {
        createInfo.enabledLayerCount = 0;
    }

    VkResult result = vkCreateDevice(physicalDevice_, &createInfo, nullptr, &device_);
    if (result != VK_SUCCESS) {
        throw std::runtime_error("Failed to create logical device! Error code: " + std::to_string(result));
    }

    if (queueFamilyIndices_.graphicsFamily.has_value()) {
        vkGetDeviceQueue(device_, queueFamilyIndices_.graphicsFamily.value(), 0, &graphicsQueue_);
    }
    if (surface_ != VK_NULL_HANDLE && queueFamilyIndices_.presentFamily.has_value()) {
        vkGetDeviceQueue(device_, queueFamilyIndices_.presentFamily.value(), 0, &presentQueue_);
    }
    if (queueFamilyIndices_.computeFamily.has_value()) {
        vkGetDeviceQueue(device_, queueFamilyIndices_.computeFamily.value(), 0, &computeQueue_);
    }
    std::cout << "Logical device created successfully." << std::endl;
}

} // namespace VulkanHIP
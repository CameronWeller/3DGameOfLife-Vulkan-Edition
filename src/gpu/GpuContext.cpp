#include "gpu/GpuContext.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

#include <GLFW/glfw3.h>

namespace gol3d {
namespace {

constexpr const char* VALIDATION_LAYER = "VK_LAYER_KHRONOS_validation";
// Portability drivers (MoltenVK on macOS) are only listed after opting in.
constexpr const char* PORTABILITY_ENUMERATION = "VK_KHR_portability_enumeration";
constexpr const char* PORTABILITY_SUBSET = "VK_KHR_portability_subset";
// The compute shaders sum and combine values across a subgroup.
constexpr VkSubgroupFeatureFlags SUBGROUP_OPS = VK_SUBGROUP_FEATURE_BASIC_BIT |
                                                VK_SUBGROUP_FEATURE_ARITHMETIC_BIT |
                                                VK_SUBGROUP_FEATURE_BALLOT_BIT;

// True when the environment variable is set to anything but "" or "0".
bool envFlag(const char* name) {
    const char* value = std::getenv(name);
    return value && *value && std::strcmp(value, "0") != 0;
}

VKAPI_ATTR VkBool32 VKAPI_CALL onValidationMessage(VkDebugUtilsMessageSeverityFlagBitsEXT severity,
                                                   VkDebugUtilsMessageTypeFlagsEXT,
                                                   const VkDebugUtilsMessengerCallbackDataEXT* data,
                                                   void*) {
    if (severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
        std::cerr << "Vulkan: " << data->pMessage << std::endl;
    }
    return VK_FALSE;
}

bool hasExtension(const std::vector<VkExtensionProperties>& list, const char* name) {
    return std::any_of(list.begin(), list.end(), [&](const VkExtensionProperties& extension) {
        return std::strcmp(extension.extensionName, name) == 0;
    });
}

std::vector<VkExtensionProperties> instanceExtensions() {
    uint32_t count = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &count, nullptr);
    std::vector<VkExtensionProperties> list(count);
    vkEnumerateInstanceExtensionProperties(nullptr, &count, list.data());
    return list;
}

std::vector<VkExtensionProperties> deviceExtensions(VkPhysicalDevice device) {
    uint32_t count = 0;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &count, nullptr);
    std::vector<VkExtensionProperties> list(count);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &count, list.data());
    return list;
}

bool validationLayerInstalled() {
    uint32_t count = 0;
    vkEnumerateInstanceLayerProperties(&count, nullptr);
    std::vector<VkLayerProperties> layers(count);
    vkEnumerateInstanceLayerProperties(&count, layers.data());
    return std::any_of(layers.begin(), layers.end(), [](const VkLayerProperties& layer) {
        return std::strcmp(layer.layerName, VALIDATION_LAYER) == 0;
    });
}

std::string versionString(uint32_t version) {
    return std::to_string(VK_API_VERSION_MAJOR(version)) + "." +
           std::to_string(VK_API_VERSION_MINOR(version));
}

// Discrete GPUs first, software renderers last.
int preference(VkPhysicalDeviceType type) {
    switch (type) {
        case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
            return 4;
        case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
            return 3;
        case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
            return 2;
        case VK_PHYSICAL_DEVICE_TYPE_CPU:
            return 1;
        default:
            return 0;
    }
}

// What the game needs to know about a GPU it could use.
struct Candidate {
    VkPhysicalDevice device = VK_NULL_HANDLE;
    VkPhysicalDeviceProperties properties{};
    uint32_t queueFamily = UINT32_MAX;
};

// Why `device` cannot run the game, or "" when it can (then `out` is filled).
std::string whyUnsuitable(VkPhysicalDevice device, VkSurfaceKHR surface, const char* wantedName,
                          Candidate& out) {
    VkPhysicalDeviceSubgroupProperties subgroup{
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES};
    VkPhysicalDeviceProperties2 properties{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
                                           &subgroup};
    vkGetPhysicalDeviceProperties2(device, &properties);
    const VkPhysicalDeviceProperties& deviceProperties = properties.properties;
    if (deviceProperties.apiVersion < VK_API_VERSION_1_3) {
        return "Vulkan " + versionString(deviceProperties.apiVersion) + " (needs 1.3)";
    }

    VkPhysicalDeviceVulkan13Features features13{
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES};
    VkPhysicalDeviceFeatures2 features{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2, &features13};
    vkGetPhysicalDeviceFeatures2(device, &features);
    if (!features13.dynamicRendering || !features13.synchronization2) {
        return "no dynamic rendering or synchronization2";
    }
    bool computeSubgroups = (subgroup.supportedStages & VK_SHADER_STAGE_COMPUTE_BIT) &&
                            (subgroup.supportedOperations & SUBGROUP_OPS) == SUBGROUP_OPS;
    if (!computeSubgroups) return "no compute subgroup arithmetic";
    if (!hasExtension(deviceExtensions(device), VK_KHR_SWAPCHAIN_EXTENSION_NAME)) {
        return "no swapchain";
    }

    // One queue family must draw, compute and present to the window.
    uint32_t familyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &familyCount, nullptr);
    std::vector<VkQueueFamilyProperties> families(familyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &familyCount, families.data());
    uint32_t family = UINT32_MAX;
    for (uint32_t i = 0; i < familyCount && family == UINT32_MAX; ++i) {
        VkBool32 presents = VK_FALSE;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presents);
        const VkQueueFlags needed = VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT;
        if (presents && (families[i].queueFlags & needed) == needed) family = i;
    }
    if (family == UINT32_MAX) return "no queue that draws, computes and presents to this window";

    if (wantedName && *wantedName && !std::strstr(deviceProperties.deviceName, wantedName)) {
        return "not selected by GOL3D_GPU";
    }
    out = Candidate{device, deviceProperties, family};
    return "";
}

} // namespace

void GpuContext::init(GLFWwindow* window) {
    createInstance();
    if (glfwCreateWindowSurface(instance, window, nullptr, &surface) != VK_SUCCESS) {
        throw std::runtime_error("Could not create a Vulkan surface for the window.");
    }
    choosePhysicalDevice();
    createDevice();
}

void GpuContext::createInstance() {
    uint32_t loaderVersion = VK_API_VERSION_1_0;
    if (vkEnumerateInstanceVersion) vkEnumerateInstanceVersion(&loaderVersion);
    if (loaderVersion < VK_API_VERSION_1_3) {
        throw std::runtime_error("The Vulkan loader on this system is version " +
                                 versionString(loaderVersion) +
                                 "; 3D Life needs Vulkan 1.3. Update your GPU driver.");
    }

    // The extensions GLFW needs to make a surface for windows on this platform.
    uint32_t glfwCount = 0;
    const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwCount);
    if (!glfwExtensions) {
        throw std::runtime_error("This system has no Vulkan surface support for windows.");
    }
    std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwCount);
    const std::vector<VkExtensionProperties> available = instanceExtensions();

    VkInstanceCreateInfo createInfo{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    if (hasExtension(available, PORTABILITY_ENUMERATION)) {
        extensions.push_back(PORTABILITY_ENUMERATION);
        createInfo.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
    }

    std::vector<const char*> layers;
    if (envFlag("GOL3D_VALIDATION")) {
        if (validationLayerInstalled() &&
            hasExtension(available, VK_EXT_DEBUG_UTILS_EXTENSION_NAME)) {
            layers.push_back(VALIDATION_LAYER);
            extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        } else {
            std::cerr << "GOL3D_VALIDATION is set but " << VALIDATION_LAYER << " is not installed."
                      << std::endl;
        }
    }

    VkApplicationInfo appInfo{VK_STRUCTURE_TYPE_APPLICATION_INFO};
    appInfo.pApplicationName = "3D Life";
    appInfo.pEngineName = "gol3d";
    appInfo.apiVersion = VK_API_VERSION_1_3;
    createInfo.pApplicationInfo = &appInfo;
    createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();
    createInfo.enabledLayerCount = static_cast<uint32_t>(layers.size());
    createInfo.ppEnabledLayerNames = layers.data();
    if (VkResult result = vkCreateInstance(&createInfo, nullptr, &instance); result != VK_SUCCESS) {
        throw std::runtime_error("Could not start Vulkan (error " + std::to_string(result) + ").");
    }
    volkLoadInstanceOnly(instance);

    if (!layers.empty()) {
        // Print validation warnings and errors.
        VkDebugUtilsMessengerCreateInfoEXT messengerInfo{
            VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT};
        messengerInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                                        VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        messengerInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                                    VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                                    VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        messengerInfo.pfnUserCallback = onValidationMessage;
        vkCreateDebugUtilsMessengerEXT(instance, &messengerInfo, nullptr, &messenger_);
    }
}

// Picks the most capable GPU that can run the game, or explains why none can.
void GpuContext::choosePhysicalDevice() {
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

    const char* wantedName = std::getenv("GOL3D_GPU");
    std::ostringstream rejected;
    int bestPreference = -1;
    for (VkPhysicalDevice device : devices) {
        Candidate candidate;
        std::string why = whyUnsuitable(device, surface, wantedName, candidate);
        if (!why.empty()) {
            VkPhysicalDeviceProperties deviceProperties;
            vkGetPhysicalDeviceProperties(device, &deviceProperties);
            rejected << "\n  " << deviceProperties.deviceName << ": " << why;
            continue;
        }
        if (preference(candidate.properties.deviceType) > bestPreference) {
            bestPreference = preference(candidate.properties.deviceType);
            physicalDevice = candidate.device;
            queueFamily = candidate.queueFamily;
            properties = candidate.properties;
        }
    }
    if (physicalDevice == VK_NULL_HANDLE) {
        std::string reasons =
            rejected.str().empty() ? "\n  (no Vulkan devices found)" : rejected.str();
        throw std::runtime_error(
            "No GPU can run 3D Life. It needs Vulkan 1.3; update your GPU driver." + reasons);
    }
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memoryProperties);
}

void GpuContext::createDevice() {
    float priority = 1.0f;
    VkDeviceQueueCreateInfo queueInfo{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
    queueInfo.queueFamilyIndex = queueFamily;
    queueInfo.queueCount = 1;
    queueInfo.pQueuePriorities = &priority;

    std::vector<const char*> extensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
    if (hasExtension(deviceExtensions(physicalDevice), PORTABILITY_SUBSET)) {
        extensions.push_back(PORTABILITY_SUBSET);
    }

    // Only the features the game uses: robust buffer access and friends cost
    // performance on some drivers and nothing here needs them.
    VkPhysicalDeviceVulkan13Features features13{
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES};
    features13.dynamicRendering = VK_TRUE;
    features13.synchronization2 = VK_TRUE;
    VkPhysicalDeviceFeatures2 features{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2, &features13};

    VkDeviceCreateInfo deviceInfo{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO, &features};
    deviceInfo.queueCreateInfoCount = 1;
    deviceInfo.pQueueCreateInfos = &queueInfo;
    deviceInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    deviceInfo.ppEnabledExtensionNames = extensions.data();
    if (VkResult result = vkCreateDevice(physicalDevice, &deviceInfo, nullptr, &device);
        result != VK_SUCCESS) {
        throw std::runtime_error("Could not open the GPU (error " + std::to_string(result) + ").");
    }
    volkLoadDevice(device);
    vkGetDeviceQueue(device, queueFamily, 0, &queue);

    VkCommandPoolCreateInfo poolInfo{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = queueFamily;
    if (vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool) != VK_SUCCESS) {
        throw std::runtime_error("Could not create a command pool.");
    }
}

void GpuContext::waitForFence(VkFence fence, const char* what) const {
    constexpr uint64_t TIMEOUT_NS = 4'000'000'000;
    VkResult result = vkWaitForFences(device, 1, &fence, VK_TRUE, TIMEOUT_NS);
    if (result == VK_TIMEOUT) {
        throw std::runtime_error(std::string("GPU did not finish ") + what + " within 4 s");
    }
    if (result != VK_SUCCESS) {
        throw std::runtime_error(std::string("Lost the GPU while waiting for ") + what);
    }
}

SurfaceSupport GpuContext::querySurface() const {
    SurfaceSupport support;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &support.capabilities);
    uint32_t count = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &count, nullptr);
    support.formats.resize(count);
    vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &count, support.formats.data());
    vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &count, nullptr);
    support.presentModes.resize(count);
    vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &count,
                                              support.presentModes.data());
    return support;
}

void GpuContext::destroy() {
    if (device) {
        vkDestroyCommandPool(device, commandPool, nullptr);
        vkDestroyDevice(device, nullptr);
        device = VK_NULL_HANDLE;
    }
    if (instance) {
        if (surface) vkDestroySurfaceKHR(instance, surface, nullptr);
        if (messenger_) vkDestroyDebugUtilsMessengerEXT(instance, messenger_, nullptr);
        vkDestroyInstance(instance, nullptr);
        instance = VK_NULL_HANDLE;
    }
    surface = VK_NULL_HANDLE;
    messenger_ = VK_NULL_HANDLE;
}

} // namespace gol3d

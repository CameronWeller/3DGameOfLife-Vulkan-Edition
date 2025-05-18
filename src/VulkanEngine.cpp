#include "VulkanEngine.h"
#include <iostream>
#include <set>
#include <fstream>
#include <glm/glm.hpp>
#include <algorithm>
#include <cstring>
#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>
#include <vector>
#include <string>
#include <stdexcept>
#include <optional>
#include <array>

// Initialize static member
VulkanEngine* VulkanEngine::instance = nullptr;

VulkanEngine::VulkanEngine() : vkInstance(VK_NULL_HANDLE),
                              physicalDevice(VK_NULL_HANDLE),
                              device(VK_NULL_HANDLE),
                              graphicsQueue(VK_NULL_HANDLE),
                              presentQueue(VK_NULL_HANDLE),
                              computeQueue(VK_NULL_HANDLE),
                              surface(VK_NULL_HANDLE),
                              pipelineLayout(VK_NULL_HANDLE),
                              graphicsPipeline(VK_NULL_HANDLE),
                              descriptorSetLayout(VK_NULL_HANDLE),
                              graphicsCommandPool(VK_NULL_HANDLE),
                              computeCommandPool(VK_NULL_HANDLE),
                              window(nullptr) {
    // Set static instance
    instance = this;
}

VulkanEngine::~VulkanEngine() {
    cleanup();
    // Clear static instance
    if (instance == this) {
        instance = nullptr;
    }
}

void VulkanEngine::init() {
    initWindow();
    createInstance();
    setupDebugMessenger();  // Add debug messenger setup
    createSurface();
    pickPhysicalDevice();
    createLogicalDevice();
    memoryPool = std::make_unique<MemoryPool>(device);  // Initialize memory pool
    createCommandPools();
    createDescriptorSetLayout();
    createCommandBuffers();
    createSyncObjects();  // Add synchronization objects
    createGraphicsPipeline();
}

void VulkanEngine::run() {
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        // TODO: Add optimized rendering code here
    }
}

void VulkanEngine::cleanup() {
    try {
        if (device != VK_NULL_HANDLE) {
            // Wait for device to finish operations before cleanup
            VkResult result = vkDeviceWaitIdle(device);
            if (result != VK_SUCCESS) {
                std::cerr << "Warning: vkDeviceWaitIdle failed with error code: " << result << std::endl;
                // Continue with cleanup even if wait fails
            }
        }
        
        // Clean up in reverse order of creation
        destroySyncObjects();
        
        if (memoryPool) {
            memoryPool.reset();
        }
        
        // Clean up pipeline resources
        if (graphicsPipeline != VK_NULL_HANDLE) {
            vkDestroyPipeline(device, graphicsPipeline, nullptr);
            graphicsPipeline = VK_NULL_HANDLE;
        }
        
        if (pipelineLayout != VK_NULL_HANDLE) {
            vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
            pipelineLayout = VK_NULL_HANDLE;
        }
        
        if (descriptorSetLayout != VK_NULL_HANDLE) {
            vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);
            descriptorSetLayout = VK_NULL_HANDLE;
        }
        
        // Clean up shader modules
        for (auto shaderModule : shaderModules) {
            if (shaderModule != VK_NULL_HANDLE) {
                vkDestroyShaderModule(device, shaderModule, nullptr);
            }
        }
        shaderModules.clear();
        
        // Clean up command pools
        if (graphicsCommandPool != VK_NULL_HANDLE) {
            vkDestroyCommandPool(device, graphicsCommandPool, nullptr);
            graphicsCommandPool = VK_NULL_HANDLE;
        }
        
        if (computeCommandPool != VK_NULL_HANDLE) {
            vkDestroyCommandPool(device, computeCommandPool, nullptr);
            computeCommandPool = VK_NULL_HANDLE;
        }
        
        // Clean up device
        if (device != VK_NULL_HANDLE) {
            vkDestroyDevice(device, nullptr);
            device = VK_NULL_HANDLE;
        }
        
        // Clean up surface
        cleanupSurface();
        
        // Clean up instance
        if (vkInstance != VK_NULL_HANDLE) {
            vkDestroyInstance(vkInstance, nullptr);
            vkInstance = VK_NULL_HANDLE;
        }
        
        // Clean up window
        if (window != nullptr) {
            glfwDestroyWindow(window);
            window = nullptr;
        }
        
        glfwTerminate();
        
        destroyDebugMessenger();
        
        std::cout << "Cleanup completed successfully" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Error during cleanup: " << e.what() << std::endl;
        // Don't re-throw here as we're already in cleanup
    }
}

void VulkanEngine::initWindow() {
    glfwInit();

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

    window = glfwCreateWindow(WIDTH, HEIGHT, WINDOW_TITLE, nullptr, nullptr);
    if (window == nullptr) {
        throw std::runtime_error("Failed to create GLFW window!");
    }

    glfwSetWindowUserPointer(window, this);
    glfwSetFramebufferSizeCallback(window, framebufferResizeCallback);
}

void VulkanEngine::createSurface() {
    if (glfwCreateWindowSurface(vkInstance, window, nullptr, &surface) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create window surface!");
    }
}

void VulkanEngine::cleanupSurface() {
    try {
        if (surface != VK_NULL_HANDLE) {
            vkDestroySurfaceKHR(vkInstance, surface, nullptr);
            surface = VK_NULL_HANDLE;
        }
    } catch (const std::exception& e) {
        std::cerr << "Error cleaning up surface: " << e.what() << std::endl;
    }
}

void VulkanEngine::framebufferResizeCallback(GLFWwindow* window, int width, int height) {
    auto app = reinterpret_cast<VulkanEngine*>(glfwGetWindowUserPointer(window));
    // TODO: Handle window resize
}

void VulkanEngine::createInstance() {
    if (!checkValidationLayerSupport()) {
        std::cout << "Validation layers requested but not available!" << std::endl;
        validationLayers.clear(); // Clear validation layers if not supported
    }

    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "Vulkan HIP Engine";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "CustomEngine";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_0;

    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;

    auto extensions = getRequiredExtensions();
    createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();

    if (!validationLayers.empty()) {
        createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
        createInfo.ppEnabledLayerNames = validationLayers.data();
    } else {
        createInfo.enabledLayerCount = 0;
    }

    if (vkCreateInstance(&createInfo, nullptr, &vkInstance) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create Vulkan instance!");
    }

    // Setup debug messenger if validation layers are enabled
    if (!validationLayers.empty()) {
        setupDebugMessenger();
    }
}

void VulkanEngine::pickPhysicalDevice() {
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(vkInstance, &deviceCount, nullptr);

    if (deviceCount == 0) {
        throw std::runtime_error("Failed to find GPUs with Vulkan support!");
    }

    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(vkInstance, &deviceCount, devices.data());

    for (const auto& device : devices) {
        if (isDeviceSuitable(device)) {
            physicalDevice = device;
            break;
        }
    }

    if (physicalDevice == VK_NULL_HANDLE) {
        throw std::runtime_error("Failed to find a suitable GPU!");
    }
}

QueueFamilyIndices VulkanEngine::findQueueFamilies(VkPhysicalDevice device) {
    QueueFamilyIndices indices;

    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

    // Find queue families that support graphics and presentation
    for (uint32_t i = 0; i < queueFamilyCount; i++) {
        if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            indices.graphicsFamily = i;
        }

        VkBool32 presentSupport = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);
        if (presentSupport) {
            indices.presentFamily = i;
        }

        if (indices.isComplete()) {
            break;
        }
    }

    return indices;
}

void VulkanEngine::createLogicalDevice() {
    try {
        queueFamilyIndices = findQueueFamilies(physicalDevice);
        if (!queueFamilyIndices.isComplete()) {
            throw std::runtime_error("Failed to find required queue families!");
        }

        std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
        std::set<uint32_t> uniqueQueueFamilies = {
            queueFamilyIndices.graphicsFamily.value(),
            queueFamilyIndices.presentFamily.value()
        };

        // Only add compute queue if it's available
        if (queueFamilyIndices.computeFamily.has_value()) {
            uniqueQueueFamilies.insert(queueFamilyIndices.computeFamily.value());
        }

        float queuePriority = 1.0f;
        for (uint32_t queueFamily : uniqueQueueFamilies) {
            VkDeviceQueueCreateInfo queueCreateInfo{};
            queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queueCreateInfo.queueFamilyIndex = queueFamily;
            queueCreateInfo.queueCount = 1;
            queueCreateInfo.pQueuePriorities = &queuePriority;
            queueCreateInfos.push_back(queueCreateInfo);
        }

        enableDeviceFeatures();

        VkDeviceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
        createInfo.pQueueCreateInfos = queueCreateInfos.data();
        createInfo.pEnabledFeatures = &deviceInfo.features;
        createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
        createInfo.ppEnabledExtensionNames = deviceExtensions.data();

        // Enable validation layers for device if they're enabled
        if (!validationLayers.empty()) {
            createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
            createInfo.ppEnabledLayerNames = validationLayers.data();
        } else {
            createInfo.enabledLayerCount = 0;
        }

        VkResult result = vkCreateDevice(physicalDevice, &createInfo, nullptr, &device);
        if (result != VK_SUCCESS) {
            throw std::runtime_error("Failed to create logical device! Error code: " + std::to_string(result));
        }

        // Get queue handles with error checking
        VkQueue tempQueue;
        vkGetDeviceQueue(device, queueFamilyIndices.graphicsFamily.value(), 0, &tempQueue);
        graphicsQueue = tempQueue;

        vkGetDeviceQueue(device, queueFamilyIndices.presentFamily.value(), 0, &tempQueue);
        presentQueue = tempQueue;
        
        // Only get compute queue if it's available
        if (queueFamilyIndices.computeFamily.has_value()) {
            vkGetDeviceQueue(device, queueFamilyIndices.computeFamily.value(), 0, &tempQueue);
            computeQueue = tempQueue;
        }

        std::cout << "Logical device created successfully!" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Error creating logical device: " << e.what() << std::endl;
        cleanup(); // Clean up any partially created resources
        throw; // Re-throw to be handled by the caller
    }
}

void VulkanEngine::enableDeviceFeatures() {
    // Enable performance-critical device features
    deviceInfo.features.samplerAnisotropy = VK_TRUE;
    deviceInfo.features.fillModeNonSolid = VK_TRUE;
    deviceInfo.features.wideLines = VK_TRUE;
    deviceInfo.features.multiDrawIndirect = VK_TRUE;
    deviceInfo.features.drawIndirectFirstInstance = VK_TRUE;
    deviceInfo.features.shaderStorageBufferArrayDynamicIndexing = VK_TRUE;
    deviceInfo.features.shaderUniformBufferArrayDynamicIndexing = VK_TRUE;
    deviceInfo.features.shaderSampledImageArrayDynamicIndexing = VK_TRUE;
    deviceInfo.features.shaderStorageImageArrayDynamicIndexing = VK_TRUE;
    deviceInfo.features.shaderClipDistance = VK_TRUE;
    deviceInfo.features.shaderCullDistance = VK_TRUE;
    deviceInfo.features.shaderFloat64 = VK_TRUE;
    deviceInfo.features.shaderInt64 = VK_TRUE;
    deviceInfo.features.shaderInt16 = VK_TRUE;
    deviceInfo.features.shaderResourceResidency = VK_TRUE;
    deviceInfo.features.shaderResourceMinLod = VK_TRUE;
    deviceInfo.features.sparseBinding = VK_TRUE;
    deviceInfo.features.sparseResidencyBuffer = VK_TRUE;
    deviceInfo.features.sparseResidencyImage2D = VK_TRUE;
    deviceInfo.features.sparseResidencyImage3D = VK_TRUE;
    deviceInfo.features.sparseResidency2Samples = VK_TRUE;
    deviceInfo.features.sparseResidency4Samples = VK_TRUE;
    deviceInfo.features.sparseResidency8Samples = VK_TRUE;
    deviceInfo.features.sparseResidency16Samples = VK_TRUE;
    deviceInfo.features.sparseResidencyAliased = VK_TRUE;
    deviceInfo.features.variableMultisampleRate = VK_TRUE;
    deviceInfo.features.inheritedQueries = VK_TRUE;
}

void VulkanEngine::createCommandPools() {
    // Create graphics command pool
    VkCommandPoolCreateInfo graphicsPoolInfo{};
    graphicsPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    graphicsPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    graphicsPoolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value();

    if (vkCreateCommandPool(device, &graphicsPoolInfo, nullptr, &graphicsCommandPool) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create graphics command pool!");
    }

    // Create compute command pool
    VkCommandPoolCreateInfo computePoolInfo{};
    computePoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    computePoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    computePoolInfo.queueFamilyIndex = queueFamilyIndices.computeFamily.value();

    if (vkCreateCommandPool(device, &computePoolInfo, nullptr, &computeCommandPool) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create compute command pool!");
    }
}

void VulkanEngine::createCommandBuffers() {
    // Create graphics command buffers
    graphicsCommandBuffers.resize(2); // Double buffering for better performance
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = graphicsCommandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = static_cast<uint32_t>(graphicsCommandBuffers.size());

    if (vkAllocateCommandBuffers(device, &allocInfo, graphicsCommandBuffers.data()) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate graphics command buffers!");
    }

    // Create compute command buffers
    computeCommandBuffers.resize(2); // Double buffering for compute operations
    allocInfo.commandPool = computeCommandPool;
    allocInfo.commandBufferCount = static_cast<uint32_t>(computeCommandBuffers.size());

    if (vkAllocateCommandBuffers(device, &allocInfo, computeCommandBuffers.data()) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate compute command buffers!");
    }
}

void VulkanEngine::recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex) {
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
        throw std::runtime_error("Failed to begin recording command buffer!");
    }

    // TODO: Add optimized command recording here

    if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
        throw std::runtime_error("Failed to record command buffer!");
    }
}

void VulkanEngine::createShaderStages(const std::string& vertPath, const std::string& fragPath,
                                    VkPipelineShaderStageCreateInfo& vertStageInfo,
                                    VkPipelineShaderStageCreateInfo& fragStageInfo) {
    auto vertShaderCode = readFile(vertPath);
    auto fragShaderCode = readFile(fragPath);

    VkShaderModule vertShaderModule = createShaderModule(vertShaderCode);
    VkShaderModule fragShaderModule = createShaderModule(fragShaderCode);

    vertStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertStageInfo.module = vertShaderModule;
    vertStageInfo.pName = "main";

    fragStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragStageInfo.module = fragShaderModule;
    fragStageInfo.pName = "main";

    // Store shader modules for cleanup
    shaderModules.push_back(vertShaderModule);
    shaderModules.push_back(fragShaderModule);
}

void VulkanEngine::createGraphicsPipeline() {
    VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
    VkPipelineShaderStageCreateInfo fragShaderStageInfo{};

    createShaderStages("shaders/basic.vert.spv", "shaders/basic.frag.spv",
                      vertShaderStageInfo, fragShaderStageInfo);

    VkPipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo, fragShaderStageInfo};

    // TODO: Add pipeline creation code here
    // This will be implemented in the next step
}

bool VulkanEngine::checkValidationLayerSupport() {
    uint32_t layerCount;
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

    std::vector<VkLayerProperties> availableLayers(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

    for (const char* layerName : validationLayers) {
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

std::vector<const char*> VulkanEngine::getRequiredExtensions() {
    uint32_t glfwExtensionCount = 0;
    const char** glfwExtensions;
    glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

    std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

    if (checkValidationLayerSupport()) {
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }

    return extensions;
}

bool VulkanEngine::isDeviceSuitable(VkPhysicalDevice device) {
    QueueFamilyIndices indices = findQueueFamilies(device);
    bool extensionsSupported = checkDeviceExtensionSupport(device);

    bool swapChainAdequate = false;
    if (extensionsSupported) {
        // TODO: Add swap chain support check
        swapChainAdequate = true;
    }

    return indices.isComplete() && extensionsSupported && swapChainAdequate;
}

bool VulkanEngine::checkDeviceExtensionSupport(VkPhysicalDevice device) {
    uint32_t extensionCount;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

    std::vector<VkExtensionProperties> availableExtensions(extensionCount);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

    std::set<std::string> requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());

    for (const auto& extension : availableExtensions) {
        requiredExtensions.erase(extension.extensionName);
    }

    return requiredExtensions.empty();
}

VkShaderModule VulkanEngine::createShaderModule(const std::vector<char>& code) {
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = code.size();
    createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

    VkShaderModule shaderModule;
    if (vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create shader module!");
    }

    return shaderModule;
}

std::vector<char> VulkanEngine::readFile(const std::string& filename) {
    std::ifstream file(filename, std::ios::ate | std::ios::binary);

    if (!file.is_open()) {
        throw std::runtime_error("Failed to open shader file: " + filename);
    }

    size_t fileSize = (size_t)file.tellg();
    if (fileSize == 0) {
        throw std::runtime_error("Shader file is empty: " + filename);
    }

    std::vector<char> buffer(fileSize);
    file.seekg(0);
    file.read(buffer.data(), fileSize);
    file.close();

    return buffer;
}

VkPipeline VulkanEngine::createComputePipeline(const std::string& shaderPath) {
    auto shaderCode = readFile(shaderPath);
    VkShaderModule computeShaderModule = createShaderModule(shaderCode);
    
    // Store shader module for cleanup
    shaderModules.push_back(computeShaderModule);

    VkPipelineShaderStageCreateInfo shaderStageInfo{};
    shaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    shaderStageInfo.module = computeShaderModule;
    shaderStageInfo.pName = "main";

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &descriptorSetLayout;
    
    // Add push constant range for grid dimensions
    VkPushConstantRange pushConstantRange{};
    pushConstantRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    pushConstantRange.offset = 0;
    pushConstantRange.size = sizeof(uint32_t) * 3; // width, height, depth
    
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;

    VkResult result = vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipelineLayout);
    if (result != VK_SUCCESS) {
        throw std::runtime_error("Failed to create compute pipeline layout!");
    }

    VkComputePipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineInfo.stage = shaderStageInfo;
    pipelineInfo.layout = pipelineLayout;

    VkPipeline computePipeline;
    result = vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &computePipeline);
    if (result != VK_SUCCESS) {
        vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
        throw std::runtime_error("Failed to create compute pipeline!");
    }

    return computePipeline;
}

void VulkanEngine::destroyComputePipeline(VkPipeline pipeline) {
    if (pipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(device, pipeline, nullptr);
    }
    if (pipelineLayout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
    }
}

uint32_t VulkanEngine::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) && 
            (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }

    throw std::runtime_error("Failed to find suitable memory type!");
}

VkBuffer VulkanEngine::createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties) {
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VkBuffer buffer;
    if (vkCreateBuffer(device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create buffer!");
    }

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(device, buffer, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);

    VkDeviceMemory bufferMemory;
    if (vkAllocateMemory(device, &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate buffer memory!");
    }

    vkBindBufferMemory(device, buffer, bufferMemory, 0);
    return buffer;
}

void VulkanEngine::destroyBuffer(VkBuffer buffer, VkDeviceMemory memory) {
    if (buffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(device, buffer, nullptr);
    }
    if (memory != VK_NULL_HANDLE) {
        vkFreeMemory(device, memory, nullptr);
    }
}

VkCommandBuffer VulkanEngine::beginSingleTimeCommands() {
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = graphicsCommandPool;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(commandBuffer, &beginInfo);
    return commandBuffer;
}

void VulkanEngine::endSingleTimeCommands(VkCommandBuffer commandBuffer) {
    vkEndCommandBuffer(commandBuffer);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(graphicsQueue);

    vkFreeCommandBuffers(device, graphicsCommandPool, 1, &commandBuffer);
}

void VulkanEngine::copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size) {
    VkCommandBuffer commandBuffer = beginSingleTimeCommands();

    VkBufferCopy copyRegion{};
    copyRegion.size = size;
    vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

    endSingleTimeCommands(commandBuffer);
}

void VulkanEngine::createDescriptorSetLayout() {
    // Create descriptor set layout for compute shader
    VkDescriptorSetLayoutBinding computeBinding{};
    computeBinding.binding = 0;
    computeBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    computeBinding.descriptorCount = 1;
    computeBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    computeBinding.pImmutableSamplers = nullptr;

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 1;
    layoutInfo.pBindings = &computeBinding;

    if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &descriptorSetLayout) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create descriptor set layout!");
    }
}

// Add MemoryPool implementation
MemoryPool::MemoryPool(VkDevice device) : device(device), maxStagingSize(0) {}

MemoryPool::~MemoryPool() {
    for (const auto& allocation : bufferPool) {
        vkDestroyBuffer(device, allocation.buffer, nullptr);
        vkFreeMemory(device, allocation.memory, nullptr);
    }
    for (const auto& staging : stagingPool) {
        vkDestroyBuffer(device, staging.buffer, nullptr);
        vkFreeMemory(device, staging.memory, nullptr);
    }
}

MemoryPool::BufferAllocation MemoryPool::allocateBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties) {
    // Try to find an existing buffer that matches requirements
    for (auto& allocation : bufferPool) {
        if (!allocation.inUse && allocation.size >= size && 
            allocation.usage == usage && allocation.properties == properties) {
            allocation.inUse = true;
            return allocation;
        }
    }

    // Create new buffer if no suitable one found
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VkBuffer buffer;
    if (vkCreateBuffer(device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create buffer!");
    }

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(device, buffer, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);

    VkDeviceMemory memory;
    if (vkAllocateMemory(device, &allocInfo, nullptr, &memory) != VK_SUCCESS) {
        vkDestroyBuffer(device, buffer, nullptr);
        throw std::runtime_error("Failed to allocate buffer memory!");
    }

    vkBindBufferMemory(device, buffer, memory, 0);

    BufferAllocation allocation{buffer, memory, size, usage, properties, true};
    bufferPool.push_back(allocation);
    return allocation;
}

void MemoryPool::freeBuffer(const BufferAllocation& allocation) {
    auto it = std::find_if(bufferPool.begin(), bufferPool.end(),
        [&](const BufferAllocation& a) {
            return a.buffer == allocation.buffer && a.memory == allocation.memory;
        });
    if (it != bufferPool.end()) {
        it->inUse = false;
    }
}

MemoryPool::StagingBuffer MemoryPool::getStagingBuffer(VkDeviceSize size) {
    // Try to find an existing staging buffer
    for (auto& staging : stagingPool) {
        if (!staging.inUse && staging.size >= size) {
            staging.inUse = true;
            return staging;
        }
    }

    // Create new staging buffer if needed
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VkBuffer buffer;
    if (vkCreateBuffer(device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create staging buffer!");
    }

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(device, buffer, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(
        memRequirements.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    );

    VkDeviceMemory memory;
    if (vkAllocateMemory(device, &allocInfo, nullptr, &memory) != VK_SUCCESS) {
        vkDestroyBuffer(device, buffer, nullptr);
        throw std::runtime_error("Failed to allocate staging memory!");
    }

    vkBindBufferMemory(device, buffer, memory, 0);

    StagingBuffer staging{buffer, memory, size, true};
    stagingPool.push_back(staging);
    maxStagingSize = std::max(maxStagingSize, size);
    return staging;
}

void MemoryPool::returnStagingBuffer(const StagingBuffer& buffer) {
    auto it = std::find_if(stagingPool.begin(), stagingPool.end(),
        [&](const StagingBuffer& s) {
            return s.buffer == buffer.buffer && s.memory == buffer.memory;
        });
    if (it != stagingPool.end()) {
        it->inUse = false;
    }
}

// Add validation layer setup
void VulkanEngine::setupDebugMessenger() {
    if (!checkValidationLayerSupport()) return;

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

    auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(vkInstance, "vkCreateDebugUtilsMessengerEXT");
    if (func != nullptr) {
        if (func(vkInstance, &createInfo, nullptr, &debugMessenger) != VK_SUCCESS) {
            throw std::runtime_error("Failed to set up debug messenger!");
        }
    } else {
        throw std::runtime_error("Failed to load vkCreateDebugUtilsMessengerEXT!");
    }
}

void VulkanEngine::destroyDebugMessenger() {
    if (!checkValidationLayerSupport()) return;

    auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(vkInstance, "vkDestroyDebugUtilsMessengerEXT");
    if (func != nullptr) {
        func(vkInstance, debugMessenger, nullptr);
    }
}

VKAPI_ATTR VkBool32 VKAPI_CALL VulkanEngine::debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData) {
    
    std::cerr << "Validation layer: " << pCallbackData->pMessage << std::endl;
    return VK_FALSE;
}

// Add synchronization objects
void VulkanEngine::createSyncObjects() {
    computeSemaphores.resize(2);
    computeFences.resize(2);

    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (size_t i = 0; i < 2; i++) {
        if (vkCreateSemaphore(device, &semaphoreInfo, nullptr, &computeSemaphores[i]) != VK_SUCCESS ||
            vkCreateFence(device, &fenceInfo, nullptr, &computeFences[i]) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create compute synchronization objects!");
        }
    }
}

void VulkanEngine::destroySyncObjects() {
    try {
        for (size_t i = 0; i < computeSemaphores.size(); i++) {
            if (computeSemaphores[i] != VK_NULL_HANDLE) {
                vkDestroySemaphore(device, computeSemaphores[i], nullptr);
                computeSemaphores[i] = VK_NULL_HANDLE;
            }
            if (computeFences[i] != VK_NULL_HANDLE) {
                vkDestroyFence(device, computeFences[i], nullptr);
                computeFences[i] = VK_NULL_HANDLE;
            }
        }
        computeSemaphores.clear();
        computeFences.clear();
    } catch (const std::exception& e) {
        std::cerr << "Error destroying sync objects: " << e.what() << std::endl;
    }
}

void VulkanEngine::waitForComputeCompletion() {
    vkQueueWaitIdle(computeQueue);
}

// Add findMemoryType implementation for MemoryPool
uint32_t MemoryPool::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(VulkanEngine::getPhysicalDevice(), &memProperties);

    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) && 
            (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }

    throw std::runtime_error("Failed to find suitable memory type!");
} 
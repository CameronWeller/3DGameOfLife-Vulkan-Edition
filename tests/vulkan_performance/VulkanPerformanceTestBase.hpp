#pragma once

#include <vulkan/vulkan.h>
#include <gtest/gtest.h>
#include <benchmark/benchmark.h>
#include <memory>
#include <vector>
#include <string>
#include <chrono>
#include <functional>
#include <optional>

class VulkanPerformanceTestBase : public ::testing::Test {
public:
    void SetUp() override {
        // Initialize Vulkan instance
        VkApplicationInfo appInfo{};
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pApplicationName = "Vulkan Performance Test";
        appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.pEngineName = "No Engine";
        appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.apiVersion = VK_API_VERSION_1_0;

        VkInstanceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        createInfo.pApplicationInfo = &appInfo;

        // Enable validation layers in debug mode
        #ifdef _DEBUG
        const std::vector<const char*> validationLayers = {
            "VK_LAYER_KHRONOS_validation"
        };
        createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
        createInfo.ppEnabledLayerNames = validationLayers.data();
        #endif

        if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create Vulkan instance");
        }

        // Get physical device
        uint32_t deviceCount = 0;
        vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
        if (deviceCount == 0) {
            throw std::runtime_error("Failed to find GPUs with Vulkan support");
        }

        std::vector<VkPhysicalDevice> devices(deviceCount);
        vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

        // Select the first device for now
        physicalDevice = devices[0];

        // Find queue family that supports graphics operations
        uint32_t queueFamilyIndex = findQueueFamilies(physicalDevice);

        // Create logical device
        float queuePriority = 1.0f;
        VkDeviceQueueCreateInfo queueCreateInfo{};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = queueFamilyIndex;
        queueCreateInfo.queueCount = 1;
        queueCreateInfo.pQueuePriorities = &queuePriority;

        VkDeviceCreateInfo deviceCreateInfo{};
        deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        deviceCreateInfo.pQueueCreateInfos = &queueCreateInfo;
        deviceCreateInfo.queueCreateInfoCount = 1;

        if (vkCreateDevice(physicalDevice, &deviceCreateInfo, nullptr, &device) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create logical device");
        }

        // Get the queue handle
        vkGetDeviceQueue(device, queueFamilyIndex, 0, &queue);
        graphicsQueueFamily = queueFamilyIndex;
    }

    void TearDown() override {
        if (device != VK_NULL_HANDLE) {
            vkDestroyDevice(device, nullptr);
        }
        if (instance != VK_NULL_HANDLE) {
            vkDestroyInstance(instance, nullptr);
        }
    }

    // Helper function to find suitable queue families
    uint32_t findQueueFamilies(VkPhysicalDevice device) {
        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

        std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

        // Find a queue family that supports graphics operations
        for (uint32_t i = 0; i < queueFamilies.size(); i++) {
            if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                return i;
            }
        }

        throw std::runtime_error("Failed to find suitable queue family");
    }

    // Helper function to find memory type
    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) {
        VkPhysicalDeviceMemoryProperties memProperties;
        vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

        for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
            if ((typeFilter & (1 << i)) && 
                (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
                return i;
            }
        }

        throw std::runtime_error("Failed to find suitable memory type");
    }

    // Helper functions for performance testing
    void recordCommandBuffer(VkCommandBuffer commandBuffer, const std::function<void(VkCommandBuffer)>& recordFunc) {
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        vkBeginCommandBuffer(commandBuffer, &beginInfo);
        recordFunc(commandBuffer);
        vkEndCommandBuffer(commandBuffer);
    }

    void submitCommandBuffer(VkCommandBuffer commandBuffer) {
        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffer;

        vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE);
        vkQueueWaitIdle(queue);
    }

    // Performance measurement functions
    void measureExecutionTime(const std::string& operationName, std::function<void()> operation) {
        auto start = std::chrono::steady_clock::now();
        operation();
        auto end = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        std::cout << operationName << " took " << duration.count() << " microseconds" << std::endl;
    }

    void measureMemoryOperation(const std::string& operationName, std::function<void()> operation) {
        auto start = std::chrono::steady_clock::now();
        operation();
        auto end = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        std::cout << operationName << " took " << duration.count() << " microseconds" << std::endl;
    }

public:
    VkInstance instance = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    VkQueue queue = VK_NULL_HANDLE;
    uint32_t graphicsQueueFamily = 0;
}; 
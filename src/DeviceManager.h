#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include <optional>
#include <string>
#include "QueueFamilyIndices.h"

class DeviceManager {
public:
    DeviceManager(VkInstance instance, VkSurfaceKHR surface, const std::vector<const char*>& requiredDeviceExtensions);
    ~DeviceManager();

    DeviceManager(const DeviceManager&) = delete;
    DeviceManager& operator=(const DeviceManager&) = delete;

    void pickPhysicalDevice();
    void createLogicalDevice(const std::vector<const char*>& validationLayers, const VkPhysicalDeviceFeatures& enabledFeatures);

    VkPhysicalDevice getPhysicalDevice() const { return physicalDevice_; }
    VkDevice getDevice() const { return device_; }
    const QueueFamilyIndices& getQueueFamilyIndices() const { return queueFamilyIndices_; }
    VkQueue getGraphicsQueue() const { return graphicsQueue_; }
    VkQueue getPresentQueue() const { return presentQueue_; }
    VkQueue getComputeQueue() const { return computeQueue_; }

private:
    bool isDeviceSuitable(VkPhysicalDevice device) const;
    QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device) const;
    bool checkDeviceExtensionSupport(VkPhysicalDevice device) const;
    void queryPhysicalDeviceProperties(VkPhysicalDevice device);

    VkInstance instance_;
    VkSurfaceKHR surface_;
    VkPhysicalDevice physicalDevice_ = VK_NULL_HANDLE;
    VkDevice device_ = VK_NULL_HANDLE;
    
    QueueFamilyIndices queueFamilyIndices_;
    VkQueue graphicsQueue_ = VK_NULL_HANDLE;
    VkQueue presentQueue_ = VK_NULL_HANDLE;
    VkQueue computeQueue_ = VK_NULL_HANDLE;

    std::vector<const char*> requiredDeviceExtensions_;
    VkPhysicalDeviceFeatures supportedFeatures_;
    VkPhysicalDeviceProperties deviceProperties_;
}; 
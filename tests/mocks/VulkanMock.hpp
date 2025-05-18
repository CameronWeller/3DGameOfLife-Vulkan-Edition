#pragma once

#include <vulkan/vulkan.h>
#include <gmock/gmock.h>
#include <memory>

class VulkanMock {
public:
    MOCK_METHOD(VkResult, vkCreateInstance, (const VkInstanceCreateInfo*, const VkAllocationCallbacks*, VkInstance*));
    MOCK_METHOD(VkResult, vkCreateDevice, (VkPhysicalDevice, const VkDeviceCreateInfo*, const VkAllocationCallbacks*, VkDevice*));
    MOCK_METHOD(VkResult, vkCreateBuffer, (VkDevice, const VkBufferCreateInfo*, const VkAllocationCallbacks*, VkBuffer*));
    MOCK_METHOD(VkResult, vkAllocateMemory, (VkDevice, const VkMemoryAllocateInfo*, const VkAllocationCallbacks*, VkDeviceMemory*));
    MOCK_METHOD(VkResult, vkBindBufferMemory, (VkDevice, VkBuffer, VkDeviceMemory, VkDeviceSize));
    MOCK_METHOD(void, vkDestroyBuffer, (VkDevice, VkBuffer, const VkAllocationCallbacks*));
    MOCK_METHOD(void, vkFreeMemory, (VkDevice, VkDeviceMemory, const VkAllocationCallbacks*));
    MOCK_METHOD(void, vkGetBufferMemoryRequirements, (VkDevice, VkBuffer, VkMemoryRequirements*));
    MOCK_METHOD(VkResult, vkCreateShaderModule, (VkDevice, const VkShaderModuleCreateInfo*, const VkAllocationCallbacks*, VkShaderModule*));
    MOCK_METHOD(void, vkDestroyShaderModule, (VkDevice, VkShaderModule, const VkAllocationCallbacks*));
};

// Global mock instance
extern std::unique_ptr<VulkanMock> g_vulkanMock;

// Mock function implementations
#define VK_MOCK_FUNCTION(name, ...) \
    VkResult name(__VA_ARGS__) { \
        return g_vulkanMock->name(__VA_ARGS__); \
    }

// Example mock implementation
VK_MOCK_FUNCTION(vkCreateInstance, const VkInstanceCreateInfo* pCreateInfo, 
                 const VkAllocationCallbacks* pAllocator, VkInstance* pInstance) 
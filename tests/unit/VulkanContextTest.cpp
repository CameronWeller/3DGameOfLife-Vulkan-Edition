#include <gtest/gtest.h>
#include "VulkanContext.h"
#include "WindowManager.h"
#include <vector>
#include <GLFW/glfw3.h>

class VulkanContextTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize GLFW for window creation
        if (!glfwInit()) {
            throw std::runtime_error("Failed to initialize GLFW");
        }
        
        // Initialize window manager
        WindowManager::WindowConfig config;
        config.width = 800;
        config.height = 600;
        config.title = "Vulkan Test Window";
        WindowManager::getInstance().init(config);
    }

    void TearDown() override {
        // Cleanup window manager
        WindowManager::getInstance().cleanup();
        glfwTerminate();
    }
};

TEST_F(VulkanContextTest, InitializationTest) {
    auto& context = VulkanContext::getInstance();
    std::vector<const char*> requiredExtensions = {
        VK_KHR_SURFACE_EXTENSION_NAME,
        VK_KHR_WIN32_SURFACE_EXTENSION_NAME
    };
    
    // Test initialization
    EXPECT_NO_THROW(context.init(requiredExtensions));
    
    // Verify instance creation
    EXPECT_NE(context.getVkInstance(), VK_NULL_HANDLE);
    
    // Verify physical device selection
    EXPECT_NE(context.getPhysicalDevice(), VK_NULL_HANDLE);
    
    // Verify logical device creation
    EXPECT_NE(context.getDevice(), VK_NULL_HANDLE);
    
    // Verify queue creation
    EXPECT_NE(context.getGraphicsQueue(), VK_NULL_HANDLE);
    EXPECT_NE(context.getPresentQueue(), VK_NULL_HANDLE);
    EXPECT_NE(context.getComputeQueue(), VK_NULL_HANDLE);
    
    // Cleanup
    context.cleanup();
}

TEST_F(VulkanContextTest, DeviceCreationTest) {
    auto& context = VulkanContext::getInstance();
    std::vector<const char*> requiredExtensions = {
        VK_KHR_SURFACE_EXTENSION_NAME,
        VK_KHR_WIN32_SURFACE_EXTENSION_NAME
    };
    
    context.init(requiredExtensions);
    
    // Test device features
    VkPhysicalDeviceFeatures deviceFeatures;
    vkGetPhysicalDeviceFeatures(context.getPhysicalDevice(), &deviceFeatures);
    EXPECT_TRUE(deviceFeatures.samplerAnisotropy);
    
    // Test device properties
    VkPhysicalDeviceProperties deviceProperties;
    vkGetPhysicalDeviceProperties(context.getPhysicalDevice(), &deviceProperties);
    EXPECT_GT(deviceProperties.limits.maxImageDimension2D, 0);
    
    // Test device memory properties
    VkPhysicalDeviceMemoryProperties memoryProperties;
    vkGetPhysicalDeviceMemoryProperties(context.getPhysicalDevice(), &memoryProperties);
    EXPECT_GT(memoryProperties.memoryTypeCount, 0);
    
    context.cleanup();
}

TEST_F(VulkanContextTest, QueueFamilyTest) {
    auto& context = VulkanContext::getInstance();
    std::vector<const char*> requiredExtensions = {
        VK_KHR_SURFACE_EXTENSION_NAME,
        VK_KHR_WIN32_SURFACE_EXTENSION_NAME
    };
    
    context.init(requiredExtensions);
    
    // Get queue family indices
    QueueFamilyIndices indices = context.getQueueFamilyIndices();
    
    // Verify queue family indices
    EXPECT_TRUE(indices.isComplete());
    EXPECT_TRUE(indices.graphicsFamily.has_value());
    EXPECT_TRUE(indices.presentFamily.has_value());
    EXPECT_TRUE(indices.computeFamily.has_value());
    
    // Verify queue family properties
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(context.getPhysicalDevice(), &queueFamilyCount, nullptr);
    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(context.getPhysicalDevice(), &queueFamilyCount, queueFamilies.data());
    
    // Check graphics queue capabilities
    EXPECT_TRUE(queueFamilies[indices.graphicsFamily.value()].queueFlags & VK_QUEUE_GRAPHICS_BIT);
    
    // Check compute queue capabilities
    EXPECT_TRUE(queueFamilies[indices.computeFamily.value()].queueFlags & VK_QUEUE_COMPUTE_BIT);
    
    context.cleanup();
}

TEST_F(VulkanContextTest, ValidationLayerTest) {
    auto& context = VulkanContext::getInstance();
    
    // Test validation layer configuration
    ValidationLayerConfig config;
    config.enabled = true;
    config.layers = {"VK_LAYER_KHRONOS_validation"};
    config.errorMessages = true;
    config.warningMessages = true;
    
    context.setValidationLayerConfig(config);
    EXPECT_TRUE(context.areValidationLayersEnabled());
    
    // Test initialization with validation layers
    std::vector<const char*> requiredExtensions = {
        VK_KHR_SURFACE_EXTENSION_NAME,
        VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
        VK_EXT_DEBUG_UTILS_EXTENSION_NAME
    };
    
    EXPECT_NO_THROW(context.init(requiredExtensions));
    context.cleanup();
}

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
} 
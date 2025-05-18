#include <gtest/gtest.h>
#include "../src/VulkanEngine.h"
#include <memory>

class QueueFamilyTest : public ::testing::Test {
protected:
    void SetUp() override {
        engine = std::make_unique<VulkanEngine>();
        engine->initWindow(800, 600, "Queue Family Test Window");
        engine->initVulkan();
    }

    void TearDown() override {
        engine->cleanup();
    }

    std::unique_ptr<VulkanEngine> engine;
};

// Test queue family indices
TEST_F(QueueFamilyTest, QueueFamilyIndicesTest) {
    QueueFamilyIndices indices = engine->findQueueFamilies(engine->getPhysicalDevice());
    
    // Graphics queue family must exist
    EXPECT_TRUE(indices.graphicsFamily.has_value());
    
    // Present queue family must exist
    EXPECT_TRUE(indices.presentFamily.has_value());
    
    // Compute queue family might exist
    // Note: Some devices might not have a dedicated compute queue
    if (indices.computeFamily.has_value()) {
        EXPECT_NE(indices.computeFamily.value(), indices.graphicsFamily.value());
    }
}

// Test queue creation
TEST_F(QueueFamilyTest, QueueCreationTest) {
    QueueFamilyIndices indices = engine->findQueueFamilies(engine->getPhysicalDevice());
    
    // Test graphics queue
    EXPECT_NE(engine->getGraphicsQueue(), VK_NULL_HANDLE);
    
    // Test present queue
    EXPECT_NE(engine->getPresentQueue(), VK_NULL_HANDLE);
    
    // Test compute queue if available
    if (indices.computeFamily.has_value()) {
        EXPECT_NE(engine->getComputeQueue(), VK_NULL_HANDLE);
    }
}

// Test queue family properties
TEST_F(QueueFamilyTest, QueueFamilyPropertiesTest) {
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(engine->getPhysicalDevice(), &queueFamilyCount, nullptr);
    
    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(engine->getPhysicalDevice(), &queueFamilyCount, queueFamilies.data());
    
    // At least one queue family must exist
    EXPECT_GT(queueFamilyCount, 0);
    
    // Check if any queue family supports graphics operations
    bool hasGraphicsQueue = false;
    for (const auto& queueFamily : queueFamilies) {
        if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            hasGraphicsQueue = true;
            break;
        }
    }
    EXPECT_TRUE(hasGraphicsQueue);
}

// Test queue family presentation support
TEST_F(QueueFamilyTest, QueueFamilyPresentationSupportTest) {
    QueueFamilyIndices indices = engine->findQueueFamilies(engine->getPhysicalDevice());
    
    // Present queue family must exist
    EXPECT_TRUE(indices.presentFamily.has_value());
    
    // Check if the present queue family supports presentation
    VkBool32 presentSupport = false;
    vkGetPhysicalDeviceSurfaceSupportKHR(
        engine->getPhysicalDevice(),
        indices.presentFamily.value(),
        engine->getSurface(),
        &presentSupport
    );
    EXPECT_TRUE(presentSupport);
}

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
} 
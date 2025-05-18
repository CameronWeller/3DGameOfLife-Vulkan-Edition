#include <gtest/gtest.h>
#include "../src/VulkanEngine.h"
#include <memory>

class VulkanEngineTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup code that will be called before each test
    }

    void TearDown() override {
        // Cleanup code that will be called after each test
    }

    std::unique_ptr<VulkanEngine> engine;
};

// Test initialization
TEST_F(VulkanEngineTest, InitializationTest) {
    EXPECT_NO_THROW(engine = std::make_unique<VulkanEngine>());
    EXPECT_NE(engine, nullptr);
}

// Test window creation
TEST_F(VulkanEngineTest, WindowCreationTest) {
    engine = std::make_unique<VulkanEngine>();
    EXPECT_NO_THROW(engine->initWindow(800, 600, "Test Window"));
    EXPECT_NE(engine->getWindow(), nullptr);
}

// Test Vulkan instance creation
TEST_F(VulkanEngineTest, InstanceCreationTest) {
    engine = std::make_unique<VulkanEngine>();
    engine->initWindow(800, 600, "Test Window");
    EXPECT_NO_THROW(engine->initVulkan());
    EXPECT_NE(engine->getVkInstance(), VK_NULL_HANDLE);
}

// Test physical device selection
TEST_F(VulkanEngineTest, PhysicalDeviceSelectionTest) {
    engine = std::make_unique<VulkanEngine>();
    engine->initWindow(800, 600, "Test Window");
    engine->initVulkan();
    EXPECT_NE(engine->getPhysicalDevice(), VK_NULL_HANDLE);
}

// Test logical device creation
TEST_F(VulkanEngineTest, LogicalDeviceCreationTest) {
    engine = std::make_unique<VulkanEngine>();
    engine->initWindow(800, 600, "Test Window");
    engine->initVulkan();
    EXPECT_NE(engine->getDevice(), VK_NULL_HANDLE);
}

// Test queue creation
TEST_F(VulkanEngineTest, QueueCreationTest) {
    engine = std::make_unique<VulkanEngine>();
    engine->initWindow(800, 600, "Test Window");
    engine->initVulkan();
    EXPECT_NE(engine->getGraphicsQueue(), VK_NULL_HANDLE);
    EXPECT_NE(engine->getPresentQueue(), VK_NULL_HANDLE);
}

// Test swapchain creation
TEST_F(VulkanEngineTest, SwapchainCreationTest) {
    engine = std::make_unique<VulkanEngine>();
    engine->initWindow(800, 600, "Test Window");
    engine->initVulkan();
    EXPECT_NO_THROW(engine->createSwapChain());
}

// Test command pool creation
TEST_F(VulkanEngineTest, CommandPoolCreationTest) {
    engine = std::make_unique<VulkanEngine>();
    engine->initWindow(800, 600, "Test Window");
    engine->initVulkan();
    EXPECT_NO_THROW(engine->createCommandPools());
}

// Test descriptor set layout creation
TEST_F(VulkanEngineTest, DescriptorSetLayoutCreationTest) {
    engine = std::make_unique<VulkanEngine>();
    engine->initWindow(800, 600, "Test Window");
    engine->initVulkan();
    EXPECT_NO_THROW(engine->createDescriptorSetLayout());
}

// Test pipeline creation
TEST_F(VulkanEngineTest, PipelineCreationTest) {
    engine = std::make_unique<VulkanEngine>();
    engine->initWindow(800, 600, "Test Window");
    engine->initVulkan();
    engine->createSwapChain();
    engine->createDescriptorSetLayout();
    EXPECT_NO_THROW(engine->createGraphicsPipeline());
}

// Test cleanup
TEST_F(VulkanEngineTest, CleanupTest) {
    engine = std::make_unique<VulkanEngine>();
    engine->initWindow(800, 600, "Test Window");
    engine->initVulkan();
    EXPECT_NO_THROW(engine->cleanup());
}

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
} 
#include <gtest/gtest.h>
#include "../src/VulkanEngine.h"
#include "../src/VulkanContext.h"
#include <memory>

using namespace VulkanHIP;

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

// Test Vulkan context initialization
TEST_F(VulkanEngineTest, VulkanContextInitializationTest) {
    engine = std::make_unique<VulkanEngine>();
    EXPECT_NO_THROW(engine->init());
    EXPECT_NE(engine->getVulkanContext(), nullptr);
}

// Test device creation
TEST_F(VulkanEngineTest, DeviceCreationTest) {
    engine = std::make_unique<VulkanEngine>();
    engine->init();
    auto context = engine->getVulkanContext();
    EXPECT_NE(context->getDevice(), VK_NULL_HANDLE);
    EXPECT_NE(context->getPhysicalDevice(), VK_NULL_HANDLE);
}

// Test queue creation
TEST_F(VulkanEngineTest, QueueCreationTest) {
    engine = std::make_unique<VulkanEngine>();
    engine->init();
    auto context = engine->getVulkanContext();
    EXPECT_NE(context->getGraphicsQueue(), VK_NULL_HANDLE);
    EXPECT_NE(context->getPresentQueue(), VK_NULL_HANDLE);
    EXPECT_NE(context->getComputeQueue(), VK_NULL_HANDLE);
}

// Test memory pool creation
TEST_F(VulkanEngineTest, MemoryPoolCreationTest) {
    engine = std::make_unique<VulkanEngine>();
    engine->init();
    EXPECT_NE(engine->getMemoryPool(), nullptr);
}

// Test command pool creation
TEST_F(VulkanEngineTest, CommandPoolCreationTest) {
    engine = std::make_unique<VulkanEngine>();
    engine->init();
    EXPECT_NO_THROW(engine->createCommandPools());
}

// Test descriptor set layout creation
TEST_F(VulkanEngineTest, DescriptorSetLayoutCreationTest) {
    engine = std::make_unique<VulkanEngine>();
    engine->init();
    EXPECT_NO_THROW(engine->createDescriptorSetLayout());
}

// Test pipeline creation
TEST_F(VulkanEngineTest, PipelineCreationTest) {
    engine = std::make_unique<VulkanEngine>();
    engine->init();
    engine->createDescriptorSetLayout();
    EXPECT_NO_THROW(engine->createGraphicsPipeline());
}

// Test cleanup
TEST_F(VulkanEngineTest, CleanupTest) {
    engine = std::make_unique<VulkanEngine>();
    engine->init();
    EXPECT_NO_THROW(engine->cleanup());
}

// Test memory manager creation
TEST_F(VulkanEngineTest, MemoryManagerCreationTest) {
    engine = std::make_unique<VulkanEngine>();
    engine->init();
    EXPECT_NE(engine->getMemoryManager(), nullptr);
}

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
} 
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
TEST_F(VulkanEngineTest, Initialization) {
    // Create a VulkanEngine instance
    auto engine = std::make_unique<VulkanEngine>();
    
    // Test that the engine is not null
    ASSERT_NE(engine, nullptr);
    
    // Test that the engine is not initialized by default
    EXPECT_FALSE(engine->isInitialized());
}

// Test window creation
TEST_F(VulkanEngineTest, WindowCreation) {
    auto engine = std::make_unique<VulkanEngine>();
    
    // Test window creation with valid parameters
    EXPECT_TRUE(engine->createWindow(800, 600, "Test Window"));
    
    // Test window creation with invalid parameters
    EXPECT_FALSE(engine->createWindow(0, 0, "Invalid Window"));
}

// Test shader compilation
TEST_F(VulkanEngineTest, ShaderCompilation) {
    auto engine = std::make_unique<VulkanEngine>();
    
    // Test shader compilation with valid shader
    EXPECT_TRUE(engine->compileShader("shaders/basic.vert", VK_SHADER_STAGE_VERTEX_BIT));
    
    // Test shader compilation with invalid shader
    EXPECT_FALSE(engine->compileShader("nonexistent.vert", VK_SHADER_STAGE_VERTEX_BIT));
}

// Test memory allocation
TEST_F(VulkanEngineTest, MemoryAllocation) {
    auto engine = std::make_unique<VulkanEngine>();
    
    // Test buffer allocation
    VkBuffer buffer;
    VkDeviceMemory memory;
    EXPECT_TRUE(engine->allocateBuffer(1024, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, &buffer, &memory));
    
    // Test buffer deallocation
    EXPECT_TRUE(engine->freeBuffer(buffer, memory));
}

// Test command buffer management
TEST_F(VulkanEngineTest, CommandBufferManagement) {
    auto engine = std::make_unique<VulkanEngine>();
    
    // Test command buffer creation
    VkCommandBuffer cmdBuffer;
    EXPECT_TRUE(engine->createCommandBuffer(&cmdBuffer));
    
    // Test command buffer recording
    EXPECT_TRUE(engine->beginCommandBuffer(cmdBuffer));
    EXPECT_TRUE(engine->endCommandBuffer(cmdBuffer));
    
    // Test command buffer cleanup
    EXPECT_TRUE(engine->freeCommandBuffer(cmdBuffer));
}

// Test synchronization primitives
TEST_F(VulkanEngineTest, Synchronization) {
    auto engine = std::make_unique<VulkanEngine>();
    
    // Test semaphore creation
    VkSemaphore semaphore;
    EXPECT_TRUE(engine->createSemaphore(&semaphore));
    
    // Test fence creation
    VkFence fence;
    EXPECT_TRUE(engine->createFence(&fence));
    
    // Test cleanup
    EXPECT_TRUE(engine->destroySemaphore(semaphore));
    EXPECT_TRUE(engine->destroyFence(fence));
}

// Test error handling
TEST_F(VulkanEngineTest, ErrorHandling) {
    auto engine = std::make_unique<VulkanEngine>();
    
    // Test invalid initialization
    EXPECT_FALSE(engine->initialize(nullptr));
    
    // Test invalid resource creation
    VkBuffer invalidBuffer = VK_NULL_HANDLE;
    VkDeviceMemory invalidMemory = VK_NULL_HANDLE;
    EXPECT_FALSE(engine->allocateBuffer(0, 0, &invalidBuffer, &invalidMemory));
}

// Test cleanup
TEST_F(VulkanEngineTest, Cleanup) {
    auto engine = std::make_unique<VulkanEngine>();
    
    // Initialize engine
    EXPECT_TRUE(engine->initialize());
    
    // Test cleanup
    EXPECT_TRUE(engine->cleanup());
    
    // Test double cleanup
    EXPECT_FALSE(engine->cleanup());
}

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
} 
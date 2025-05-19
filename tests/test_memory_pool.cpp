#include <gtest/gtest.h>
#include "../src/VulkanEngine.h"
#include "../src/VulkanContext.h"
#include "../src/MemoryPool.h"

class MemoryPoolTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize VulkanEngine which will create VulkanContext and MemoryPool
        engine = std::make_unique<VulkanEngine>();
        engine->init();
        
        // Get the device and physical device from VulkanContext
        auto context = engine->getVulkanContext();
        device = context->getDevice();
        physicalDevice = context->getPhysicalDevice();
        
        // Create a new MemoryPool instance for testing
        pool = std::make_unique<MemoryPool>(device, physicalDevice);
    }

    void TearDown() override {
        pool.reset();
        engine.reset();
    }

    std::unique_ptr<VulkanEngine> engine;
    std::unique_ptr<MemoryPool> pool;
    VkDevice device = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
};

TEST_F(MemoryPoolTest, BufferAllocation) {
    // Test buffer allocation
    auto allocation = pool->allocateBuffer(1024, 
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
    
    EXPECT_NE(allocation.buffer, VK_NULL_HANDLE);
    EXPECT_NE(allocation.memory, VK_NULL_HANDLE);
    EXPECT_EQ(allocation.size, 1024);
    EXPECT_TRUE(allocation.inUse);
    
    // Test buffer reuse
    pool->freeBuffer(allocation);
    auto newAllocation = pool->allocateBuffer(1024,
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
    
    EXPECT_EQ(newAllocation.buffer, allocation.buffer);
}

TEST_F(MemoryPoolTest, StagingBuffer) {
    // Test staging buffer allocation
    auto staging = pool->getStagingBuffer(2048);
    
    EXPECT_NE(staging.buffer, VK_NULL_HANDLE);
    EXPECT_NE(staging.memory, VK_NULL_HANDLE);
    EXPECT_EQ(staging.size, 2048);
    EXPECT_TRUE(staging.inUse);
    
    // Test staging buffer reuse
    pool->returnStagingBuffer(staging);
    auto newStaging = pool->getStagingBuffer(2048);
    
    EXPECT_EQ(newStaging.buffer, staging.buffer);
}

TEST_F(MemoryPoolTest, InvalidAllocation) {
    // Test allocation with invalid size
    EXPECT_THROW(pool->allocateBuffer(0, 
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT),
        std::runtime_error);
        
    // Test staging buffer with invalid size
    EXPECT_THROW(pool->getStagingBuffer(0),
        std::runtime_error);
}

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
} 
#include <gtest/gtest.h>
#include "VulkanEngine.h"

class MemoryPoolTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize Vulkan instance and device for testing
        // This is a simplified version - in real tests, you'd want to mock Vulkan
        VkInstanceCreateInfo instanceInfo{};
        instanceInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        vkCreateInstance(&instanceInfo, nullptr, &instance);
        
        // Create a test device
        // ... device creation code ...
    }

    void TearDown() override {
        if (device != VK_NULL_HANDLE) {
            vkDestroyDevice(device, nullptr);
        }
        if (instance != VK_NULL_HANDLE) {
            vkDestroyInstance(instance, nullptr);
        }
    }

    VkInstance instance = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
};

TEST_F(MemoryPoolTest, BufferAllocation) {
    MemoryPool pool(device);
    
    // Test buffer allocation
    auto allocation = pool.allocateBuffer(1024, 
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
    
    EXPECT_NE(allocation.buffer, VK_NULL_HANDLE);
    EXPECT_NE(allocation.memory, VK_NULL_HANDLE);
    EXPECT_EQ(allocation.size, 1024);
    EXPECT_TRUE(allocation.inUse);
    
    // Test buffer reuse
    pool.freeBuffer(allocation);
    auto newAllocation = pool.allocateBuffer(1024,
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
    
    EXPECT_EQ(newAllocation.buffer, allocation.buffer);
}

TEST_F(MemoryPoolTest, StagingBuffer) {
    MemoryPool pool(device);
    
    // Test staging buffer allocation
    auto staging = pool.getStagingBuffer(2048);
    
    EXPECT_NE(staging.buffer, VK_NULL_HANDLE);
    EXPECT_NE(staging.memory, VK_NULL_HANDLE);
    EXPECT_EQ(staging.size, 2048);
    EXPECT_TRUE(staging.inUse);
    
    // Test staging buffer reuse
    pool.returnStagingBuffer(staging);
    auto newStaging = pool.getStagingBuffer(2048);
    
    EXPECT_EQ(newStaging.buffer, staging.buffer);
}

TEST_F(MemoryPoolTest, InvalidAllocation) {
    MemoryPool pool(device);
    
    // Test allocation with invalid size
    EXPECT_THROW(pool.allocateBuffer(0, 
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT),
        std::runtime_error);
        
    // Test staging buffer with invalid size
    EXPECT_THROW(pool.getStagingBuffer(0),
        std::runtime_error);
} 
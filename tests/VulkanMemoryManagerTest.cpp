#include <gtest/gtest.h>
#include "VulkanMemoryManager.h"
#include "VulkanContext.h"
#include <memory>

class VulkanMemoryManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create a Vulkan context for memory manager
        context = std::make_unique<VulkanContext>();
        context->initialize();
        memoryManager = std::make_unique<VulkanMemoryManager>(context.get());
    }
    void TearDown() override {
        memoryManager.reset();
        if (context) context->cleanup();
        context.reset();
    }
    std::unique_ptr<VulkanContext> context;
    std::unique_ptr<VulkanMemoryManager> memoryManager;
};

TEST_F(VulkanMemoryManagerTest, Initialization) {
    ASSERT_NE(memoryManager, nullptr);
    // Check that the memory manager is initialized with a valid context
    EXPECT_NE(memoryManager->getDevice(), VK_NULL_HANDLE);
}

TEST_F(VulkanMemoryManagerTest, AllocateAndFreeBuffer) {
    VkBuffer buffer = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkDeviceSize size = 1024;
    VkBufferUsageFlags usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    VkMemoryPropertyFlags properties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

    bool result = memoryManager->allocateBuffer(size, usage, properties, buffer, memory);
    EXPECT_TRUE(result);
    EXPECT_NE(buffer, VK_NULL_HANDLE);
    EXPECT_NE(memory, VK_NULL_HANDLE);

    // Free the buffer
    memoryManager->freeBuffer(buffer, memory);
}

TEST_F(VulkanMemoryManagerTest, AllocateAndFreeImage) {
    VkImage image = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkExtent3D extent = {64, 64, 1};
    VkFormat format = VK_FORMAT_R8G8B8A8_UNORM;
    VkImageUsageFlags usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    VkMemoryPropertyFlags properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

    bool result = memoryManager->allocateImage(extent, format, usage, properties, image, memory);
    EXPECT_TRUE(result);
    EXPECT_NE(image, VK_NULL_HANDLE);
    EXPECT_NE(memory, VK_NULL_HANDLE);

    // Free the image
    memoryManager->freeImage(image, memory);
}

TEST_F(VulkanMemoryManagerTest, AllocationFailure) {
    VkBuffer buffer = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    // Try to allocate an unreasonably large buffer to force failure
    VkDeviceSize size = 1ull << 40; // 1 TB
    VkBufferUsageFlags usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    VkMemoryPropertyFlags properties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;

    bool result = memoryManager->allocateBuffer(size, usage, properties, buffer, memory);
    EXPECT_FALSE(result);
    EXPECT_EQ(buffer, VK_NULL_HANDLE);
    EXPECT_EQ(memory, VK_NULL_HANDLE);
} 
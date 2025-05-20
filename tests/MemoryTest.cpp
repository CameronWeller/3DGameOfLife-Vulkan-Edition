#include <gtest/gtest.h>
#include "../src/VulkanEngine.h"
#include "../src/VulkanMemoryManager.h"
#include <memory>

using namespace VulkanHIP;

class MemoryTest : public ::testing::Test {
protected:
    void SetUp() override {
        engine = std::make_unique<VulkanEngine>();
        engine->init();
        memoryManager = engine->getMemoryManager();
    }

    void TearDown() override {
        engine->cleanup();
    }

    std::unique_ptr<VulkanEngine> engine;
    VulkanMemoryManager* memoryManager;
};

// Test buffer creation
TEST_F(MemoryTest, BufferCreationTest) {
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = 1024;  // 1KB buffer
    bufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VkBuffer buffer;
    EXPECT_EQ(vkCreateBuffer(engine->getVulkanContext()->getDevice(), &bufferInfo, nullptr, &buffer), VK_SUCCESS);
    EXPECT_NE(buffer, VK_NULL_HANDLE);

    // Cleanup
    vkDestroyBuffer(engine->getVulkanContext()->getDevice(), buffer, nullptr);
}

// Test memory allocation through memory manager
TEST_F(MemoryTest, MemoryManagerAllocationTest) {
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = 1024;
    bufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VkBuffer buffer;
    vkCreateBuffer(engine->getVulkanContext()->getDevice(), &bufferInfo, nullptr, &buffer);

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(engine->getVulkanContext()->getDevice(), buffer, &memRequirements);

    VkDeviceMemory bufferMemory;
    EXPECT_TRUE(memoryManager->allocateMemory(memRequirements, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, bufferMemory));
    EXPECT_NE(bufferMemory, VK_NULL_HANDLE);

    // Cleanup
    memoryManager->freeMemory(bufferMemory);
    vkDestroyBuffer(engine->getVulkanContext()->getDevice(), buffer, nullptr);
}

// Test memory mapping through memory manager
TEST_F(MemoryTest, MemoryManagerMappingTest) {
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = 1024;
    bufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VkBuffer buffer;
    vkCreateBuffer(engine->getVulkanContext()->getDevice(), &bufferInfo, nullptr, &buffer);

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(engine->getVulkanContext()->getDevice(), buffer, &memRequirements);

    VkDeviceMemory bufferMemory;
    memoryManager->allocateMemory(memRequirements, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, bufferMemory);
    vkBindBufferMemory(engine->getVulkanContext()->getDevice(), buffer, bufferMemory, 0);

    // Test memory mapping
    void* data;
    EXPECT_TRUE(memoryManager->mapMemory(bufferMemory, 0, bufferInfo.size, 0, &data));
    EXPECT_NE(data, nullptr);

    // Write some test data
    uint32_t* testData = static_cast<uint32_t*>(data);
    testData[0] = 0x12345678;

    memoryManager->unmapMemory(bufferMemory);

    // Cleanup
    memoryManager->freeMemory(bufferMemory);
    vkDestroyBuffer(engine->getVulkanContext()->getDevice(), buffer, nullptr);
}

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
} 
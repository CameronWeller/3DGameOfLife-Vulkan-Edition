#include <gtest/gtest.h>
#include "../src/VulkanEngine.h"
#include <memory>

class MemoryTest : public ::testing::Test {
protected:
    void SetUp() override {
        engine = std::make_unique<VulkanEngine>();
        engine->initWindow(800, 600, "Memory Test Window");
        engine->initVulkan();
    }

    void TearDown() override {
        engine->cleanup();
    }

    std::unique_ptr<VulkanEngine> engine;
};

// Test buffer creation
TEST_F(MemoryTest, BufferCreationTest) {
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = 1024;  // 1KB buffer
    bufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VkBuffer buffer;
    EXPECT_EQ(vkCreateBuffer(engine->getDevice(), &bufferInfo, nullptr, &buffer), VK_SUCCESS);
    EXPECT_NE(buffer, VK_NULL_HANDLE);

    // Cleanup
    vkDestroyBuffer(engine->getDevice(), buffer, nullptr);
}

// Test memory allocation
TEST_F(MemoryTest, MemoryAllocationTest) {
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = 1024;
    bufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VkBuffer buffer;
    vkCreateBuffer(engine->getDevice(), &bufferInfo, nullptr, &buffer);

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(engine->getDevice(), buffer, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = engine->findMemoryType(
        memRequirements.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    );

    VkDeviceMemory bufferMemory;
    EXPECT_EQ(vkAllocateMemory(engine->getDevice(), &allocInfo, nullptr, &bufferMemory), VK_SUCCESS);
    EXPECT_NE(bufferMemory, VK_NULL_HANDLE);

    // Cleanup
    vkFreeMemory(engine->getDevice(), bufferMemory, nullptr);
    vkDestroyBuffer(engine->getDevice(), buffer, nullptr);
}

// Test memory mapping
TEST_F(MemoryTest, MemoryMappingTest) {
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = 1024;
    bufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VkBuffer buffer;
    vkCreateBuffer(engine->getDevice(), &bufferInfo, nullptr, &buffer);

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(engine->getDevice(), buffer, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = engine->findMemoryType(
        memRequirements.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    );

    VkDeviceMemory bufferMemory;
    vkAllocateMemory(engine->getDevice(), &allocInfo, nullptr, &bufferMemory);
    vkBindBufferMemory(engine->getDevice(), buffer, bufferMemory, 0);

    // Test memory mapping
    void* data;
    EXPECT_EQ(vkMapMemory(engine->getDevice(), bufferMemory, 0, bufferInfo.size, 0, &data), VK_SUCCESS);
    EXPECT_NE(data, nullptr);

    // Write some test data
    uint32_t* testData = static_cast<uint32_t*>(data);
    testData[0] = 0x12345678;

    vkUnmapMemory(engine->getDevice(), bufferMemory);

    // Cleanup
    vkFreeMemory(engine->getDevice(), bufferMemory, nullptr);
    vkDestroyBuffer(engine->getDevice(), buffer, nullptr);
}

// Test memory type finding
TEST_F(MemoryTest, MemoryTypeFindingTest) {
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = 1024;
    bufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VkBuffer buffer;
    vkCreateBuffer(engine->getDevice(), &bufferInfo, nullptr, &buffer);

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(engine->getDevice(), buffer, &memRequirements);

    // Test finding memory type for different properties
    uint32_t hostVisibleType = engine->findMemoryType(
        memRequirements.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    );
    EXPECT_NE(hostVisibleType, UINT32_MAX);

    uint32_t deviceLocalType = engine->findMemoryType(
        memRequirements.memoryTypeBits,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
    );
    EXPECT_NE(deviceLocalType, UINT32_MAX);

    // Cleanup
    vkDestroyBuffer(engine->getDevice(), buffer, nullptr);
}

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
} 
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "VulkanEngine.h"
#include "mocks/VulkanMock.hpp"

using ::testing::Return;
using ::testing::NiceMock;

class VulkanIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        g_vulkanMock = std::make_unique<NiceMock<VulkanMock>>();
        
        // Set up default mock behaviors
        ON_CALL(*g_vulkanMock, vkCreateInstance)
            .WillByDefault(Return(VK_SUCCESS));
        ON_CALL(*g_vulkanMock, vkCreateDevice)
            .WillByDefault(Return(VK_SUCCESS));
        ON_CALL(*g_vulkanMock, vkCreateBuffer)
            .WillByDefault(Return(VK_SUCCESS));
        ON_CALL(*g_vulkanMock, vkAllocateMemory)
            .WillByDefault(Return(VK_SUCCESS));
        ON_CALL(*g_vulkanMock, vkBindBufferMemory)
            .WillByDefault(Return(VK_SUCCESS));
    }

    void TearDown() override {
        g_vulkanMock.reset();
    }
};

TEST_F(VulkanIntegrationTest, CreateInstance) {
    EXPECT_CALL(*g_vulkanMock, vkCreateInstance)
        .Times(1)
        .WillOnce(Return(VK_SUCCESS));

    VulkanEngine engine;
    EXPECT_NO_THROW(engine.initVulkan());
}

TEST_F(VulkanIntegrationTest, CreateBuffer) {
    VulkanEngine engine;
    engine.initVulkan();

    EXPECT_CALL(*g_vulkanMock, vkCreateBuffer)
        .Times(1)
        .WillOnce(Return(VK_SUCCESS));
    EXPECT_CALL(*g_vulkanMock, vkAllocateMemory)
        .Times(1)
        .WillOnce(Return(VK_SUCCESS));
    EXPECT_CALL(*g_vulkanMock, vkBindBufferMemory)
        .Times(1)
        .WillOnce(Return(VK_SUCCESS));

    VkBuffer buffer;
    VkDeviceMemory memory;
    EXPECT_NO_THROW(engine.createBuffer(1024, 
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
        buffer, memory));
}

TEST_F(VulkanIntegrationTest, ShaderCompilation) {
    VulkanEngine engine;
    engine.initVulkan();

    EXPECT_CALL(*g_vulkanMock, vkCreateShaderModule)
        .Times(1)
        .WillOnce(Return(VK_SUCCESS));

    const std::vector<uint32_t> shaderCode = {0x07230203, 0x00010000, 0x00080001, 0x00000000};
    VkShaderModule shaderModule;
    EXPECT_NO_THROW(engine.createShaderModule(shaderCode, shaderModule));
}

TEST_F(VulkanIntegrationTest, ErrorHandling) {
    VulkanEngine engine;
    engine.initVulkan();

    EXPECT_CALL(*g_vulkanMock, vkCreateBuffer)
        .Times(1)
        .WillOnce(Return(VK_ERROR_OUT_OF_HOST_MEMORY));

    VkBuffer buffer;
    VkDeviceMemory memory;
    EXPECT_THROW(engine.createBuffer(1024,
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
        buffer, memory),
        std::runtime_error);
} 
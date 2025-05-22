#include <gtest/gtest.h>
#include "../src/VulkanEngine.h"
#include "../src/VulkanMemoryManager.h"
#include <vector>

using namespace VulkanHIP;

class MemoryTest : public ::testing::Test {
protected:
    void SetUp() override {
        engine = std::make_unique<VulkanEngine>();
        engine->init();
        memoryManager = &engine->getMemoryManager();
    }

    void TearDown() override {
        engine.reset();
    }

    std::unique_ptr<VulkanEngine> engine;
    VulkanMemoryManager* memoryManager;
};

TEST_F(MemoryTest, BufferCreation) {
    const VkDeviceSize bufferSize = 1024;
    auto allocation = memoryManager->createBuffer(
        bufferSize,
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VMA_MEMORY_USAGE_CPU_TO_GPU
    );

    EXPECT_NE(allocation.buffer, VK_NULL_HANDLE);
    EXPECT_NE(allocation.allocation, nullptr);
    EXPECT_EQ(allocation.size, bufferSize);
    EXPECT_NE(allocation.mappedData, nullptr);

    memoryManager->destroyBuffer(allocation);
}

TEST_F(MemoryTest, MemoryMapping) {
    const VkDeviceSize bufferSize = 1024;
    auto allocation = memoryManager->createBuffer(
        bufferSize,
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VMA_MEMORY_USAGE_CPU_TO_GPU
    );

    void* data = memoryManager->mapMemory(allocation);
    EXPECT_NE(data, nullptr);

    // Write some data
    std::vector<float> testData(256, 1.0f);
    memcpy(data, testData.data(), testData.size() * sizeof(float));

    memoryManager->unmapMemory(allocation);
    memoryManager->destroyBuffer(allocation);
}

TEST_F(MemoryTest, StagingBuffer) {
    const VkDeviceSize bufferSize = 1024;
    auto stagingBuffer = memoryManager->createStagingBuffer(bufferSize);

    EXPECT_NE(stagingBuffer.buffer, VK_NULL_HANDLE);
    EXPECT_NE(stagingBuffer.allocation, nullptr);
    EXPECT_EQ(stagingBuffer.size, bufferSize);
    EXPECT_NE(stagingBuffer.mappedData, nullptr);

    // Write some data
    std::vector<float> testData(256, 1.0f);
    memcpy(stagingBuffer.mappedData, testData.data(), testData.size() * sizeof(float));

    memoryManager->destroyStagingBuffer(stagingBuffer);
}

TEST_F(MemoryTest, BufferCopy) {
    const VkDeviceSize bufferSize = 1024;
    
    // Create source buffer
    auto srcAllocation = memoryManager->createBuffer(
        bufferSize,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VMA_MEMORY_USAGE_CPU_TO_GPU
    );

    // Create destination buffer
    auto dstAllocation = memoryManager->createBuffer(
        bufferSize,
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VMA_MEMORY_USAGE_GPU_ONLY
    );

    // Write data to source buffer
    std::vector<float> testData(256, 1.0f);
    memcpy(srcAllocation.mappedData, testData.data(), testData.size() * sizeof(float));

    // Copy data
    memoryManager->copyBuffer(srcAllocation, dstAllocation, bufferSize);

    memoryManager->destroyBuffer(srcAllocation);
    memoryManager->destroyBuffer(dstAllocation);
}

TEST_F(MemoryTest, ImageAllocation) {
    const uint32_t width = 512;
    const uint32_t height = 512;
    auto imageAlloc = memoryManager->allocateImage(
        width, height,
        VK_FORMAT_R8G8B8A8_UNORM,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
    );

    EXPECT_NE(imageAlloc.image, VK_NULL_HANDLE);
    EXPECT_NE(imageAlloc.allocation, nullptr);
    EXPECT_EQ(imageAlloc.size, width * height * 4);
    EXPECT_TRUE(imageAlloc.inUse);

    memoryManager->freeImage(imageAlloc);
}

TEST_F(MemoryTest, ImageViewCreation) {
    const uint32_t width = 512;
    const uint32_t height = 512;
    auto imageAlloc = memoryManager->allocateImage(
        width, height,
        VK_FORMAT_R8G8B8A8_UNORM,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
    );

    VkImageView imageView;
    memoryManager->createImageView(
        imageAlloc.image,
        VK_FORMAT_R8G8B8A8_UNORM,
        VK_IMAGE_ASPECT_COLOR_BIT,
        imageView
    );

    EXPECT_NE(imageView, VK_NULL_HANDLE);

    vkDestroyImageView(memoryManager->getDevice(), imageView, nullptr);
    memoryManager->freeImage(imageAlloc);
}

TEST_F(MemoryTest, ImageLayoutTransition) {
    const uint32_t width = 512;
    const uint32_t height = 512;
    auto imageAlloc = memoryManager->allocateImage(
        width, height,
        VK_FORMAT_R8G8B8A8_UNORM,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
    );

    // Test transition from UNDEFINED to TRANSFER_DST_OPTIMAL
    memoryManager->transitionImageLayout(
        imageAlloc.image,
        VK_FORMAT_R8G8B8A8_UNORM,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
    );

    // Test transition from TRANSFER_DST_OPTIMAL to SHADER_READ_ONLY_OPTIMAL
    memoryManager->transitionImageLayout(
        imageAlloc.image,
        VK_FORMAT_R8G8B8A8_UNORM,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    );

    memoryManager->freeImage(imageAlloc);
}

TEST_F(MemoryTest, BufferToImageCopy) {
    const uint32_t width = 512;
    const uint32_t height = 512;
    const VkDeviceSize bufferSize = width * height * 4;

    // Create staging buffer
    auto stagingBuffer = memoryManager->createStagingBuffer(bufferSize);
    std::vector<uint8_t> testData(bufferSize, 0xFF);
    memcpy(stagingBuffer.mappedData, testData.data(), bufferSize);

    // Create destination image
    auto imageAlloc = memoryManager->allocateImage(
        width, height,
        VK_FORMAT_R8G8B8A8_UNORM,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
    );

    // Transition image layout and copy
    memoryManager->transitionImageLayout(
        imageAlloc.image,
        VK_FORMAT_R8G8B8A8_UNORM,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
    );

    memoryManager->copyBufferToImage(stagingBuffer.buffer, imageAlloc.image, width, height);

    memoryManager->destroyStagingBuffer(stagingBuffer);
    memoryManager->freeImage(imageAlloc);
}

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
} 
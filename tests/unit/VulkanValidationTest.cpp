#include <gtest/gtest.h>
#include <memory>
#include <vector>
#include "VulkanContext.h"
#include "GameOfLife3D.h"
#include "ComputeShader.h"

class VulkanValidationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Enable validation layers
        vulkanContext = std::make_unique<VulkanContext>();
        ASSERT_TRUE(vulkanContext->initialize(true));  // true enables validation layers
    }

    void TearDown() override {
        vulkanContext.reset();
    }

    std::unique_ptr<VulkanContext> vulkanContext;
};

// Test proper buffer creation and destruction
TEST_F(VulkanValidationTest, BufferCreationDestruction) {
    const int bufferSize = 1024 * 1024;  // 1MB buffer
    
    // Create buffer with invalid size
    auto invalidBuffer = vulkanContext->createBuffer(
        0,  // Invalid size
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
    );
    EXPECT_EQ(invalidBuffer, VK_NULL_HANDLE) << "Buffer creation should fail with invalid size";
    
    // Create buffer with invalid usage flags
    auto invalidUsageBuffer = vulkanContext->createBuffer(
        bufferSize,
        0,  // Invalid usage flags
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
    );
    EXPECT_EQ(invalidUsageBuffer, VK_NULL_HANDLE) << "Buffer creation should fail with invalid usage flags";
    
    // Create valid buffer
    auto buffer = vulkanContext->createBuffer(
        bufferSize,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    );
    EXPECT_NE(buffer, VK_NULL_HANDLE) << "Buffer creation should succeed with valid parameters";
    
    // Destroy buffer
    vulkanContext->destroyBuffer(buffer);
}

// Test proper texture creation and destruction
TEST_F(VulkanValidationTest, TextureCreationDestruction) {
    const int textureSize = 1024;  // 1024x1024 texture
    
    // Create texture with invalid size
    auto invalidTexture = vulkanContext->createTexture(
        0,  // Invalid width
        textureSize,
        VK_FORMAT_R8G8B8A8_UNORM,
        VK_IMAGE_USAGE_SAMPLED_BIT
    );
    EXPECT_EQ(invalidTexture, VK_NULL_HANDLE) << "Texture creation should fail with invalid size";
    
    // Create texture with invalid format
    auto invalidFormatTexture = vulkanContext->createTexture(
        textureSize,
        textureSize,
        VK_FORMAT_UNDEFINED,  // Invalid format
        VK_IMAGE_USAGE_SAMPLED_BIT
    );
    EXPECT_EQ(invalidFormatTexture, VK_NULL_HANDLE) << "Texture creation should fail with invalid format";
    
    // Create valid texture
    auto texture = vulkanContext->createTexture(
        textureSize,
        textureSize,
        VK_FORMAT_R8G8B8A8_UNORM,
        VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT
    );
    EXPECT_NE(texture, VK_NULL_HANDLE) << "Texture creation should succeed with valid parameters";
    
    // Destroy texture
    vulkanContext->destroyTexture(texture);
}

// Test proper pipeline creation and destruction
TEST_F(VulkanValidationTest, PipelineCreationDestruction) {
    // Create pipeline with invalid shader path
    auto invalidPipeline = vulkanContext->createComputePipeline("nonexistent_shader.comp.spv");
    EXPECT_EQ(invalidPipeline, VK_NULL_HANDLE) << "Pipeline creation should fail with invalid shader path";
    
    // Create valid pipeline
    auto pipeline = vulkanContext->createComputePipeline("shaders/game_of_life.comp.spv");
    EXPECT_NE(pipeline, VK_NULL_HANDLE) << "Pipeline creation should succeed with valid shader";
    
    // Destroy pipeline
    vulkanContext->destroyPipeline(pipeline);
}

// Test proper command buffer usage
TEST_F(VulkanValidationTest, CommandBufferUsage) {
    // Create command buffer
    auto cmdBuffer = vulkanContext->createCommandBuffer();
    EXPECT_NE(cmdBuffer, VK_NULL_HANDLE) << "Command buffer creation should succeed";
    
    // Begin command buffer
    EXPECT_TRUE(vulkanContext->beginCommandBuffer(cmdBuffer)) << "Command buffer begin should succeed";
    
    // End command buffer
    EXPECT_TRUE(vulkanContext->endCommandBuffer(cmdBuffer)) << "Command buffer end should succeed";
    
    // Submit command buffer
    EXPECT_TRUE(vulkanContext->submitCommandBuffer(cmdBuffer)) << "Command buffer submission should succeed";
    
    // Wait for completion
    EXPECT_TRUE(vulkanContext->waitForCompute()) << "Command buffer completion wait should succeed";
    
    // Destroy command buffer
    vulkanContext->destroyCommandBuffer(cmdBuffer);
}

// Test proper synchronization
TEST_F(VulkanValidationTest, Synchronization) {
    // Create compute shader
    auto shader = std::make_unique<ComputeShader>(vulkanContext.get());
    ASSERT_TRUE(shader->initialize());
    
    // Create semaphores
    auto semaphore = vulkanContext->createSemaphore();
    EXPECT_NE(semaphore, VK_NULL_HANDLE) << "Semaphore creation should succeed";
    
    // Create fence
    auto fence = vulkanContext->createFence();
    EXPECT_NE(fence, VK_NULL_HANDLE) << "Fence creation should succeed";
    
    // Execute compute shader with synchronization
    EXPECT_TRUE(shader->execute(semaphore, fence)) << "Compute shader execution with synchronization should succeed";
    
    // Wait for fence
    EXPECT_TRUE(vulkanContext->waitForFence(fence)) << "Fence wait should succeed";
    
    // Clean up
    vulkanContext->destroySemaphore(semaphore);
    vulkanContext->destroyFence(fence);
}

// Test proper descriptor set usage
TEST_F(VulkanValidationTest, DescriptorSetUsage) {
    // Create descriptor set layout
    auto layout = vulkanContext->createDescriptorSetLayout();
    EXPECT_NE(layout, VK_NULL_HANDLE) << "Descriptor set layout creation should succeed";
    
    // Create descriptor pool
    auto pool = vulkanContext->createDescriptorPool();
    EXPECT_NE(pool, VK_NULL_HANDLE) << "Descriptor pool creation should succeed";
    
    // Allocate descriptor set
    auto descriptorSet = vulkanContext->allocateDescriptorSet(pool, layout);
    EXPECT_NE(descriptorSet, VK_NULL_HANDLE) << "Descriptor set allocation should succeed";
    
    // Create buffer for descriptor set
    const int bufferSize = 1024;
    auto buffer = vulkanContext->createBuffer(
        bufferSize,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
    );
    EXPECT_NE(buffer, VK_NULL_HANDLE) << "Buffer creation should succeed";
    
    // Update descriptor set
    EXPECT_TRUE(vulkanContext->updateDescriptorSet(descriptorSet, buffer)) << "Descriptor set update should succeed";
    
    // Clean up
    vulkanContext->destroyBuffer(buffer);
    vulkanContext->destroyDescriptorPool(pool);
    vulkanContext->destroyDescriptorSetLayout(layout);
}

// Test proper shader module creation and destruction
TEST_F(VulkanValidationTest, ShaderModuleCreationDestruction) {
    // Create shader module with invalid SPIR-V
    std::vector<uint32_t> invalidSpirv = {0x00000000};  // Invalid SPIR-V
    auto invalidModule = vulkanContext->createShaderModule(invalidSpirv);
    EXPECT_EQ(invalidModule, VK_NULL_HANDLE) << "Shader module creation should fail with invalid SPIR-V";
    
    // Load and create valid shader module
    auto module = vulkanContext->loadShaderModule("shaders/game_of_life.comp.spv");
    EXPECT_NE(module, VK_NULL_HANDLE) << "Shader module creation should succeed with valid SPIR-V";
    
    // Destroy shader module
    vulkanContext->destroyShaderModule(module);
} 
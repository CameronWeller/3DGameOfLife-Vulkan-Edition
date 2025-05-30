#include <gtest/gtest.h>
#include <memory>
#include <vector>
#include "VulkanContext.h"
#include "GameOfLife3D.h"
#include "ComputeShader.h"

class MemoryLeakTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Enable validation layers for memory tracking
        vulkanContext = std::make_unique<VulkanContext>();
        ASSERT_TRUE(vulkanContext->initialize(true));  // true enables validation layers
    }

    void TearDown() override {
        vulkanContext.reset();
    }

    std::unique_ptr<VulkanContext> vulkanContext;
};

// Test for memory leaks during Game of Life initialization and cleanup
TEST_F(MemoryLeakTest, GameOfLifeMemoryLeak) {
    const int numIterations = 100;
    std::vector<std::unique_ptr<GameOfLife3D>> gameInstances;
    
    // Track initial memory usage
    size_t initialMemory = vulkanContext->getTotalMemoryUsage();
    
    // Create and destroy multiple Game of Life instances
    for (int i = 0; i < numIterations; ++i) {
        auto game = std::make_unique<GameOfLife3D>(vulkanContext.get());
        ASSERT_TRUE(game->initializeGrid(64, 64, 64));
        gameInstances.push_back(std::move(game));
        
        // Simulate some updates
        for (int j = 0; j < 10; ++j) {
            gameInstances.back()->update();
        }
        
        // Clear instances periodically to test cleanup
        if (i % 10 == 0) {
            gameInstances.clear();
        }
    }
    
    // Clear all instances
    gameInstances.clear();
    
    // Check final memory usage
    size_t finalMemory = vulkanContext->getTotalMemoryUsage();
    size_t memoryLeak = finalMemory - initialMemory;
    
    EXPECT_EQ(memoryLeak, 0) << "Memory leak detected: " << memoryLeak << " bytes";
}

// Test for memory leaks during compute shader operations
TEST_F(MemoryLeakTest, ComputeShaderMemoryLeak) {
    const int numIterations = 100;
    std::vector<std::unique_ptr<ComputeShader>> shaderInstances;
    
    // Track initial memory usage
    size_t initialMemory = vulkanContext->getTotalMemoryUsage();
    
    // Create and destroy multiple compute shader instances
    for (int i = 0; i < numIterations; ++i) {
        auto shader = std::make_unique<ComputeShader>(vulkanContext.get());
        ASSERT_TRUE(shader->initialize());
        shaderInstances.push_back(std::move(shader));
        
        // Simulate some compute operations
        for (int j = 0; j < 10; ++j) {
            shaderInstances.back()->execute();
            vulkanContext->waitForCompute();
        }
        
        // Clear instances periodically to test cleanup
        if (i % 10 == 0) {
            shaderInstances.clear();
        }
    }
    
    // Clear all instances
    shaderInstances.clear();
    
    // Check final memory usage
    size_t finalMemory = vulkanContext->getTotalMemoryUsage();
    size_t memoryLeak = finalMemory - initialMemory;
    
    EXPECT_EQ(memoryLeak, 0) << "Memory leak detected: " << memoryLeak << " bytes";
}

// Test for memory leaks during buffer operations
TEST_F(MemoryLeakTest, BufferMemoryLeak) {
    const int numIterations = 100;
    const int bufferSize = 1024 * 1024;  // 1MB buffer
    
    // Track initial memory usage
    size_t initialMemory = vulkanContext->getTotalMemoryUsage();
    
    // Create and destroy multiple buffers
    for (int i = 0; i < numIterations; ++i) {
        std::vector<float> data(bufferSize / sizeof(float), 1.0f);
        
        // Create buffer and upload data
        auto buffer = vulkanContext->createBuffer(
            bufferSize,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
        );
        
        ASSERT_TRUE(buffer != VK_NULL_HANDLE);
        ASSERT_TRUE(vulkanContext->uploadBufferData(buffer, data.data(), bufferSize));
        
        // Simulate some buffer operations
        for (int j = 0; j < 10; ++j) {
            vulkanContext->downloadBufferData(buffer, data.data(), bufferSize);
        }
        
        // Destroy buffer
        vulkanContext->destroyBuffer(buffer);
    }
    
    // Check final memory usage
    size_t finalMemory = vulkanContext->getTotalMemoryUsage();
    size_t memoryLeak = finalMemory - initialMemory;
    
    EXPECT_EQ(memoryLeak, 0) << "Memory leak detected: " << memoryLeak << " bytes";
}

// Test for memory leaks during texture operations
TEST_F(MemoryLeakTest, TextureMemoryLeak) {
    const int numIterations = 100;
    const int textureSize = 1024;  // 1024x1024 texture
    
    // Track initial memory usage
    size_t initialMemory = vulkanContext->getTotalMemoryUsage();
    
    // Create and destroy multiple textures
    for (int i = 0; i < numIterations; ++i) {
        std::vector<uint32_t> data(textureSize * textureSize, 0xFFFFFFFF);
        
        // Create texture and upload data
        auto texture = vulkanContext->createTexture(
            textureSize,
            textureSize,
            VK_FORMAT_R8G8B8A8_UNORM,
            VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT
        );
        
        ASSERT_TRUE(texture != VK_NULL_HANDLE);
        ASSERT_TRUE(vulkanContext->uploadTextureData(texture, data.data(), textureSize * textureSize * 4));
        
        // Simulate some texture operations
        for (int j = 0; j < 10; ++j) {
            vulkanContext->transitionImageLayout(texture, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        }
        
        // Destroy texture
        vulkanContext->destroyTexture(texture);
    }
    
    // Check final memory usage
    size_t finalMemory = vulkanContext->getTotalMemoryUsage();
    size_t memoryLeak = finalMemory - initialMemory;
    
    EXPECT_EQ(memoryLeak, 0) << "Memory leak detected: " << memoryLeak << " bytes";
}

// Test for memory leaks during pipeline operations
TEST_F(MemoryLeakTest, PipelineMemoryLeak) {
    const int numIterations = 100;
    
    // Track initial memory usage
    size_t initialMemory = vulkanContext->getTotalMemoryUsage();
    
    // Create and destroy multiple pipelines
    for (int i = 0; i < numIterations; ++i) {
        // Create compute pipeline
        auto pipeline = vulkanContext->createComputePipeline("shaders/game_of_life.comp.spv");
        ASSERT_TRUE(pipeline != VK_NULL_HANDLE);
        
        // Simulate some pipeline operations
        for (int j = 0; j < 10; ++j) {
            vulkanContext->bindComputePipeline(pipeline);
        }
        
        // Destroy pipeline
        vulkanContext->destroyPipeline(pipeline);
    }
    
    // Check final memory usage
    size_t finalMemory = vulkanContext->getTotalMemoryUsage();
    size_t memoryLeak = finalMemory - initialMemory;
    
    EXPECT_EQ(memoryLeak, 0) << "Memory leak detected: " << memoryLeak << " bytes";
} 
#include <gtest/gtest.h>
#include <memory>
#include <vector>
#include "VulkanContext.h"
#include "GameOfLife3D.h"
#include "ComputeShader.h"

class GPUMemoryTest : public ::testing::Test {
protected:
    void SetUp() override {
        vulkanContext = std::make_unique<VulkanContext>();
        ASSERT_TRUE(vulkanContext->initialize(true));  // Enable validation layers
    }

    void TearDown() override {
        vulkanContext.reset();
    }

    std::unique_ptr<VulkanContext> vulkanContext;
};

// Test GPU memory allocation and deallocation
TEST_F(GPUMemoryTest, BufferMemoryAllocation) {
    const std::vector<size_t> bufferSizes = {
        1024 * 1024,      // 1MB
        4 * 1024 * 1024,  // 4MB
        16 * 1024 * 1024  // 16MB
    };

    for (size_t size : bufferSizes) {
        // Track initial GPU memory
        size_t initialGPUMemory = vulkanContext->getGPUMemoryUsage();
        
        // Create buffer
        auto buffer = vulkanContext->createBuffer(
            size,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
        );
        ASSERT_NE(buffer, VK_NULL_HANDLE) << "Failed to create buffer of size " << size;
        
        // Check GPU memory after allocation
        size_t allocatedGPUMemory = vulkanContext->getGPUMemoryUsage();
        EXPECT_GE(allocatedGPUMemory - initialGPUMemory, size) 
            << "GPU memory allocation less than expected for buffer size " << size;
        
        // Destroy buffer
        vulkanContext->destroyBuffer(buffer);
        
        // Check GPU memory after deallocation
        size_t finalGPUMemory = vulkanContext->getGPUMemoryUsage();
        EXPECT_LE(finalGPUMemory - initialGPUMemory, 1024 * 1024) 
            << "GPU memory leak detected after buffer destruction";
    }
}

// Test GPU memory fragmentation
TEST_F(GPUMemoryTest, MemoryFragmentation) {
    const int numBuffers = 100;
    const size_t bufferSize = 1024 * 1024;  // 1MB
    std::vector<VkBuffer> buffers;
    
    // Track initial GPU memory
    size_t initialGPUMemory = vulkanContext->getGPUMemoryUsage();
    
    // Create and destroy buffers in a pattern to test fragmentation
    for (int i = 0; i < numBuffers; ++i) {
        // Create buffer
        auto buffer = vulkanContext->createBuffer(
            bufferSize,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
        );
        ASSERT_NE(buffer, VK_NULL_HANDLE) << "Failed to create buffer " << i;
        buffers.push_back(buffer);
        
        // Destroy every other buffer to create fragmentation
        if (i % 2 == 1 && !buffers.empty()) {
            vulkanContext->destroyBuffer(buffers.back());
            buffers.pop_back();
        }
    }
    
    // Destroy remaining buffers
    for (auto buffer : buffers) {
        vulkanContext->destroyBuffer(buffer);
    }
    
    // Check final GPU memory
    size_t finalGPUMemory = vulkanContext->getGPUMemoryUsage();
    EXPECT_LE(finalGPUMemory - initialGPUMemory, 1024 * 1024) 
        << "GPU memory leak detected after fragmentation test";
}

// Test GPU memory usage during compute operations
TEST_F(GPUMemoryTest, ComputeMemoryUsage) {
    const int gridSize = 64;
    const int numIterations = 1000;
    
    auto game = std::make_unique<GameOfLife3D>(vulkanContext.get());
    ASSERT_TRUE(game->initializeGrid(gridSize, gridSize, gridSize));
    
    // Track initial GPU memory
    size_t initialGPUMemory = vulkanContext->getGPUMemoryUsage();
    size_t maxGPUMemory = initialGPUMemory;
    
    for (int i = 0; i < numIterations; ++i) {
        // Update simulation
        game->update();
        
        // Check GPU memory usage
        size_t currentGPUMemory = vulkanContext->getGPUMemoryUsage();
        maxGPUMemory = std::max(maxGPUMemory, currentGPUMemory);
        
        // Verify memory stability
        EXPECT_LE(currentGPUMemory - initialGPUMemory, 1024 * 1024 * 2) 
            << "GPU memory usage increased by more than 2MB at iteration " << i;
    }
    
    // Log memory usage
    std::cout << "Compute Memory Test Results:" << std::endl;
    std::cout << "  Initial GPU Memory: " << initialGPUMemory / 1024 << "KB" << std::endl;
    std::cout << "  Max GPU Memory: " << maxGPUMemory / 1024 << "KB" << std::endl;
    std::cout << "  Final GPU Memory: " << vulkanContext->getGPUMemoryUsage() / 1024 << "KB" << std::endl;
}

// Test GPU memory usage during texture operations
TEST_F(GPUMemoryTest, TextureMemoryUsage) {
    const std::vector<std::pair<int, int>> textureSizes = {
        {512, 512},
        {1024, 1024},
        {2048, 2048}
    };
    
    for (const auto& [width, height] : textureSizes) {
        // Track initial GPU memory
        size_t initialGPUMemory = vulkanContext->getGPUMemoryUsage();
        
        // Create texture
        auto texture = vulkanContext->createTexture(
            width,
            height,
            VK_FORMAT_R8G8B8A8_UNORM,
            VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT
        );
        ASSERT_NE(texture, VK_NULL_HANDLE) << "Failed to create texture " << width << "x" << height;
        
        // Check GPU memory after allocation
        size_t allocatedGPUMemory = vulkanContext->getGPUMemoryUsage();
        size_t expectedMemory = width * height * 4;  // 4 bytes per pixel
        EXPECT_GE(allocatedGPUMemory - initialGPUMemory, expectedMemory) 
            << "GPU memory allocation less than expected for texture " << width << "x" << height;
        
        // Destroy texture
        vulkanContext->destroyTexture(texture);
        
        // Check GPU memory after deallocation
        size_t finalGPUMemory = vulkanContext->getGPUMemoryUsage();
        EXPECT_LE(finalGPUMemory - initialGPUMemory, 1024 * 1024) 
            << "GPU memory leak detected after texture destruction";
    }
}

// Test GPU memory usage during pipeline operations
TEST_F(GPUMemoryTest, PipelineMemoryUsage) {
    const int numPipelines = 10;
    std::vector<VkPipeline> pipelines;
    
    // Track initial GPU memory
    size_t initialGPUMemory = vulkanContext->getGPUMemoryUsage();
    
    // Create multiple pipelines
    for (int i = 0; i < numPipelines; ++i) {
        auto pipeline = vulkanContext->createComputePipeline("shaders/game_of_life.comp.spv");
        ASSERT_NE(pipeline, VK_NULL_HANDLE) << "Failed to create pipeline " << i;
        pipelines.push_back(pipeline);
    }
    
    // Check GPU memory after pipeline creation
    size_t allocatedGPUMemory = vulkanContext->getGPUMemoryUsage();
    EXPECT_GT(allocatedGPUMemory - initialGPUMemory, 0) 
        << "No GPU memory allocated for pipelines";
    
    // Destroy pipelines
    for (auto pipeline : pipelines) {
        vulkanContext->destroyPipeline(pipeline);
    }
    
    // Check GPU memory after pipeline destruction
    size_t finalGPUMemory = vulkanContext->getGPUMemoryUsage();
    EXPECT_LE(finalGPUMemory - initialGPUMemory, 1024 * 1024) 
        << "GPU memory leak detected after pipeline destruction";
} 
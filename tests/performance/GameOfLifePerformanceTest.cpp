#include <gtest/gtest.h>
#include <benchmark/benchmark.h>
#include <chrono>
#include <memory>
#include "GameOfLife3D.h"
#include "VulkanContext.h"

class GameOfLifePerformanceTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize Vulkan context
        vulkanContext = std::make_unique<VulkanContext>();
        ASSERT_TRUE(vulkanContext->initialize());
        
        // Initialize Game of Life with different grid sizes
        gameOfLife = std::make_unique<GameOfLife3D>(vulkanContext.get());
    }

    void TearDown() override {
        gameOfLife.reset();
        vulkanContext.reset();
    }

    std::unique_ptr<VulkanContext> vulkanContext;
    std::unique_ptr<GameOfLife3D> gameOfLife;
};

// Test simulation performance with different grid sizes
TEST_F(GameOfLifePerformanceTest, SimulationPerformance) {
    const std::vector<int> gridSizes = {32, 64, 128};
    const int iterations = 100;

    for (int size : gridSizes) {
        gameOfLife->initializeGrid(size, size, size);
        
        auto start = std::chrono::high_resolution_clock::now();
        
        for (int i = 0; i < iterations; ++i) {
            gameOfLife->update();
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        
        double avgTimePerIteration = static_cast<double>(duration.count()) / iterations;
        double fps = 1000.0 / avgTimePerIteration;
        
        std::cout << "Grid size " << size << "x" << size << "x" << size << ":\n"
                  << "  Average time per iteration: " << avgTimePerIteration << "ms\n"
                  << "  FPS: " << fps << "\n";
        
        // Performance requirements: at least 30 FPS for 128³ grid
        if (size == 128) {
            EXPECT_GE(fps, 30.0) << "Performance below target for 128³ grid";
        }
    }
}

// Test memory usage with different grid sizes
TEST_F(GameOfLifePerformanceTest, MemoryUsage) {
    const std::vector<int> gridSizes = {32, 64, 128};
    
    for (int size : gridSizes) {
        size_t initialMemory = vulkanContext->getTotalMemoryUsage();
        
        gameOfLife->initializeGrid(size, size, size);
        
        size_t finalMemory = vulkanContext->getTotalMemoryUsage();
        size_t memoryUsed = finalMemory - initialMemory;
        
        std::cout << "Grid size " << size << "x" << size << "x" << size << ":\n"
                  << "  Memory used: " << (memoryUsed / (1024 * 1024)) << "MB\n";
        
        // Memory requirements: less than 1GB for 128³ grid
        if (size == 128) {
            EXPECT_LT(memoryUsed, 1024 * 1024 * 1024) << "Memory usage exceeds 1GB for 128³ grid";
        }
    }
}

// Test initialization performance
TEST_F(GameOfLifePerformanceTest, InitializationPerformance) {
    const std::vector<int> gridSizes = {32, 64, 128};
    
    for (int size : gridSizes) {
        auto start = std::chrono::high_resolution_clock::now();
        
        gameOfLife->initializeGrid(size, size, size);
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        
        std::cout << "Grid size " << size << "x" << size << "x" << size << ":\n"
                  << "  Initialization time: " << duration.count() << "ms\n";
        
        // Initialization requirements: less than 1 second for 128³ grid
        if (size == 128) {
            EXPECT_LT(duration.count(), 1000) << "Initialization time exceeds 1 second for 128³ grid";
        }
    }
}

// Test pattern loading performance
TEST_F(GameOfLifePerformanceTest, PatternLoadingPerformance) {
    const std::vector<std::string> patterns = {
        "glider",
        "blinker",
        "random"
    };
    
    gameOfLife->initializeGrid(64, 64, 64);
    
    for (const auto& pattern : patterns) {
        auto start = std::chrono::high_resolution_clock::now();
        
        gameOfLife->loadPattern(pattern);
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        
        std::cout << "Pattern: " << pattern << "\n"
                  << "  Loading time: " << duration.count() << "ms\n";
        
        // Pattern loading requirements: less than 100ms per pattern
        EXPECT_LT(duration.count(), 100) << "Pattern loading time exceeds 100ms";
    }
}

// Test rendering performance
TEST_F(GameOfLifePerformanceTest, RenderingPerformance) {
    const std::vector<int> gridSizes = {32, 64, 128};
    const int frames = 100;
    
    for (int size : gridSizes) {
        gameOfLife->initializeGrid(size, size, size);
        
        auto start = std::chrono::high_resolution_clock::now();
        
        for (int i = 0; i < frames; ++i) {
            gameOfLife->render();
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        
        double avgTimePerFrame = static_cast<double>(duration.count()) / frames;
        double fps = 1000.0 / avgTimePerFrame;
        
        std::cout << "Grid size " << size << "x" << size << "x" << size << ":\n"
                  << "  Average time per frame: " << avgTimePerFrame << "ms\n"
                  << "  FPS: " << fps << "\n";
        
        // Rendering requirements: at least 60 FPS for 128³ grid
        if (size == 128) {
            EXPECT_GE(fps, 60.0) << "Rendering performance below target for 128³ grid";
        }
    }
} 
#include <gtest/gtest.h>
#include <memory>
#include <chrono>
#include <thread>
#include "GameOfLife3D.h"
#include "VulkanContext.h"

class StressTest : public ::testing::Test {
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

// Test long-running simulation stability
TEST_F(StressTest, LongRunningSimulation) {
    const int gridSize = 64;
    const int numIterations = 10000;  // Run for 10,000 iterations
    const int checkInterval = 1000;   // Check memory every 1,000 iterations
    
    auto game = std::make_unique<GameOfLife3D>(vulkanContext.get());
    ASSERT_TRUE(game->initializeGrid(gridSize, gridSize, gridSize));
    
    // Track initial memory usage
    size_t initialMemory = vulkanContext->getTotalMemoryUsage();
    size_t maxMemory = initialMemory;
    size_t minFPS = std::numeric_limits<double>::max();
    
    auto startTime = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < numIterations; ++i) {
        // Update simulation
        game->update();
        
        // Check memory usage periodically
        if (i % checkInterval == 0) {
            size_t currentMemory = vulkanContext->getTotalMemoryUsage();
            maxMemory = std::max(maxMemory, currentMemory);
            
            // Calculate FPS
            auto currentTime = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - startTime).count();
            double fps = (i + 1) * 1000.0 / duration;
            minFPS = std::min(minFPS, fps);
            
            // Verify memory stability
            EXPECT_LE(currentMemory - initialMemory, 1024 * 1024) 
                << "Memory usage increased by more than 1MB at iteration " << i;
            
            // Verify performance
            EXPECT_GE(fps, 30.0) << "FPS dropped below 30 at iteration " << i;
        }
    }
    
    // Final memory check
    size_t finalMemory = vulkanContext->getTotalMemoryUsage();
    EXPECT_LE(finalMemory - initialMemory, 1024 * 1024) 
        << "Final memory usage increased by more than 1MB";
    
    // Log performance metrics
    std::cout << "Stress Test Results:" << std::endl;
    std::cout << "  Total Iterations: " << numIterations << std::endl;
    std::cout << "  Initial Memory: " << initialMemory / 1024 << "KB" << std::endl;
    std::cout << "  Max Memory: " << maxMemory / 1024 << "KB" << std::endl;
    std::cout << "  Final Memory: " << finalMemory / 1024 << "KB" << std::endl;
    std::cout << "  Minimum FPS: " << minFPS << std::endl;
}

// Test resource stability under rapid grid size changes
TEST_F(StressTest, RapidGridSizeChanges) {
    const int numChanges = 100;
    const std::vector<int> gridSizes = {32, 64, 128, 256};
    
    auto game = std::make_unique<GameOfLife3D>(vulkanContext.get());
    size_t initialMemory = vulkanContext->getTotalMemoryUsage();
    
    for (int i = 0; i < numChanges; ++i) {
        // Change grid size
        int size = gridSizes[i % gridSizes.size()];
        ASSERT_TRUE(game->initializeGrid(size, size, size));
        
        // Run a few iterations
        for (int j = 0; j < 10; ++j) {
            game->update();
        }
        
        // Check memory usage
        size_t currentMemory = vulkanContext->getTotalMemoryUsage();
        EXPECT_LE(currentMemory - initialMemory, 1024 * 1024 * 2) 
            << "Memory usage increased by more than 2MB at change " << i;
    }
}

// Test stability under rapid rule changes
TEST_F(StressTest, RapidRuleChanges) {
    const int gridSize = 64;
    const int numChanges = 100;
    const std::vector<std::string> ruleSets = {"5766", "4555", "B3/S23"};
    
    auto game = std::make_unique<GameOfLife3D>(vulkanContext.get());
    ASSERT_TRUE(game->initializeGrid(gridSize, gridSize, gridSize));
    
    size_t initialMemory = vulkanContext->getTotalMemoryUsage();
    
    for (int i = 0; i < numChanges; ++i) {
        // Change rule set
        std::string ruleSet = ruleSets[i % ruleSets.size()];
        ASSERT_TRUE(game->setRuleSet(ruleSet));
        
        // Run a few iterations
        for (int j = 0; j < 10; ++j) {
            game->update();
        }
        
        // Check memory usage
        size_t currentMemory = vulkanContext->getTotalMemoryUsage();
        EXPECT_LE(currentMemory - initialMemory, 1024 * 1024) 
            << "Memory usage increased by more than 1MB at change " << i;
    }
}

// Test stability under rapid pattern loading
TEST_F(StressTest, RapidPatternLoading) {
    const int gridSize = 64;
    const int numLoads = 100;
    
    auto game = std::make_unique<GameOfLife3D>(vulkanContext.get());
    ASSERT_TRUE(game->initializeGrid(gridSize, gridSize, gridSize));
    
    size_t initialMemory = vulkanContext->getTotalMemoryUsage();
    
    for (int i = 0; i < numLoads; ++i) {
        // Create and load a random pattern
        std::vector<bool> pattern(gridSize * gridSize * gridSize, false);
        for (size_t j = 0; j < pattern.size(); j += 100) {
            pattern[j] = true;  // Create a sparse pattern
        }
        
        ASSERT_TRUE(game->loadPattern(pattern));
        
        // Run a few iterations
        for (int j = 0; j < 10; ++j) {
            game->update();
        }
        
        // Check memory usage
        size_t currentMemory = vulkanContext->getTotalMemoryUsage();
        EXPECT_LE(currentMemory - initialMemory, 1024 * 1024) 
            << "Memory usage increased by more than 1MB at load " << i;
    }
} 
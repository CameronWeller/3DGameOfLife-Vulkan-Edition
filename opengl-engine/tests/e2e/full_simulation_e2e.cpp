#include <gtest/gtest.h>
#include "opengl/Grid3D.h"
#include "opengl/compute/OpenGLCompute.h"
#include "opengl/rendering/OpenGLVoxelRenderer.h"
#include "opengl/ui/OpenGLImGui.h"
#include "opengl/OpenGLContext.h"
#include "opengl/OpenGLWindowManager.h"
#include "../../../include/Logger.h"
#include "../../../include/GameRules.h"
#include <chrono>
#include <thread>

using namespace OpenGLHIP;
using namespace VulkanHIP;

class FullSimulationE2ETest : public ::testing::Test {
protected:
    void SetUp() override {
        OpenGLWindowManager::WindowConfig windowConfig;
        windowConfig.width = 1280;
        windowConfig.height = 720;
        windowConfig.title = "E2E Test";
        windowConfig.visible = false;
        
        windowManager_.init(windowConfig);
        
        OpenGLContextConfig contextConfig;
        contextConfig.majorVersion = 4;
        contextConfig.minorVersion = 3;
        
        context_.initialize(windowManager_.getWindow(), contextConfig);
        
        if (!context_.supportsComputeShaders()) {
            GTEST_SKIP() << "Compute shaders not supported";
        }
        
        renderer_ = std::make_unique<OpenGLVoxelRenderer>();
        renderer_->initialize();
        
        imgui_.initialize(windowManager_.getWindow());
    }
    
    void TearDown() override {
        imgui_.shutdown();
        grid_.reset();
        renderer_.reset();
        context_.cleanup();
        windowManager_.cleanup();
    }
    
    OpenGLWindowManager& windowManager_ = OpenGLWindowManager::getInstance();
    OpenGLContext& context_ = OpenGLContext::getInstance();
    std::unique_ptr<Grid3D> grid_;
    std::unique_ptr<OpenGLVoxelRenderer> renderer_;
    OpenGLImGui& imgui_ = OpenGLImGui::getInstance();
};

TEST_F(FullSimulationE2ETest, CompleteSimulationCycle) {
    // Create grid
    grid_ = std::make_unique<Grid3D>(32, 32, 32);
    
    // Initialize with random pattern
    grid_->randomize(0.3f);
    
    uint64_t initialGeneration = grid_->getGeneration();
    uint64_t initialPopulation = grid_->getPopulation();
    
    // Run several simulation steps
    for (int i = 0; i < 5; ++i) {
        grid_->update();
        
        // Update renderer
        grid_->updateVoxelRenderer(*renderer_);
        
        // Small delay to simulate frame time
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    EXPECT_GT(grid_->getGeneration(), initialGeneration);
    EXPECT_GT(grid_->getGeneration(), initialGeneration + 4);
}

TEST_F(FullSimulationE2ETest, PatternLoadSave) {
    grid_ = std::make_unique<Grid3D>(16, 16, 16);
    
    // Create a simple pattern
    grid_->setCell(8, 8, 8, true);
    grid_->setCell(7, 8, 8, true);
    grid_->setCell(9, 8, 8, true);
    
    uint64_t initialPopulation = grid_->getPopulation();
    
    // Save pattern
    std::string testPatternFile = "test_pattern_e2e.pattern";
    ASSERT_TRUE(grid_->savePattern(testPatternFile));
    
    // Clear grid
    grid_->clear();
    EXPECT_EQ(grid_->getPopulation(), 0);
    
    // Load pattern back
    ASSERT_TRUE(grid_->loadPattern(testPatternFile));
    EXPECT_GE(grid_->getPopulation(), initialPopulation);
    
    // Cleanup
    std::filesystem::remove(testPatternFile);
}

TEST_F(FullSimulationE2ETest, RuleSetChanges) {
    grid_ = std::make_unique<Grid3D>(16, 16, 16);
    grid_->randomize(0.3f);
    
    uint64_t gen1 = grid_->getGeneration();
    grid_->update();
    
    // Change rule set
    grid_->setRuleSet(GameRules::RULE_HIGHLIFE);
    grid_->update();
    
    EXPECT_GT(grid_->getGeneration(), gen1);
}

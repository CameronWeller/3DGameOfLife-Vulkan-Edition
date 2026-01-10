#include <gtest/gtest.h>
#include "opengl/compute/OpenGLCompute.h"
#include "opengl/Grid3D.h"
#include "opengl/OpenGLContext.h"
#include "opengl/OpenGLWindowManager.h"
#include "../../../include/Logger.h"
#include "../../../include/GameRules.h"

using namespace OpenGLHIP;
using namespace VulkanHIP;

class ComputePipelineIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        OpenGLWindowManager::WindowConfig windowConfig;
        windowConfig.width = 800;
        windowConfig.height = 600;
        windowConfig.title = "Integration Test";
        windowConfig.visible = false;
        
        windowManager_.init(windowConfig);
        
        OpenGLContextConfig contextConfig;
        contextConfig.majorVersion = 4;
        contextConfig.minorVersion = 3;
        
        context_.initialize(windowManager_.getWindow(), contextConfig);
        
        if (!context_.supportsComputeShaders()) {
            GTEST_SKIP() << "Compute shaders not supported";
        }
    }
    
    void TearDown() override {
        grid_.reset();
        context_.cleanup();
        windowManager_.cleanup();
    }
    
    OpenGLWindowManager& windowManager_ = OpenGLWindowManager::getInstance();
    OpenGLContext& context_ = OpenGLContext::getInstance();
    std::unique_ptr<Grid3D> grid_;
};

TEST_F(ComputePipelineIntegrationTest, GridInitialization) {
    grid_ = std::make_unique<Grid3D>(16, 16, 16);
    
    EXPECT_EQ(grid_->getWidth(), 16);
    EXPECT_EQ(grid_->getHeight(), 16);
    EXPECT_EQ(grid_->getDepth(), 16);
    EXPECT_EQ(grid_->getTotalCells(), 16 * 16 * 16);
}

TEST_F(ComputePipelineIntegrationTest, GridUpdate) {
    grid_ = std::make_unique<Grid3D>(16, 16, 16);
    
    // Initialize some cells
    grid_->setCell(8, 8, 8, true);
    grid_->setCell(7, 8, 8, true);
    grid_->setCell(9, 8, 8, true);
    
    uint64_t initialGeneration = grid_->getGeneration();
    
    // Update grid
    ASSERT_NO_THROW(grid_->update());
    
    EXPECT_GT(grid_->getGeneration(), initialGeneration);
}

TEST_F(ComputePipelineIntegrationTest, ComputeShadersIntegration) {
    grid_ = std::make_unique<Grid3D>(32, 32, 32);
    
    // Check that compute shader is initialized
    auto& compute = grid_->getCompute();
    EXPECT_TRUE(compute.isInitialized());
    EXPECT_NE(compute.getProgram(), 0);
    
    // Test dispatch
    ASSERT_NO_THROW(grid_->update());
    
    // Wait for completion
    compute.waitForCompletion();
}

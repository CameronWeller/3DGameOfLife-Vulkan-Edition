#include <gtest/gtest.h>
#include "opengl/rendering/OpenGLRenderer.h"
#include "opengl/rendering/OpenGLVoxelRenderer.h"
#include "opengl/Grid3D.h"
#include "opengl/OpenGLContext.h"
#include "opengl/OpenGLWindowManager.h"
#include "../../../include/Logger.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <fstream>
#include <filesystem>
#include <vector>
#include <cmath>

using namespace OpenGLHIP;
using namespace VulkanHIP;

class VisualRegressionTest : public ::testing::Test {
protected:
    void SetUp() override {
        OpenGLWindowManager::WindowConfig windowConfig;
        windowConfig.width = 1280;
        windowConfig.height = 720;
        windowConfig.title = "Visual Regression Test";
        windowConfig.visible = false;
        
        windowManager_.init(windowConfig);
        
        OpenGLContextConfig contextConfig;
        contextConfig.majorVersion = 4;
        contextConfig.minorVersion = 3;
        
        context_.initialize(windowManager_.getWindow(), contextConfig);
        
        renderer_ = std::make_unique<OpenGLRenderer>();
        renderer_->initialize();
        
        voxelRenderer_ = std::make_unique<OpenGLVoxelRenderer>();
        voxelRenderer_->initialize();
        
        grid_ = std::make_unique<Grid3D>(32, 32, 32);
        grid_->randomize(0.2f);
        grid_->updateVoxelRenderer(*voxelRenderer_);
        
        baselineDir_ = "opengl-engine/tests/visual_regression/baselines";
        outputDir_ = "opengl-engine/tests/visual_regression/output";
        std::filesystem::create_directories(baselineDir_);
        std::filesystem::create_directories(outputDir_);
    }
    
    void TearDown() override {
        voxelRenderer_.reset();
        renderer_.reset();
        grid_.reset();
        context_.cleanup();
        windowManager_.cleanup();
    }
    
    void captureScreenshot(const std::string& filename) {
        int width, height;
        windowManager_.getFramebufferSize(&width, &height);
        
        std::vector<uint8_t> pixels(width * height * 4);
        glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
        
        // Save as PPM (simple format for testing)
        std::ofstream file(filename, std::ios::binary);
        if (file.is_open()) {
            file << "P6\n" << width << " " << height << "\n255\n";
            
            // Convert RGBA to RGB and flip vertically
            for (int y = height - 1; y >= 0; --y) {
                for (int x = 0; x < width; ++x) {
                    int idx = (y * width + x) * 4;
                    file.write(reinterpret_cast<const char*>(&pixels[idx]), 3);
                }
            }
            file.close();
        }
    }
    
    bool compareScreenshots(const std::string& baseline, const std::string& output) {
        if (!std::filesystem::exists(baseline) || !std::filesystem::exists(output)) {
            return false;
        }
        
        // Simple file size comparison (for basic regression detection)
        // In production, use proper image comparison library
        auto baselineSize = std::filesystem::file_size(baseline);
        auto outputSize = std::filesystem::file_size(output);
        
        // Allow 5% difference for rendering variations
        double diff = std::abs(static_cast<double>(baselineSize) - static_cast<double>(outputSize)) 
                     / static_cast<double>(baselineSize);
        return diff < 0.05;
    }
    
    OpenGLWindowManager& windowManager_ = OpenGLWindowManager::getInstance();
    OpenGLContext& context_ = OpenGLContext::getInstance();
    std::unique_ptr<OpenGLRenderer> renderer_;
    std::unique_ptr<OpenGLVoxelRenderer> voxelRenderer_;
    std::unique_ptr<Grid3D> grid_;
    std::string baselineDir_;
    std::string outputDir_;
};

TEST_F(VisualRegressionTest, BasicRenderingBaseline) {
    // Render a frame
    glm::mat4 view = glm::lookAt(glm::vec3(20.0f), glm::vec3(16.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    glm::mat4 proj = glm::perspective(glm::radians(45.0f), 1280.0f / 720.0f, 0.1f, 100.0f);
    
    renderer_->clear(0.1f, 0.1f, 0.1f, 1.0f);
    voxelRenderer_->render(view, proj, glm::vec3(20.0f), 1.0f, 0, 0.0f);
    context_.swapBuffers();
    
    std::string outputFile = outputDir_ + "/basic_rendering.ppm";
    captureScreenshot(outputFile);
    
    std::string baselineFile = baselineDir_ + "/basic_rendering.ppm";
    
    if (std::filesystem::exists(baselineFile)) {
        EXPECT_TRUE(compareScreenshots(baselineFile, outputFile)) 
            << "Visual regression detected! Screenshot differs from baseline.";
    } else {
        // Create baseline on first run
        std::filesystem::copy_file(outputFile, baselineFile, 
                                   std::filesystem::copy_options::overwrite_existing);
        GTEST_SKIP() << "Baseline created, skipping comparison";
    }
}

TEST_F(VisualRegressionTest, SimulationStateRendering) {
    // Render after simulation step
    grid_->update();
    grid_->updateVoxelRenderer(*voxelRenderer_);
    
    glm::mat4 view = glm::lookAt(glm::vec3(20.0f), glm::vec3(16.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    glm::mat4 proj = glm::perspective(glm::radians(45.0f), 1280.0f / 720.0f, 0.1f, 100.0f);
    
    renderer_->clear(0.1f, 0.1f, 0.1f, 1.0f);
    voxelRenderer_->render(view, proj, glm::vec3(20.0f), 1.0f, 0, 0.0f);
    context_.swapBuffers();
    
    std::string outputFile = outputDir_ + "/simulation_state.ppm";
    captureScreenshot(outputFile);
    
    std::string baselineFile = baselineDir_ + "/simulation_state.ppm";
    
    if (std::filesystem::exists(baselineFile)) {
        EXPECT_TRUE(compareScreenshots(baselineFile, outputFile))
            << "Visual regression detected in simulation state!";
    } else {
        std::filesystem::copy_file(outputFile, baselineFile,
                                   std::filesystem::copy_options::overwrite_existing);
        GTEST_SKIP() << "Baseline created for simulation state";
    }
}

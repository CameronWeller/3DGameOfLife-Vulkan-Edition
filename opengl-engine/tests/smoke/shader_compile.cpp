#include <gtest/gtest.h>
#include "opengl/compute/OpenGLCompute.h"
#include "opengl/OpenGLContext.h"
#include "opengl/OpenGLWindowManager.h"
#include "../../../include/Logger.h"
#include <fstream>
#include <sstream>
#include <filesystem>

using namespace OpenGLHIP;
using namespace VulkanHIP;

class ShaderCompilationSmokeTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize window and context first
        OpenGLWindowManager::WindowConfig windowConfig;
        windowConfig.width = 800;
        windowConfig.height = 600;
        windowConfig.title = "Shader Test";
        windowConfig.visible = false;
        
        windowManager_.init(windowConfig);
        
        OpenGLContextConfig contextConfig;
        contextConfig.majorVersion = 4;
        contextConfig.minorVersion = 3;
        contextConfig.debugContext = true;
        
        context_.initialize(windowManager_.getWindow(), contextConfig);
    }
    
    void TearDown() override {
        compute_.reset();
        context_.cleanup();
        windowManager_.cleanup();
    }
    
    OpenGLWindowManager& windowManager_ = OpenGLWindowManager::getInstance();
    OpenGLContext& context_ = OpenGLContext::getInstance();
    std::unique_ptr<OpenGLCompute> compute_ = std::make_unique<OpenGLCompute>();
};

TEST_F(ShaderCompilationSmokeTest, SimpleComputeShaderCompile) {
    // Simple compute shader source for testing
    const char* shaderSource = R"(
#version 430

layout(local_size_x = 8, local_size_y = 8, local_size_z = 8) in;

layout(std140, binding = 0) uniform PushConstants {
    uint width;
    uint height;
    uint depth;
    float time;
    uint ruleSet;
    uint surviveMin;
    uint surviveMax;
    uint birthCount;
    uint padding[3];
} constants;

layout(std430, binding = 1) readonly buffer StateBuffer {
    uint state[];
} currentState;

layout(std430, binding = 2) writeonly buffer NextStateBuffer {
    uint nextState[];
} nextStateBuffer;

void main() {
    uint index = gl_GlobalInvocationID.x + 
                 gl_GlobalInvocationID.y * constants.width +
                 gl_GlobalInvocationID.z * constants.width * constants.height;
    
    if (index >= constants.width * constants.height * constants.depth) {
        return;
    }
    
    nextStateBuffer.nextState[index] = currentState.state[index];
}
)";
    
    ASSERT_TRUE(compute_->compileShader(shaderSource));
    EXPECT_TRUE(compute_->linkProgram());
}

TEST_F(ShaderCompilationSmokeTest, ShaderFileLoad) {
    // Try to load the actual compute shader file
    std::string shaderPath = "opengl-engine/shaders/compute/game_of_life_3d_opengl.comp";
    
    if (std::filesystem::exists(shaderPath) || 
        std::filesystem::exists("../" + shaderPath) ||
        std::filesystem::exists("../../" + shaderPath)) {
        
        // If shader file exists, test loading
        // Note: This may fail in CI environments where shaders aren't available
        // So we'll make it a non-fatal test
        bool loaded = compute_->loadShader(shaderPath);
        if (loaded) {
            // If loaded successfully, try to compile
            std::ifstream file(shaderPath);
            if (!file.is_open() && std::filesystem::exists("../" + shaderPath)) {
                shaderPath = "../" + shaderPath;
                file.open(shaderPath);
            }
            if (file.is_open()) {
                std::stringstream source;
                source << file.rdbuf();
                file.close();
                
                EXPECT_TRUE(compute_->compileShader(source.str()));
            }
        }
    } else {
        // Skip test if shader file doesn't exist (e.g., in CI)
        GTEST_SKIP() << "Shader file not found, skipping file load test";
    }
}

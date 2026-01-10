#include <gtest/gtest.h>
#include "opengl/compute/OpenGLCompute.h"
#include "opengl/OpenGLContext.h"
#include "opengl/OpenGLWindowManager.h"
#include "../../../include/Logger.h"
#include <vector>

using namespace OpenGLHIP;
using namespace VulkanHIP;

class ComputePipelineUnitTest : public ::testing::Test {
protected:
    void SetUp() override {
        OpenGLWindowManager::WindowConfig windowConfig;
        windowConfig.width = 800;
        windowConfig.height = 600;
        windowConfig.title = "Compute Test";
        windowConfig.visible = false;
        
        windowManager_.init(windowConfig);
        
        OpenGLContextConfig contextConfig;
        contextConfig.majorVersion = 4;
        contextConfig.minorVersion = 3;
        
        context_.initialize(windowManager_.getWindow(), contextConfig);
        
        if (!context_.supportsComputeShaders()) {
            GTEST_SKIP() << "Compute shaders not supported, skipping compute tests";
        }
        
        compute_ = std::make_unique<OpenGLCompute>();
    }
    
    void TearDown() override {
        compute_.reset();
        context_.cleanup();
        windowManager_.cleanup();
    }
    
    OpenGLWindowManager& windowManager_ = OpenGLWindowManager::getInstance();
    OpenGLContext& context_ = OpenGLContext::getInstance();
    std::unique_ptr<OpenGLCompute> compute_;
};

TEST_F(ComputePipelineUnitTest, ShaderCompilation) {
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
    if (index < constants.width * constants.height * constants.depth) {
        nextStateBuffer.nextState[index] = currentState.state[index];
    }
}
)";
    
    EXPECT_TRUE(compute_->compileShader(shaderSource));
    EXPECT_TRUE(compute_->linkProgram());
    EXPECT_TRUE(compute_->isInitialized());
}

TEST_F(ComputePipelineUnitTest, StateBufferCreation) {
    const char* shaderSource = R"(
#version 430
layout(local_size_x = 8, local_size_y = 8, local_size_z = 8) in;
void main() {}
)";
    
    compute_->compileShader(shaderSource);
    compute_->linkProgram();
    
    ASSERT_NO_THROW(compute_->createStateBuffers(16, 16, 16));
    EXPECT_NE(&compute_->getCurrentStateBuffer(), nullptr);
    EXPECT_NE(&compute_->getNextStateBuffer(), nullptr);
}

TEST_F(ComputePipelineUnitTest, UniformBufferCreation) {
    const char* shaderSource = R"(
#version 430
layout(local_size_x = 8) in;
void main() {}
)";
    
    compute_->compileShader(shaderSource);
    compute_->linkProgram();
    
    ASSERT_NO_THROW(compute_->createUniformBuffer());
    
    ComputePushConstants constants{};
    constants.width = 32;
    constants.height = 32;
    constants.depth = 32;
    constants.time = 0.0f;
    constants.ruleSet = 0;
    
    ASSERT_NO_THROW(compute_->updatePushConstants(constants));
}

TEST_F(ComputePipelineUnitTest, StateBufferUpdate) {
    const char* shaderSource = R"(
#version 430
layout(local_size_x = 8) in;
void main() {}
)";
    
    compute_->compileShader(shaderSource);
    compute_->linkProgram();
    compute_->createStateBuffers(8, 8, 8);
    
    std::vector<uint32_t> state(8 * 8 * 8, 1);
    ASSERT_NO_THROW(compute_->updateStateBuffer(state, true));
}

TEST_F(ComputePipelineUnitTest, Dispatch) {
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
    if (index < constants.width * constants.height * constants.depth) {
        nextStateBuffer.nextState[index] = currentState.state[index];
    }
}
)";
    
    compute_->compileShader(shaderSource);
    compute_->linkProgram();
    compute_->createStateBuffers(16, 16, 16);
    compute_->createUniformBuffer();
    
    ComputePushConstants constants{};
    constants.width = 16;
    constants.height = 16;
    constants.depth = 16;
    compute_->updatePushConstants(constants);
    
    // Initialize state
    std::vector<uint32_t> initialState(16 * 16 * 16, 0);
    compute_->updateStateBuffer(initialState, true);
    
    ASSERT_NO_THROW(compute_->dispatch(16, 16, 16));
    ASSERT_NO_THROW(compute_->waitForCompletion());
}

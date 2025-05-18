#include <gtest/gtest.h>
#include "../src/VulkanEngine.h"
#include <memory>

class PipelineTest : public ::testing::Test {
protected:
    void SetUp() override {
        engine = std::make_unique<VulkanEngine>();
        engine->initWindow(800, 600, "Pipeline Test Window");
        engine->initVulkan();
        engine->createSwapChain();
        engine->createDescriptorSetLayout();
    }

    void TearDown() override {
        engine->cleanup();
    }

    std::unique_ptr<VulkanEngine> engine;
};

// Test graphics pipeline creation
TEST_F(PipelineTest, GraphicsPipelineCreationTest) {
    EXPECT_NO_THROW(engine->createGraphicsPipeline());
    EXPECT_NE(engine->getGraphicsPipeline(), VK_NULL_HANDLE);
    EXPECT_NE(engine->getPipelineLayout(), VK_NULL_HANDLE);
}

// Test pipeline layout creation
TEST_F(PipelineTest, PipelineLayoutCreationTest) {
    EXPECT_NO_THROW(engine->createPipelineLayout());
    EXPECT_NE(engine->getPipelineLayout(), VK_NULL_HANDLE);
}

// Test descriptor set layout creation
TEST_F(PipelineTest, DescriptorSetLayoutCreationTest) {
    EXPECT_NO_THROW(engine->createDescriptorSetLayout());
    EXPECT_NE(engine->getDescriptorSetLayout(), VK_NULL_HANDLE);
}

// Test pipeline recreation
TEST_F(PipelineTest, PipelineRecreationTest) {
    engine->createGraphicsPipeline();
    VkPipeline oldPipeline = engine->getGraphicsPipeline();
    
    // Simulate pipeline recreation
    engine->recreateSwapChain();
    
    EXPECT_NE(engine->getGraphicsPipeline(), VK_NULL_HANDLE);
    EXPECT_NE(engine->getGraphicsPipeline(), oldPipeline);
}

// Test shader module creation
TEST_F(PipelineTest, ShaderModuleCreationTest) {
    // Create a simple vertex shader
    const char* vertexShaderSource = R"(
        #version 450
        layout(location = 0) in vec3 inPosition;
        layout(location = 1) in vec3 inColor;
        layout(location = 0) out vec3 fragColor;
        void main() {
            gl_Position = vec4(inPosition, 1.0);
            fragColor = inColor;
        }
    )";
    
    VkShaderModule vertexShaderModule = engine->createShaderModule(vertexShaderSource, strlen(vertexShaderSource));
    EXPECT_NE(vertexShaderModule, VK_NULL_HANDLE);
    
    // Cleanup
    vkDestroyShaderModule(engine->getDevice(), vertexShaderModule, nullptr);
}

// Test pipeline cache
TEST_F(PipelineTest, PipelineCacheTest) {
    VkPipelineCacheCreateInfo cacheInfo{};
    cacheInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
    
    VkPipelineCache pipelineCache;
    EXPECT_EQ(vkCreatePipelineCache(engine->getDevice(), &cacheInfo, nullptr, &pipelineCache), VK_SUCCESS);
    EXPECT_NE(pipelineCache, VK_NULL_HANDLE);
    
    // Cleanup
    vkDestroyPipelineCache(engine->getDevice(), pipelineCache, nullptr);
}

// Test pipeline derivatives
TEST_F(PipelineTest, PipelineDerivativesTest) {
    engine->createGraphicsPipeline();
    VkPipeline basePipeline = engine->getGraphicsPipeline();
    
    // Create a derivative pipeline
    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.flags = VK_PIPELINE_CREATE_DERIVATIVE_BIT;
    pipelineInfo.basePipelineHandle = basePipeline;
    pipelineInfo.basePipelineIndex = -1;
    
    VkPipeline derivativePipeline;
    EXPECT_EQ(vkCreateGraphicsPipelines(engine->getDevice(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &derivativePipeline), VK_SUCCESS);
    EXPECT_NE(derivativePipeline, VK_NULL_HANDLE);
    
    // Cleanup
    vkDestroyPipeline(engine->getDevice(), derivativePipeline, nullptr);
}

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
} 
#include <gtest/gtest.h>
#include <memory>
#include <vector>
#include <fstream>
#include "VulkanContext.h"

class ShaderCompilationTest : public ::testing::Test {
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

// Test valid shader compilation
TEST_F(ShaderCompilationTest, ValidShaderCompilation) {
    const std::vector<std::string> shaderFiles = {
        "shaders/game_of_life.comp.spv",
        "shaders/render.vert.spv",
        "shaders/render.frag.spv"
    };

    for (const auto& shaderFile : shaderFiles) {
        // Load and create shader module
        auto shaderModule = vulkanContext->loadShaderModule(shaderFile);
        ASSERT_NE(shaderModule, VK_NULL_HANDLE) 
            << "Failed to compile valid shader: " << shaderFile;
        
        // Clean up
        vulkanContext->destroyShaderModule(shaderModule);
    }
}

// Test invalid shader compilation
TEST_F(ShaderCompilationTest, InvalidShaderCompilation) {
    const std::vector<std::string> invalidShaders = {
        "shaders/invalid.comp.spv",
        "shaders/missing.vert.spv",
        "shaders/corrupted.frag.spv"
    };

    for (const auto& shaderFile : invalidShaders) {
        // Attempt to load invalid shader
        auto shaderModule = vulkanContext->loadShaderModule(shaderFile);
        EXPECT_EQ(shaderModule, VK_NULL_HANDLE) 
            << "Should fail to compile invalid shader: " << shaderFile;
    }
}

// Test shader compilation with different optimization levels
TEST_F(ShaderCompilationTest, ShaderOptimizationLevels) {
    const std::vector<std::string> optimizationLevels = {
        "-O0",  // No optimization
        "-O1",  // Basic optimization
        "-O2",  // Full optimization
        "-O3"   // Aggressive optimization
    };

    for (const auto& optLevel : optimizationLevels) {
        // Compile shader with optimization level
        auto shaderModule = vulkanContext->compileShader(
            "shaders/game_of_life.comp",
            optLevel
        );
        ASSERT_NE(shaderModule, VK_NULL_HANDLE) 
            << "Failed to compile shader with optimization level: " << optLevel;
        
        // Clean up
        vulkanContext->destroyShaderModule(shaderModule);
    }
}

// Test shader validation
TEST_F(ShaderCompilationTest, ShaderValidation) {
    // Create a compute pipeline with the shader
    auto pipeline = vulkanContext->createComputePipeline("shaders/game_of_life.comp.spv");
    ASSERT_NE(pipeline, VK_NULL_HANDLE) << "Failed to create compute pipeline";
    
    // Verify pipeline layout
    auto pipelineLayout = vulkanContext->getPipelineLayout(pipeline);
    ASSERT_NE(pipelineLayout, VK_NULL_HANDLE) << "Failed to get pipeline layout";
    
    // Verify descriptor set layout
    auto descriptorSetLayout = vulkanContext->getDescriptorSetLayout(pipelineLayout);
    ASSERT_NE(descriptorSetLayout, VK_NULL_HANDLE) << "Failed to get descriptor set layout";
    
    // Clean up
    vulkanContext->destroyPipeline(pipeline);
}

// Test shader specialization constants
TEST_F(ShaderCompilationTest, ShaderSpecializationConstants) {
    // Define specialization constants
    struct SpecializationData {
        uint32_t workGroupSize = 8;
        uint32_t gridSize = 64;
    } specializationData;
    
    std::vector<VkSpecializationMapEntry> specializationMapEntries = {
        {0, offsetof(SpecializationData, workGroupSize), sizeof(uint32_t)},
        {1, offsetof(SpecializationData, gridSize), sizeof(uint32_t)}
    };
    
    VkSpecializationInfo specializationInfo = {
        specializationMapEntries.size(),
        specializationMapEntries.data(),
        sizeof(specializationData),
        &specializationData
    };
    
    // Create pipeline with specialization constants
    auto pipeline = vulkanContext->createComputePipeline(
        "shaders/game_of_life.comp.spv",
        &specializationInfo
    );
    ASSERT_NE(pipeline, VK_NULL_HANDLE) << "Failed to create pipeline with specialization constants";
    
    // Clean up
    vulkanContext->destroyPipeline(pipeline);
}

// Test shader compilation with different workgroup sizes
TEST_F(ShaderCompilationTest, WorkgroupSizeVariations) {
    const std::vector<uint32_t> workgroupSizes = {4, 8, 16, 32};
    
    for (uint32_t size : workgroupSizes) {
        // Create pipeline with workgroup size
        auto pipeline = vulkanContext->createComputePipeline(
            "shaders/game_of_life.comp.spv",
            size
        );
        ASSERT_NE(pipeline, VK_NULL_HANDLE) 
            << "Failed to create pipeline with workgroup size: " << size;
        
        // Clean up
        vulkanContext->destroyPipeline(pipeline);
    }
}

// Test shader compilation with different entry points
TEST_F(ShaderCompilationTest, EntryPointVariations) {
    const std::vector<std::string> entryPoints = {
        "main",
        "compute_main",
        "game_of_life_main"
    };
    
    for (const auto& entryPoint : entryPoints) {
        // Create pipeline with entry point
        auto pipeline = vulkanContext->createComputePipeline(
            "shaders/game_of_life.comp.spv",
            entryPoint
        );
        
        // Some entry points might be invalid, which is expected
        if (pipeline != VK_NULL_HANDLE) {
            vulkanContext->destroyPipeline(pipeline);
        }
    }
} 
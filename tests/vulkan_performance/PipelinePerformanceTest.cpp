#include "VulkanPerformanceTestBase.hpp"
#include <benchmark/benchmark.h>

class PipelinePerformanceTest : public VulkanPerformanceTestBase {
protected:
    void SetUp() override {
        VulkanPerformanceTestBase::SetUp();
        // Additional setup specific to pipeline testing
    }

    void TearDown() override {
        // Cleanup pipeline-specific resources
        VulkanPerformanceTestBase::TearDown();
    }
};

// Test pipeline creation performance
TEST_F(PipelinePerformanceTest, PipelineCreation) {
    // Create a simple graphics pipeline
    VkPipelineLayout pipelineLayout;
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    
    measurePipelineCreation("Simple Graphics Pipeline", [&]() {
        if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create pipeline layout");
        }
    });

    vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
}

// Benchmark pipeline creation with different configurations
static void BM_PipelineCreation(benchmark::State& state) {
    PipelinePerformanceTest test;
    test.SetUp();

    for (auto _ : state) {
        VkPipelineLayout pipelineLayout;
        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        
        if (vkCreatePipelineLayout(test.device, &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
            state.SkipWithError("Failed to create pipeline layout");
            break;
        }
        
        vkDestroyPipelineLayout(test.device, pipelineLayout, nullptr);
    }

    test.TearDown();
}
BENCHMARK(BM_PipelineCreation);

// Test pipeline binding performance
TEST_F(PipelinePerformanceTest, PipelineBinding) {
    VkCommandPool commandPool;
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.queueFamilyIndex = 0; // TODO: Use correct queue family
    
    if (vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create command pool");
    }

    VkCommandBuffer commandBuffer;
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = commandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1;

    if (vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate command buffer");
    }

    // Measure pipeline binding performance
    measurePipelineCreation("Pipeline Binding", [&]() {
        recordCommandBuffer(commandBuffer, [](VkCommandBuffer cmdBuffer) {
            // Record pipeline binding commands
            // This is a placeholder - actual pipeline binding would go here
        });
    });

    vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
    vkDestroyCommandPool(device, commandPool, nullptr);
}

// Benchmark pipeline binding with different configurations
static void BM_PipelineBinding(benchmark::State& state) {
    PipelinePerformanceTest test;
    test.SetUp();

    VkCommandPool commandPool;
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.queueFamilyIndex = 0;
    
    if (vkCreateCommandPool(test.device, &poolInfo, nullptr, &commandPool) != VK_SUCCESS) {
        state.SkipWithError("Failed to create command pool");
        test.TearDown();
        return;
    }

    VkCommandBuffer commandBuffer;
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = commandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1;

    if (vkAllocateCommandBuffers(test.device, &allocInfo, &commandBuffer) != VK_SUCCESS) {
        state.SkipWithError("Failed to allocate command buffer");
        vkDestroyCommandPool(test.device, commandPool, nullptr);
        test.TearDown();
        return;
    }

    for (auto _ : state) {
        test.recordCommandBuffer(commandBuffer, [](VkCommandBuffer cmdBuffer) {
            // Record pipeline binding commands
            // This is a placeholder - actual pipeline binding would go here
        });
    }

    vkFreeCommandBuffers(test.device, commandPool, 1, &commandBuffer);
    vkDestroyCommandPool(test.device, commandPool, nullptr);
    test.TearDown();
}
BENCHMARK(BM_PipelineBinding);

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    benchmark::Initialize(&argc, argv);
    benchmark::RunSpecifiedBenchmarks();
    return RUN_ALL_TESTS();
} 
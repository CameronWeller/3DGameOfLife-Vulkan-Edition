#include "VulkanPerformanceTestBase.hpp"
#include <benchmark/benchmark.h>

class CommandBufferPerformanceTest : public VulkanPerformanceTestBase {
protected:
    void SetUp() override {
        VulkanPerformanceTestBase::SetUp();
        // Get queue for command buffer operations
        vkGetDeviceQueue(device, 0, 0, &queue);

        // Create command pool
        VkCommandPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.queueFamilyIndex = 0; // TODO: Use correct queue family
        poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

        if (vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create command pool");
        }
    }

    void TearDown() override {
        if (commandPool != VK_NULL_HANDLE) {
            vkDestroyCommandPool(device, commandPool, nullptr);
        }
        VulkanPerformanceTestBase::TearDown();
    }

    VkCommandBuffer allocateCommandBuffer() {
        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = commandPool;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = 1;

        VkCommandBuffer commandBuffer;
        if (vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer) != VK_SUCCESS) {
            throw std::runtime_error("Failed to allocate command buffer");
        }

        return commandBuffer;
    }

protected:
    VkCommandPool commandPool = VK_NULL_HANDLE;
};

// Test command buffer allocation performance
TEST_F(CommandBufferPerformanceTest, CommandBufferAllocation) {
    measureMemoryOperation("Command Buffer Allocation", [&]() {
        VkCommandBuffer commandBuffer = allocateCommandBuffer();
        vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
    });
}

// Test command buffer recording performance
TEST_F(CommandBufferPerformanceTest, CommandBufferRecording) {
    VkCommandBuffer commandBuffer = allocateCommandBuffer();

    measurePipelineCreation("Command Buffer Recording", [&]() {
        recordCommandBuffer(commandBuffer, [](VkCommandBuffer cmdBuffer) {
            // Record some basic commands
            VkViewport viewport{};
            viewport.x = 0.0f;
            viewport.y = 0.0f;
            viewport.width = 800.0f;
            viewport.height = 600.0f;
            viewport.minDepth = 0.0f;
            viewport.maxDepth = 1.0f;
            vkCmdSetViewport(cmdBuffer, 0, 1, &viewport);

            VkRect2D scissor{};
            scissor.offset = {0, 0};
            scissor.extent = {800, 600};
            vkCmdSetScissor(cmdBuffer, 0, 1, &scissor);
        });
    });

    vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
}

// Test command buffer submission performance
TEST_F(CommandBufferPerformanceTest, CommandBufferSubmission) {
    VkCommandBuffer commandBuffer = allocateCommandBuffer();

    recordCommandBuffer(commandBuffer, [](VkCommandBuffer cmdBuffer) {
        // Record some basic commands
        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = 800.0f;
        viewport.height = 600.0f;
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        vkCmdSetViewport(cmdBuffer, 0, 1, &viewport);
    });

    measurePipelineCreation("Command Buffer Submission", [&]() {
        submitCommandBuffer(commandBuffer);
    });

    vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
}

// Benchmark command buffer allocation
static void BM_CommandBufferAllocation(benchmark::State& state) {
    CommandBufferPerformanceTest test;
    test.SetUp();

    for (auto _ : state) {
        VkCommandBuffer commandBuffer = test.allocateCommandBuffer();
        vkFreeCommandBuffers(test.device, test.commandPool, 1, &commandBuffer);
    }

    test.TearDown();
}
BENCHMARK(BM_CommandBufferAllocation);

// Benchmark command buffer recording with different command counts
static void BM_CommandBufferRecording(benchmark::State& state) {
    CommandBufferPerformanceTest test;
    test.SetUp();

    VkCommandBuffer commandBuffer = test.allocateCommandBuffer();

    for (auto _ : state) {
        test.recordCommandBuffer(commandBuffer, [&state](VkCommandBuffer cmdBuffer) {
            // Record varying number of commands
            for (int i = 0; i < state.range(0); i++) {
                VkViewport viewport{};
                viewport.x = 0.0f;
                viewport.y = 0.0f;
                viewport.width = 800.0f;
                viewport.height = 600.0f;
                viewport.minDepth = 0.0f;
                viewport.maxDepth = 1.0f;
                vkCmdSetViewport(cmdBuffer, 0, 1, &viewport);
            }
        });
    }

    vkFreeCommandBuffers(test.device, test.commandPool, 1, &commandBuffer);
    test.TearDown();
}
BENCHMARK(BM_CommandBufferRecording)->Range(1, 1000); // Test with 1 to 1000 commands

// Benchmark command buffer submission
static void BM_CommandBufferSubmission(benchmark::State& state) {
    CommandBufferPerformanceTest test;
    test.SetUp();

    VkCommandBuffer commandBuffer = test.allocateCommandBuffer();

    test.recordCommandBuffer(commandBuffer, [](VkCommandBuffer cmdBuffer) {
        // Record some basic commands
        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = 800.0f;
        viewport.height = 600.0f;
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        vkCmdSetViewport(cmdBuffer, 0, 1, &viewport);
    });

    for (auto _ : state) {
        test.submitCommandBuffer(commandBuffer);
    }

    vkFreeCommandBuffers(test.device, test.commandPool, 1, &commandBuffer);
    test.TearDown();
}
BENCHMARK(BM_CommandBufferSubmission);

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    benchmark::Initialize(&argc, argv);
    benchmark::RunSpecifiedBenchmarks();
    return RUN_ALL_TESTS();
} 
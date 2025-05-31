#include <gtest/gtest.h>
#include <vulkan/vulkan.h>
#include "VulkanPerformanceTestBase.hpp"

class CommandBufferPerformanceTest : public VulkanPerformanceTestBase {
public:
    void SetUp() override {
        VulkanPerformanceTestBase::SetUp();
        
        // Create command pool
        VkCommandPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        poolInfo.queueFamilyIndex = graphicsQueueFamily; // Use the correct graphics queue family index

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

    void TestBody() override {} // Add empty TestBody implementation

public:
    VkCommandPool commandPool = VK_NULL_HANDLE;
};

TEST_F(CommandBufferPerformanceTest, CommandBufferAllocation) {
    measureExecutionTime("Command Buffer Allocation", [this]() {
        VkCommandBuffer commandBuffer = allocateCommandBuffer();
        vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
    });
}

TEST_F(CommandBufferPerformanceTest, CommandBufferRecording) {
    VkCommandBuffer commandBuffer = allocateCommandBuffer();
    
    measureExecutionTime("Command Buffer Recording", [this, commandBuffer]() {
        recordCommandBuffer(commandBuffer, [](VkCommandBuffer cmdBuffer) {
            // Empty command buffer for now
        });
    });

    vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
}

TEST_F(CommandBufferPerformanceTest, CommandBufferSubmission) {
    VkCommandBuffer commandBuffer = allocateCommandBuffer();
    
    recordCommandBuffer(commandBuffer, [](VkCommandBuffer cmdBuffer) {
        // Empty command buffer for now
    });

    measureExecutionTime("Command Buffer Submission", [this, commandBuffer]() {
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
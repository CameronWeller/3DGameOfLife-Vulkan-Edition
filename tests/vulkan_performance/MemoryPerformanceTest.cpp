#include "VulkanPerformanceTestBase.hpp"
#include <benchmark/benchmark.h>

class MemoryPerformanceTest : public VulkanPerformanceTestBase {
protected:
    void SetUp() override {
        VulkanPerformanceTestBase::SetUp();
        // Get queue for memory operations
        vkGetDeviceQueue(device, 0, 0, &queue);
    }

    void TearDown() override {
        VulkanPerformanceTestBase::TearDown();
    }
};

// Test buffer allocation performance
TEST_F(MemoryPerformanceTest, BufferAllocation) {
    const VkDeviceSize bufferSize = 1024 * 1024; // 1MB buffer
    
    VkBuffer buffer;
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = bufferSize;
    bufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    measureMemoryOperation("Buffer Allocation", [&]() {
        if (vkCreateBuffer(device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create buffer");
        }
    });

    vkDestroyBuffer(device, buffer, nullptr);
}

// Test memory allocation and mapping
TEST_F(MemoryPerformanceTest, MemoryAllocationAndMapping) {
    const VkDeviceSize allocationSize = 1024 * 1024; // 1MB
    
    VkBuffer buffer;
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = allocationSize;
    bufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create buffer");
    }

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(device, buffer, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = 0; // TODO: Find suitable memory type

    VkDeviceMemory memory;
    measureMemoryOperation("Memory Allocation", [&]() {
        if (vkAllocateMemory(device, &allocInfo, nullptr, &memory) != VK_SUCCESS) {
            throw std::runtime_error("Failed to allocate memory");
        }
    });

    void* data;
    measureMemoryOperation("Memory Mapping", [&]() {
        if (vkMapMemory(device, memory, 0, allocationSize, 0, &data) != VK_SUCCESS) {
            throw std::runtime_error("Failed to map memory");
        }
    });

    vkUnmapMemory(device, memory);
    vkFreeMemory(device, memory, nullptr);
    vkDestroyBuffer(device, buffer, nullptr);
}

// Benchmark buffer creation with different sizes
static void BM_BufferCreation(benchmark::State& state) {
    MemoryPerformanceTest test;
    test.SetUp();

    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    for (auto _ : state) {
        bufferInfo.size = state.range(0);
        VkBuffer buffer;
        
        if (vkCreateBuffer(test.device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
            state.SkipWithError("Failed to create buffer");
            break;
        }
        
        vkDestroyBuffer(test.device, buffer, nullptr);
    }

    test.TearDown();
}
BENCHMARK(BM_BufferCreation)->Range(1<<10, 1<<30); // Test from 1KB to 1GB

// Benchmark memory allocation and mapping
static void BM_MemoryAllocationAndMapping(benchmark::State& state) {
    MemoryPerformanceTest test;
    test.SetUp();

    VkBuffer buffer;
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = state.range(0);
    bufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(test.device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
        state.SkipWithError("Failed to create buffer");
        test.TearDown();
        return;
    }

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(test.device, buffer, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = 0; // TODO: Find suitable memory type

    for (auto _ : state) {
        VkDeviceMemory memory;
        if (vkAllocateMemory(test.device, &allocInfo, nullptr, &memory) != VK_SUCCESS) {
            state.SkipWithError("Failed to allocate memory");
            break;
        }

        void* data;
        if (vkMapMemory(test.device, memory, 0, state.range(0), 0, &data) != VK_SUCCESS) {
            state.SkipWithError("Failed to map memory");
            vkFreeMemory(test.device, memory, nullptr);
            break;
        }

        vkUnmapMemory(test.device, memory);
        vkFreeMemory(test.device, memory, nullptr);
    }

    vkDestroyBuffer(test.device, buffer, nullptr);
    test.TearDown();
}
BENCHMARK(BM_MemoryAllocationAndMapping)->Range(1<<10, 1<<30); // Test from 1KB to 1GB

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    benchmark::Initialize(&argc, argv);
    benchmark::RunSpecifiedBenchmarks();
    return RUN_ALL_TESTS();
} 
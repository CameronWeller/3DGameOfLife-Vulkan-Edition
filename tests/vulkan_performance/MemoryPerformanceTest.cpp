#include <gtest/gtest.h>
#include <vulkan/vulkan.h>
#include "VulkanPerformanceTestBase.hpp"
#include <benchmark/benchmark.h>

class MemoryPerformanceTest : public VulkanPerformanceTestBase {
public:
    void SetUp() override {
        VulkanPerformanceTestBase::SetUp();
    }

    void TearDown() override {
        VulkanPerformanceTestBase::TearDown();
    }

    void TestBody() override {} // Add empty TestBody implementation
};

TEST_F(MemoryPerformanceTest, BufferAllocation) {
    const VkDeviceSize bufferSize = 1024 * 1024; // 1MB

    measureExecutionTime("Buffer Allocation", [this, bufferSize]() {
        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = bufferSize;
        bufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VkBuffer buffer;
        if (vkCreateBuffer(device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create buffer");
        }

        vkDestroyBuffer(device, buffer, nullptr);
    });
}

TEST_F(MemoryPerformanceTest, MemoryAllocation) {
    const VkDeviceSize bufferSize = 1024 * 1024; // 1MB

    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = bufferSize;
    bufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VkBuffer buffer;
    if (vkCreateBuffer(device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create buffer");
    }

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(device, buffer, &memRequirements);

    measureExecutionTime("Memory Allocation", [this, &memRequirements]() {
        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;
        // Find appropriate memory type for device local memory
        allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        VkDeviceMemory memory;
        if (vkAllocateMemory(device, &allocInfo, nullptr, &memory) != VK_SUCCESS) {
            throw std::runtime_error("Failed to allocate memory");
        }

        vkFreeMemory(device, memory, nullptr);
    });

    vkDestroyBuffer(device, buffer, nullptr);
}

TEST_F(MemoryPerformanceTest, MemoryMapping) {
    const VkDeviceSize bufferSize = 1024 * 1024; // 1MB

    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = bufferSize;
    bufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VkBuffer buffer;
    if (vkCreateBuffer(device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create buffer");
    }

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(device, buffer, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    // Find memory type that is host visible and coherent for mapping
    allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, 
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    VkDeviceMemory memory;
    if (vkAllocateMemory(device, &allocInfo, nullptr, &memory) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate memory");
    }

    vkBindBufferMemory(device, buffer, memory, 0);

    measureExecutionTime("Memory Mapping", [this, memory, bufferSize]() {
        void* data;
        if (vkMapMemory(device, memory, 0, bufferSize, 0, &data) != VK_SUCCESS) {
            throw std::runtime_error("Failed to map memory");
        }

        // Write some data
        memset(data, 0, bufferSize);

        vkUnmapMemory(device, memory);
    });

    vkDestroyBuffer(device, buffer, nullptr);
    vkFreeMemory(device, memory, nullptr);
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
    // Find memory type suitable for host mapping
    allocInfo.memoryTypeIndex = test.findMemoryType(memRequirements.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

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
#include <benchmark/benchmark.h>
#include "VulkanEngine.h"
#include <memory>

// Benchmark for engine initialization
static void BM_EngineInitialization(benchmark::State& state) {
    for (auto _ : state) {
        auto engine = std::make_unique<VulkanEngine>();
        engine->initialize();
        benchmark::DoNotOptimize(engine);
    }
}
BENCHMARK(BM_EngineInitialization);

// Benchmark for window creation
static void BM_WindowCreation(benchmark::State& state) {
    auto engine = std::make_unique<VulkanEngine>();
    engine->initialize();
    
    for (auto _ : state) {
        engine->createWindow(800, 600, "Benchmark Window");
        benchmark::DoNotOptimize(engine);
    }
}
BENCHMARK(BM_WindowCreation);

// Benchmark for shader compilation
static void BM_ShaderCompilation(benchmark::State& state) {
    auto engine = std::make_unique<VulkanEngine>();
    engine->initialize();
    
    for (auto _ : state) {
        engine->compileShader("shaders/basic.vert", VK_SHADER_STAGE_VERTEX_BIT);
        benchmark::DoNotOptimize(engine);
    }
}
BENCHMARK(BM_ShaderCompilation);

// Benchmark for buffer allocation
static void BM_BufferAllocation(benchmark::State& state) {
    auto engine = std::make_unique<VulkanEngine>();
    engine->initialize();
    
    for (auto _ : state) {
        VkBuffer buffer;
        VkDeviceMemory memory;
        engine->allocateBuffer(1024, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, &buffer, &memory);
        engine->freeBuffer(buffer, memory);
        benchmark::DoNotOptimize(engine);
    }
}
BENCHMARK(BM_BufferAllocation);

// Benchmark for command buffer operations
static void BM_CommandBufferOperations(benchmark::State& state) {
    auto engine = std::make_unique<VulkanEngine>();
    engine->initialize();
    
    for (auto _ : state) {
        VkCommandBuffer cmdBuffer;
        engine->createCommandBuffer(&cmdBuffer);
        engine->beginCommandBuffer(cmdBuffer);
        engine->endCommandBuffer(cmdBuffer);
        engine->freeCommandBuffer(cmdBuffer);
        benchmark::DoNotOptimize(engine);
    }
}
BENCHMARK(BM_CommandBufferOperations);

// Benchmark for synchronization primitives
static void BM_SynchronizationPrimitives(benchmark::State& state) {
    auto engine = std::make_unique<VulkanEngine>();
    engine->initialize();
    
    for (auto _ : state) {
        VkSemaphore semaphore;
        VkFence fence;
        engine->createSemaphore(&semaphore);
        engine->createFence(&fence);
        engine->destroySemaphore(semaphore);
        engine->destroyFence(fence);
        benchmark::DoNotOptimize(engine);
    }
}
BENCHMARK(BM_SynchronizationPrimitives);

// Benchmark for memory operations
static void BM_MemoryOperations(benchmark::State& state) {
    auto engine = std::make_unique<VulkanEngine>();
    engine->initialize();
    
    const size_t bufferSize = 1024 * 1024; // 1MB
    std::vector<uint8_t> data(bufferSize, 0);
    
    for (auto _ : state) {
        VkBuffer buffer;
        VkDeviceMemory memory;
        engine->allocateBuffer(bufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, &buffer, &memory);
        engine->mapMemory(memory, 0, bufferSize, 0, &data[0]);
        engine->unmapMemory(memory);
        engine->freeBuffer(buffer, memory);
        benchmark::DoNotOptimize(engine);
    }
}
BENCHMARK(BM_MemoryOperations);

// Benchmark for pipeline creation
static void BM_PipelineCreation(benchmark::State& state) {
    auto engine = std::make_unique<VulkanEngine>();
    engine->initialize();
    engine->createWindow(800, 600, "Benchmark Window");
    
    for (auto _ : state) {
        engine->createGraphicsPipeline();
        benchmark::DoNotOptimize(engine);
    }
}
BENCHMARK(BM_PipelineCreation);

// Benchmark for descriptor set operations
static void BM_DescriptorSetOperations(benchmark::State& state) {
    auto engine = std::make_unique<VulkanEngine>();
    engine->initialize();
    
    for (auto _ : state) {
        VkDescriptorSetLayout layout;
        VkDescriptorSet descriptorSet;
        engine->createDescriptorSetLayout(&layout);
        engine->allocateDescriptorSet(layout, &descriptorSet);
        engine->freeDescriptorSet(descriptorSet);
        engine->destroyDescriptorSetLayout(layout);
        benchmark::DoNotOptimize(engine);
    }
}
BENCHMARK(BM_DescriptorSetOperations);

// Benchmark for render pass operations
static void BM_RenderPassOperations(benchmark::State& state) {
    auto engine = std::make_unique<VulkanEngine>();
    engine->initialize();
    engine->createWindow(800, 600, "Benchmark Window");
    
    for (auto _ : state) {
        VkRenderPass renderPass;
        engine->createRenderPass(&renderPass);
        engine->destroyRenderPass(renderPass);
        benchmark::DoNotOptimize(engine);
    }
}
BENCHMARK(BM_RenderPassOperations);

BENCHMARK_MAIN(); 
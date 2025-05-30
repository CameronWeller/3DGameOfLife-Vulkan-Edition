#include <benchmark/benchmark.h>
#include "VulkanMemoryManager.h"
#include "VulkanContext.h"
#include <memory>

class VulkanMemoryManagerFixture : public benchmark::Fixture {
public:
    void SetUp(const ::benchmark::State& state) override {
        context = std::make_unique<VulkanContext>();
        context->initialize();
        memoryManager = std::make_unique<VulkanMemoryManager>(context.get());
    }
    void TearDown(const ::benchmark::State& state) override {
        memoryManager.reset();
        if (context) context->cleanup();
        context.reset();
    }
    std::unique_ptr<VulkanContext> context;
    std::unique_ptr<VulkanMemoryManager> memoryManager;
};

BENCHMARK_DEFINE_F(VulkanMemoryManagerFixture, BM_AllocateAndFreeBuffer)(benchmark::State& state) {
    for (auto _ : state) {
        VkBuffer buffer = VK_NULL_HANDLE;
        VkDeviceMemory memory = VK_NULL_HANDLE;
        VkDeviceSize size = 4096;
        VkBufferUsageFlags usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
        VkMemoryPropertyFlags properties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        bool result = memoryManager->allocateBuffer(size, usage, properties, buffer, memory);
        benchmark::DoNotOptimize(result);
        memoryManager->freeBuffer(buffer, memory);
    }
}
BENCHMARK_REGISTER_F(VulkanMemoryManagerFixture, BM_AllocateAndFreeBuffer)->Unit(benchmark::kMicrosecond)->Iterations(1000);

BENCHMARK_DEFINE_F(VulkanMemoryManagerFixture, BM_AllocateAndFreeImage)(benchmark::State& state) {
    for (auto _ : state) {
        VkImage image = VK_NULL_HANDLE;
        VkDeviceMemory memory = VK_NULL_HANDLE;
        VkExtent3D extent = {128, 128, 1};
        VkFormat format = VK_FORMAT_R8G8B8A8_UNORM;
        VkImageUsageFlags usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        VkMemoryPropertyFlags properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
        bool result = memoryManager->allocateImage(extent, format, usage, properties, image, memory);
        benchmark::DoNotOptimize(result);
        memoryManager->freeImage(image, memory);
    }
}
BENCHMARK_REGISTER_F(VulkanMemoryManagerFixture, BM_AllocateAndFreeImage)->Unit(benchmark::kMicrosecond)->Iterations(1000);

BENCHMARK_MAIN(); 
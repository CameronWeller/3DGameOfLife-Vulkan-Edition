#include <benchmark/benchmark.h>
#include "VulkanContext.h"
#include "WindowManager.h"
#include <vector>
#include <GLFW/glfw3.h>
#include <memory>

class VulkanContextFixture : public benchmark::Fixture {
public:
    void SetUp(const ::benchmark::State& state) override {
        if (!glfwInit()) {
            throw std::runtime_error("Failed to initialize GLFW");
        }
        
        WindowManager::WindowConfig config;
        config.width = 800;
        config.height = 600;
        config.title = "Vulkan Benchmark Window";
        WindowManager::getInstance().init(config);
    }

    void TearDown(const ::benchmark::State& state) override {
        WindowManager::getInstance().cleanup();
        glfwTerminate();
    }
};

BENCHMARK_DEFINE_F(VulkanContextFixture, BM_VulkanContextInitialization)(benchmark::State& state) {
    std::vector<const char*> requiredExtensions = {
        VK_KHR_SURFACE_EXTENSION_NAME,
        VK_KHR_WIN32_SURFACE_EXTENSION_NAME
    };
    
    for (auto _ : state) {
        auto& context = VulkanContext::getInstance();
        context.init(requiredExtensions);
        context.cleanup();
    }
}
BENCHMARK_REGISTER_F(VulkanContextFixture, BM_VulkanContextInitialization)
    ->Unit(benchmark::kMillisecond)
    ->Iterations(10);

BENCHMARK_DEFINE_F(VulkanContextFixture, BM_DeviceCreation)(benchmark::State& state) {
    std::vector<const char*> requiredExtensions = {
        VK_KHR_SURFACE_EXTENSION_NAME,
        VK_KHR_WIN32_SURFACE_EXTENSION_NAME
    };
    
    auto& context = VulkanContext::getInstance();
    context.init(requiredExtensions);
    
    for (auto _ : state) {
        // Test device feature queries
        VkPhysicalDeviceFeatures deviceFeatures;
        vkGetPhysicalDeviceFeatures(context.getPhysicalDevice(), &deviceFeatures);
        benchmark::DoNotOptimize(deviceFeatures);
        
        // Test device property queries
        VkPhysicalDeviceProperties deviceProperties;
        vkGetPhysicalDeviceProperties(context.getPhysicalDevice(), &deviceProperties);
        benchmark::DoNotOptimize(deviceProperties);
        
        // Test memory property queries
        VkPhysicalDeviceMemoryProperties memoryProperties;
        vkGetPhysicalDeviceMemoryProperties(context.getPhysicalDevice(), &memoryProperties);
        benchmark::DoNotOptimize(memoryProperties);
    }
    
    context.cleanup();
}
BENCHMARK_REGISTER_F(VulkanContextFixture, BM_DeviceCreation)
    ->Unit(benchmark::kMicrosecond)
    ->Iterations(1000);

BENCHMARK_DEFINE_F(VulkanContextFixture, BM_QueueFamilySelection)(benchmark::State& state) {
    std::vector<const char*> requiredExtensions = {
        VK_KHR_SURFACE_EXTENSION_NAME,
        VK_KHR_WIN32_SURFACE_EXTENSION_NAME
    };
    
    auto& context = VulkanContext::getInstance();
    context.init(requiredExtensions);
    
    for (auto _ : state) {
        // Test queue family queries
        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(context.getPhysicalDevice(), &queueFamilyCount, nullptr);
        std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(context.getPhysicalDevice(), &queueFamilyCount, queueFamilies.data());
        benchmark::DoNotOptimize(queueFamilies);
        
        // Test queue family index finding
        QueueFamilyIndices indices = context.findQueueFamilies(context.getPhysicalDevice());
        benchmark::DoNotOptimize(indices);
    }
    
    context.cleanup();
}
BENCHMARK_REGISTER_F(VulkanContextFixture, BM_QueueFamilySelection)
    ->Unit(benchmark::kMicrosecond)
    ->Iterations(1000);

BENCHMARK_DEFINE_F(VulkanContextFixture, BM_ValidationLayerSetup)(benchmark::State& state) {
    std::vector<const char*> requiredExtensions = {
        VK_KHR_SURFACE_EXTENSION_NAME,
        VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
        VK_EXT_DEBUG_UTILS_EXTENSION_NAME
    };
    
    auto& context = VulkanContext::getInstance();
    
    for (auto _ : state) {
        ValidationLayerConfig config;
        config.enabled = true;
        config.layers = {"VK_LAYER_KHRONOS_validation"};
        config.errorMessages = true;
        config.warningMessages = true;
        
        context.setValidationLayerConfig(config);
        context.init(requiredExtensions);
        context.cleanup();
    }
}
BENCHMARK_REGISTER_F(VulkanContextFixture, BM_ValidationLayerSetup)
    ->Unit(benchmark::kMillisecond)
    ->Iterations(10);

static void BM_VulkanContextInitialization(benchmark::State& state) {
    for (auto _ : state) {
        auto context = std::make_unique<VulkanContext>();
        benchmark::DoNotOptimize(context);
    }
}
BENCHMARK(BM_VulkanContextInitialization);

static void BM_CommandPoolCreation(benchmark::State& state) {
    auto context = std::make_unique<VulkanContext>();
    for (auto _ : state) {
        auto commandPool = context->createCommandPool();
        benchmark::DoNotOptimize(commandPool);
        context->destroyCommandPool(commandPool);
    }
}
BENCHMARK(BM_CommandPoolCreation);

static void BM_DevicePropertiesQuery(benchmark::State& state) {
    auto context = std::make_unique<VulkanContext>();
    for (auto _ : state) {
        auto properties = context->getDeviceProperties();
        benchmark::DoNotOptimize(properties);
    }
}
BENCHMARK(BM_DevicePropertiesQuery);

static void BM_QueueFamilyIndicesQuery(benchmark::State& state) {
    auto context = std::make_unique<VulkanContext>();
    for (auto _ : state) {
        auto indices = context->getQueueFamilyIndices();
        benchmark::DoNotOptimize(indices);
    }
}
BENCHMARK(BM_QueueFamilyIndicesQuery);

static void BM_MemoryAllocation(benchmark::State& state) {
    auto context = std::make_unique<VulkanContext>();
    VkMemoryRequirements memRequirements;
    // Set up memory requirements (this is a placeholder - actual implementation will depend on your memory allocation strategy)
    memRequirements.size = 1024;
    memRequirements.alignment = 256;
    memRequirements.memoryTypeBits = 0xFFFFFFFF;

    for (auto _ : state) {
        auto memory = context->allocateMemory(memRequirements, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
        benchmark::DoNotOptimize(memory);
        context->freeMemory(memory);
    }
}
BENCHMARK(BM_MemoryAllocation);

BENCHMARK_MAIN(); 
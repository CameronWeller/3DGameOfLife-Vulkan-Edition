#include <benchmark/benchmark.h>
#include "WindowManager.h"
#include <GLFW/glfw3.h>
#include <memory>

class WindowManagerFixture : public benchmark::Fixture {
public:
    void SetUp(const ::benchmark::State& state) override {
        if (!glfwInit()) {
            throw std::runtime_error("Failed to initialize GLFW");
        }
    }

    void TearDown(const ::benchmark::State& state) override {
        WindowManager::getInstance().cleanup();
        glfwTerminate();
    }
};

BENCHMARK_DEFINE_F(WindowManagerFixture, BM_WindowCreation)(benchmark::State& state) {
    WindowManager::WindowConfig config;
    config.width = 800;
    config.height = 600;
    config.title = "Benchmark Window";
    
    for (auto _ : state) {
        WindowManager::getInstance().init(config);
        WindowManager::getInstance().cleanup();
    }
}
BENCHMARK_REGISTER_F(WindowManagerFixture, BM_WindowCreation)
    ->Unit(benchmark::kMillisecond)
    ->Iterations(10);

BENCHMARK_DEFINE_F(WindowManagerFixture, BM_WindowResize)(benchmark::State& state) {
    WindowManager::WindowConfig config;
    config.width = 800;
    config.height = 600;
    config.title = "Resize Benchmark";
    WindowManager::getInstance().init(config);
    
    for (auto _ : state) {
        WindowManager::getInstance().setWindowSize(1024, 768);
        WindowManager::getInstance().setWindowSize(800, 600);
    }
}
BENCHMARK_REGISTER_F(WindowManagerFixture, BM_WindowResize)
    ->Unit(benchmark::kMicrosecond)
    ->Iterations(1000);

BENCHMARK_DEFINE_F(WindowManagerFixture, BM_WindowPosition)(benchmark::State& state) {
    WindowManager::WindowConfig config;
    config.width = 800;
    config.height = 600;
    config.title = "Position Benchmark";
    WindowManager::getInstance().init(config);
    
    for (auto _ : state) {
        WindowManager::getInstance().setWindowPos(100, 100);
        WindowManager::getInstance().setWindowPos(0, 0);
    }
}
BENCHMARK_REGISTER_F(WindowManagerFixture, BM_WindowPosition)
    ->Unit(benchmark::kMicrosecond)
    ->Iterations(1000);

BENCHMARK_DEFINE_F(WindowManagerFixture, BM_SurfaceCreation)(benchmark::State& state) {
    WindowManager::WindowConfig config;
    config.width = 800;
    config.height = 600;
    config.title = "Surface Benchmark";
    WindowManager::getInstance().init(config);
    
    // Create a minimal Vulkan instance
    VkInstance instance;
    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    
    uint32_t extensionCount = 0;
    const char** extensions = glfwGetRequiredInstanceExtensions(&extensionCount);
    createInfo.enabledExtensionCount = extensionCount;
    createInfo.ppEnabledExtensionNames = extensions;
    
    vkCreateInstance(&createInfo, nullptr, &instance);
    
    for (auto _ : state) {
        VkSurfaceKHR surface = WindowManager::getInstance().createSurface(instance);
        WindowManager::getInstance().destroySurface(instance, surface);
    }
    
    vkDestroyInstance(instance, nullptr);
}
BENCHMARK_REGISTER_F(WindowManagerFixture, BM_SurfaceCreation)
    ->Unit(benchmark::kMicrosecond)
    ->Iterations(100);

BENCHMARK_DEFINE_F(WindowManagerFixture, BM_EventPolling)(benchmark::State& state) {
    WindowManager::WindowConfig config;
    config.width = 800;
    config.height = 600;
    config.title = "Event Polling Benchmark";
    WindowManager::getInstance().init(config);
    
    for (auto _ : state) {
        WindowManager::getInstance().pollEvents();
    }
}
BENCHMARK_REGISTER_F(WindowManagerFixture, BM_EventPolling)
    ->Unit(benchmark::kMicrosecond)
    ->Iterations(1000);

BENCHMARK_DEFINE_F(WindowManagerFixture, BM_CallbackRegistration)(benchmark::State& state) {
    WindowManager::WindowConfig config;
    config.width = 800;
    config.height = 600;
    config.title = "Callback Benchmark";
    WindowManager::getInstance().init(config);
    
    for (auto _ : state) {
        WindowManager::getInstance().setKeyCallback([](int, int, int, int){});
        WindowManager::getInstance().setMouseButtonCallback([](int, int, int){});
        WindowManager::getInstance().setCursorPosCallback([](double, double){});
        WindowManager::getInstance().setFramebufferResizeCallback([](int, int){});
    }
}
BENCHMARK_REGISTER_F(WindowManagerFixture, BM_CallbackRegistration)
    ->Unit(benchmark::kMicrosecond)
    ->Iterations(1000);

BENCHMARK_MAIN(); 
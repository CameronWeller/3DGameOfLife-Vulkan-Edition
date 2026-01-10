#include <benchmark/benchmark.h>
#include "opengl/rendering/OpenGLRenderer.h"
#include "opengl/rendering/OpenGLVoxelRenderer.h"
#include "opengl/Grid3D.h"
#include "opengl/OpenGLContext.h"
#include "opengl/OpenGLWindowManager.h"
#include "opengl/diagnostics/OpenGLProfiler.h"
#include "../../../include/Logger.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

using namespace OpenGLHIP;
using namespace VulkanHIP;

class FrameRateBenchmark : public benchmark::Fixture {
public:
    void SetUp(const benchmark::State& state) override {
        OpenGLWindowManager::WindowConfig windowConfig;
        windowConfig.width = 1280;
        windowConfig.height = 720;
        windowConfig.title = "Frame Rate Benchmark";
        windowConfig.visible = false;
        
        windowManager_.init(windowConfig);
        
        OpenGLContextConfig contextConfig;
        contextConfig.majorVersion = 4;
        contextConfig.minorVersion = 3;
        
        context_.initialize(windowManager_.getWindow(), contextConfig);
        
        renderer_ = std::make_unique<OpenGLRenderer>();
        renderer_->initialize();
        
        voxelRenderer_ = std::make_unique<OpenGLVoxelRenderer>();
        voxelRenderer_->initialize();
        
        gridSize_ = static_cast<uint32_t>(state.range(0));
        grid_ = std::make_unique<Grid3D>(gridSize_, gridSize_, gridSize_);
        grid_->randomize(0.2f);
        
        grid_->updateVoxelRenderer(*voxelRenderer_);
        
        profiler_.initialize();
    }
    
    void TearDown(const benchmark::State&) override {
        profiler_.shutdown();
        voxelRenderer_.reset();
        renderer_.reset();
        grid_.reset();
        context_.cleanup();
        windowManager_.cleanup();
    }
    
    OpenGLWindowManager& windowManager_ = OpenGLWindowManager::getInstance();
    OpenGLContext& context_ = OpenGLContext::getInstance();
    std::unique_ptr<OpenGLRenderer> renderer_;
    std::unique_ptr<OpenGLVoxelRenderer> voxelRenderer_;
    std::unique_ptr<Grid3D> grid_;
    OpenGLProfiler& profiler_ = OpenGLProfiler::getInstance();
    uint32_t gridSize_ = 32;
};

BENCHMARK_DEFINE_F(FrameRateBenchmark, FullFrameRender)(benchmark::State& state) {
    glm::mat4 view = glm::lookAt(glm::vec3(20.0f), glm::vec3(gridSize_ / 2.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    glm::mat4 proj = glm::perspective(glm::radians(45.0f), 1280.0f / 720.0f, 0.1f, 100.0f);
    
    for (auto _ : state) {
        profiler_.beginFrame();
        
        renderer_->beginFrame();
        renderer_->clear(0.1f, 0.1f, 0.1f, 1.0f);
        
        voxelRenderer_->render(view, proj, glm::vec3(20.0f), 1.0f, 0, 0.0f);
        
        renderer_->endFrame();
        context_.swapBuffers();
        
        profiler_.endFrame();
    }
    
    state.SetItemsProcessed(state.iterations());
    auto stats = profiler_.getStats();
    state.counters["avgFPS"] = stats.avgFPS;
    state.counters["minFPS"] = stats.minFPS;
    state.counters["maxFPS"] = stats.maxFPS;
}

BENCHMARK_REGISTER_F(FrameRateBenchmark, FullFrameRender)
    ->RangeMultiplier(2)
    ->Range(32, 128)
    ->Unit(benchmark::kMillisecond)
    ->Iterations(60);

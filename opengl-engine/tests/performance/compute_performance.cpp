#include <benchmark/benchmark.h>
#include "opengl/compute/OpenGLCompute.h"
#include "opengl/Grid3D.h"
#include "opengl/OpenGLContext.h"
#include "opengl/OpenGLWindowManager.h"
#include "../../../include/Logger.h"
#include <vector>

using namespace OpenGLHIP;
using namespace VulkanHIP;

class ComputePerformanceFixture : public benchmark::Fixture {
public:
    void SetUp(const benchmark::State& state) override {
        OpenGLWindowManager::WindowConfig windowConfig;
        windowConfig.width = 800;
        windowConfig.height = 600;
        windowConfig.title = "Performance Test";
        windowConfig.visible = false;
        
        windowManager_.init(windowConfig);
        
        OpenGLContextConfig contextConfig;
        contextConfig.majorVersion = 4;
        contextConfig.minorVersion = 3;
        
        context_.initialize(windowManager_.getWindow(), contextConfig);
        
        if (!context_.supportsComputeShaders()) {
            GTEST_SKIP() << "Compute shaders not supported";
        }
        
        gridSize_ = static_cast<uint32_t>(state.range(0));
        grid_ = std::make_unique<Grid3D>(gridSize_, gridSize_, gridSize_);
        grid_->randomize(0.3f);
    }
    
    void TearDown(const benchmark::State&) override {
        grid_.reset();
        context_.cleanup();
        windowManager_.cleanup();
    }
    
    OpenGLWindowManager& windowManager_ = OpenGLWindowManager::getInstance();
    OpenGLContext& context_ = OpenGLContext::getInstance();
    std::unique_ptr<Grid3D> grid_;
    uint32_t gridSize_ = 32;
};

BENCHMARK_DEFINE_F(ComputePerformanceFixture, GridUpdate)(benchmark::State& state) {
    for (auto _ : state) {
        grid_->update();
        auto& compute = grid_->getCompute();
        compute.waitForCompletion();
    }
    
    state.SetItemsProcessed(state.iterations() * gridSize_ * gridSize_ * gridSize_);
    state.SetBytesProcessed(state.iterations() * gridSize_ * gridSize_ * gridSize_ * sizeof(uint32_t) * 2);
}

BENCHMARK_REGISTER_F(ComputePerformanceFixture, GridUpdate)
    ->RangeMultiplier(2)
    ->Range(16, 128)
    ->Unit(benchmark::kMillisecond);

BENCHMARK_DEFINE_F(ComputePerformanceFixture, ComputeDispatch)(benchmark::State& state) {
    auto& compute = grid_->getCompute();
    
    ComputePushConstants constants{};
    constants.width = gridSize_;
    constants.height = gridSize_;
    constants.depth = gridSize_;
    constants.ruleSet = 0;
    compute.updatePushConstants(constants);
    
    for (auto _ : state) {
        compute.dispatch(gridSize_, gridSize_, gridSize_);
        compute.waitForCompletion();
    }
    
    state.SetItemsProcessed(state.iterations() * gridSize_ * gridSize_ * gridSize_);
}

BENCHMARK_REGISTER_F(ComputePerformanceFixture, ComputeDispatch)
    ->RangeMultiplier(2)
    ->Range(16, 128)
    ->Unit(benchmark::kMillisecond);

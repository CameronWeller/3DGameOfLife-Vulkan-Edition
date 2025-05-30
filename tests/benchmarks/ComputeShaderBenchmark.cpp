#include <benchmark/benchmark.h>
#include <memory>
#include "VulkanContext.h"
#include "ComputeShader.h"

class ComputeShaderBenchmark : public benchmark::Fixture {
protected:
    void SetUp(const benchmark::State& state) override {
        vulkanContext = std::make_unique<VulkanContext>();
        ASSERT_TRUE(vulkanContext->initialize());
        
        computeShader = std::make_unique<ComputeShader>(vulkanContext.get());
        ASSERT_TRUE(computeShader->initialize());
    }

    void TearDown(const benchmark::State& state) override {
        computeShader.reset();
        vulkanContext.reset();
    }

    std::unique_ptr<VulkanContext> vulkanContext;
    std::unique_ptr<ComputeShader> computeShader;
};

// Benchmark compute shader execution with different workgroup sizes
BENCHMARK_DEFINE_F(ComputeShaderBenchmark, WorkgroupSize)(benchmark::State& state) {
    const int gridSize = 128;
    const int workgroupSize = state.range(0);
    
    computeShader->setWorkgroupSize(workgroupSize, workgroupSize, workgroupSize);
    computeShader->setGridSize(gridSize, gridSize, gridSize);
    
    for (auto _ : state) {
        computeShader->execute();
        vulkanContext->waitForCompute();
    }
    
    state.SetItemsProcessed(state.iterations() * gridSize * gridSize * gridSize);
    state.SetBytesProcessed(state.iterations() * gridSize * gridSize * gridSize * sizeof(float));
}

BENCHMARK_REGISTER_F(ComputeShaderBenchmark, WorkgroupSize)
    ->RangeMultiplier(2)
    ->Range(4, 32)
    ->Unit(benchmark::kMicrosecond);

// Benchmark compute shader with different grid sizes
BENCHMARK_DEFINE_F(ComputeShaderBenchmark, GridSize)(benchmark::State& state) {
    const int gridSize = state.range(0);
    const int workgroupSize = 8;  // Optimal workgroup size
    
    computeShader->setWorkgroupSize(workgroupSize, workgroupSize, workgroupSize);
    computeShader->setGridSize(gridSize, gridSize, gridSize);
    
    for (auto _ : state) {
        computeShader->execute();
        vulkanContext->waitForCompute();
    }
    
    state.SetItemsProcessed(state.iterations() * gridSize * gridSize * gridSize);
    state.SetBytesProcessed(state.iterations() * gridSize * gridSize * gridSize * sizeof(float));
}

BENCHMARK_REGISTER_F(ComputeShaderBenchmark, GridSize)
    ->RangeMultiplier(2)
    ->Range(32, 256)
    ->Unit(benchmark::kMicrosecond);

// Benchmark memory transfer performance
BENCHMARK_DEFINE_F(ComputeShaderBenchmark, MemoryTransfer)(benchmark::State& state) {
    const int gridSize = 128;
    const int bufferSize = gridSize * gridSize * gridSize * sizeof(float);
    
    std::vector<float> inputData(bufferSize / sizeof(float), 1.0f);
    std::vector<float> outputData(bufferSize / sizeof(float));
    
    computeShader->setGridSize(gridSize, gridSize, gridSize);
    
    for (auto _ : state) {
        computeShader->uploadData(inputData.data(), bufferSize);
        computeShader->execute();
        computeShader->downloadData(outputData.data(), bufferSize);
        vulkanContext->waitForCompute();
    }
    
    state.SetBytesProcessed(state.iterations() * bufferSize * 2);  // Upload and download
}

BENCHMARK_REGISTER_F(ComputeShaderBenchmark, MemoryTransfer)
    ->Unit(benchmark::kMicrosecond);

// Benchmark multiple compute passes
BENCHMARK_DEFINE_F(ComputeShaderBenchmark, MultiplePasses)(benchmark::State& state) {
    const int gridSize = 128;
    const int numPasses = state.range(0);
    
    computeShader->setGridSize(gridSize, gridSize, gridSize);
    
    for (auto _ : state) {
        for (int i = 0; i < numPasses; ++i) {
            computeShader->execute();
        }
        vulkanContext->waitForCompute();
    }
    
    state.SetItemsProcessed(state.iterations() * numPasses * gridSize * gridSize * gridSize);
}

BENCHMARK_REGISTER_F(ComputeShaderBenchmark, MultiplePasses)
    ->RangeMultiplier(2)
    ->Range(1, 16)
    ->Unit(benchmark::kMicrosecond);

// Benchmark compute shader with different rule sets
BENCHMARK_DEFINE_F(ComputeShaderBenchmark, RuleSets)(benchmark::State& state) {
    const int gridSize = 128;
    const std::string ruleSet = state.range(0) == 0 ? "5766" : "4555";
    
    computeShader->setGridSize(gridSize, gridSize, gridSize);
    computeShader->setRuleSet(ruleSet);
    
    for (auto _ : state) {
        computeShader->execute();
        vulkanContext->waitForCompute();
    }
    
    state.SetItemsProcessed(state.iterations() * gridSize * gridSize * gridSize);
}

BENCHMARK_REGISTER_F(ComputeShaderBenchmark, RuleSets)
    ->Arg(0)  // 5766 rule set
    ->Arg(1)  // 4555 rule set
    ->Unit(benchmark::kMicrosecond);

BENCHMARK_MAIN(); 
#include <benchmark/benchmark.h>
#include "SaveManager.h"
#include <string>
#include <random>
#include <filesystem>

class SaveManagerFixture : public benchmark::Fixture {
public:
    void SetUp(const ::benchmark::State& state) override {
        testFile = "benchmark_save_file.sav";
    }
    void TearDown(const ::benchmark::State& state) override {
        std::filesystem::remove(testFile);
    }
    std::string testFile;
};

// Helper function to generate random data of specified size
std::string generateRandomData(size_t size) {
    std::string data;
    data.reserve(size);
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 255);
    
    for (size_t i = 0; i < size; ++i) {
        data.push_back(static_cast<char>(dis(gen)));
    }
    return data;
}

BENCHMARK_DEFINE_F(SaveManagerFixture, BM_SaveSmallData)(benchmark::State& state) {
    SaveManager manager;
    std::string data = generateRandomData(1024); // 1KB
    
    for (auto _ : state) {
        bool result = manager.saveToFile(testFile, data);
        benchmark::DoNotOptimize(result);
    }
}
BENCHMARK_REGISTER_F(SaveManagerFixture, BM_SaveSmallData)->Unit(benchmark::kMicrosecond)->Iterations(1000);

BENCHMARK_DEFINE_F(SaveManagerFixture, BM_SaveLargeData)(benchmark::State& state) {
    SaveManager manager;
    std::string data = generateRandomData(1024 * 1024); // 1MB
    
    for (auto _ : state) {
        bool result = manager.saveToFile(testFile, data);
        benchmark::DoNotOptimize(result);
    }
}
BENCHMARK_REGISTER_F(SaveManagerFixture, BM_SaveLargeData)->Unit(benchmark::kMicrosecond)->Iterations(100);

BENCHMARK_DEFINE_F(SaveManagerFixture, BM_LoadSmallData)(benchmark::State& state) {
    SaveManager manager;
    std::string data = generateRandomData(1024); // 1KB
    manager.saveToFile(testFile, data);
    
    for (auto _ : state) {
        std::string loaded;
        bool result = manager.loadFromFile(testFile, loaded);
        benchmark::DoNotOptimize(result);
        benchmark::DoNotOptimize(loaded);
    }
}
BENCHMARK_REGISTER_F(SaveManagerFixture, BM_LoadSmallData)->Unit(benchmark::kMicrosecond)->Iterations(1000);

BENCHMARK_DEFINE_F(SaveManagerFixture, BM_LoadLargeData)(benchmark::State& state) {
    SaveManager manager;
    std::string data = generateRandomData(1024 * 1024); // 1MB
    manager.saveToFile(testFile, data);
    
    for (auto _ : state) {
        std::string loaded;
        bool result = manager.loadFromFile(testFile, loaded);
        benchmark::DoNotOptimize(result);
        benchmark::DoNotOptimize(loaded);
    }
}
BENCHMARK_REGISTER_F(SaveManagerFixture, BM_LoadLargeData)->Unit(benchmark::kMicrosecond)->Iterations(100); 
#include <benchmark/benchmark.h>
#include "RayCaster.h"
#include <random>

class RayCasterFixture : public benchmark::Fixture {
public:
    void SetUp(const ::benchmark::State& state) override {
        // Set up a 100x100x100 grid for benchmarking
        gridMin = glm::vec3(0.0f);
        gridMax = glm::vec3(100.0f);
        voxelSize = 1.0f;

        // Initialize random number generator
        rng.seed(42);
    }

    glm::vec3 gridMin;
    glm::vec3 gridMax;
    float voxelSize;
    std::mt19937 rng;

    // Helper function to generate random ray
    RayCaster::Ray generateRandomRay() {
        std::uniform_real_distribution<float> posDist(-200.0f, 200.0f);
        std::uniform_real_distribution<float> dirDist(-1.0f, 1.0f);

        RayCaster::Ray ray;
        ray.origin = glm::vec3(posDist(rng), posDist(rng), posDist(rng));
        
        // Generate random direction and normalize
        glm::vec3 dir(dirDist(rng), dirDist(rng), dirDist(rng));
        ray.direction = glm::normalize(dir);
        
        return ray;
    }

    // Helper function to generate random world position
    glm::vec3 generateRandomWorldPos() {
        std::uniform_real_distribution<float> posDist(-200.0f, 200.0f);
        return glm::vec3(posDist(rng), posDist(rng), posDist(rng));
    }
};

BENCHMARK_DEFINE_F(RayCasterFixture, BM_RayCast)(benchmark::State& state) {
    for (auto _ : state) {
        RayCaster::Ray ray = generateRandomRay();
        benchmark::DoNotOptimize(RayCaster::castRay(ray, gridMin, gridMax));
    }
}
BENCHMARK_REGISTER_F(RayCasterFixture, BM_RayCast)
    ->Unit(benchmark::kMicrosecond)
    ->Iterations(1000);

BENCHMARK_DEFINE_F(RayCasterFixture, BM_RayCastHit)(benchmark::State& state) {
    // Generate rays that are guaranteed to hit the grid
    std::vector<RayCaster::Ray> rays;
    for (int i = 0; i < 1000; ++i) {
        RayCaster::Ray ray;
        ray.origin = glm::vec3(-10.0f, 50.0f, 50.0f);
        ray.direction = glm::normalize(glm::vec3(1.0f, 0.0f, 0.0f));
        rays.push_back(ray);
    }

    size_t rayIndex = 0;
    for (auto _ : state) {
        benchmark::DoNotOptimize(RayCaster::castRay(rays[rayIndex], gridMin, gridMax));
        rayIndex = (rayIndex + 1) % rays.size();
    }
}
BENCHMARK_REGISTER_F(RayCasterFixture, BM_RayCastHit)
    ->Unit(benchmark::kMicrosecond)
    ->Iterations(1000);

BENCHMARK_DEFINE_F(RayCasterFixture, BM_RayCastMiss)(benchmark::State& state) {
    // Generate rays that are guaranteed to miss the grid
    std::vector<RayCaster::Ray> rays;
    for (int i = 0; i < 1000; ++i) {
        RayCaster::Ray ray;
        ray.origin = glm::vec3(-10.0f, 50.0f, 50.0f);
        ray.direction = glm::normalize(glm::vec3(-1.0f, 0.0f, 0.0f));
        rays.push_back(ray);
    }

    size_t rayIndex = 0;
    for (auto _ : state) {
        benchmark::DoNotOptimize(RayCaster::castRay(rays[rayIndex], gridMin, gridMax));
        rayIndex = (rayIndex + 1) % rays.size();
    }
}
BENCHMARK_REGISTER_F(RayCasterFixture, BM_RayCastMiss)
    ->Unit(benchmark::kMicrosecond)
    ->Iterations(1000);

BENCHMARK_DEFINE_F(RayCasterFixture, BM_GridPositionConversion)(benchmark::State& state) {
    for (auto _ : state) {
        glm::vec3 worldPos = generateRandomWorldPos();
        benchmark::DoNotOptimize(RayCaster::getGridPosition(worldPos, voxelSize));
    }
}
BENCHMARK_REGISTER_F(RayCasterFixture, BM_GridPositionConversion)
    ->Unit(benchmark::kMicrosecond)
    ->Iterations(1000);

BENCHMARK_DEFINE_F(RayCasterFixture, BM_WorldPositionConversion)(benchmark::State& state) {
    std::uniform_int_distribution<int> gridDist(0, 100);
    for (auto _ : state) {
        glm::vec3 gridPos(gridDist(rng), gridDist(rng), gridDist(rng));
        benchmark::DoNotOptimize(RayCaster::getWorldPosition(gridPos, voxelSize));
    }
}
BENCHMARK_REGISTER_F(RayCasterFixture, BM_WorldPositionConversion)
    ->Unit(benchmark::kMicrosecond)
    ->Iterations(1000);

BENCHMARK_DEFINE_F(RayCasterFixture, BM_RayCastParallel)(benchmark::State& state) {
    // Test rays parallel to grid faces
    std::vector<RayCaster::Ray> rays;
    std::vector<glm::vec3> directions = {
        glm::vec3(1.0f, 0.0f, 0.0f),
        glm::vec3(0.0f, 1.0f, 0.0f),
        glm::vec3(0.0f, 0.0f, 1.0f)
    };

    for (const auto& dir : directions) {
        RayCaster::Ray ray;
        ray.origin = glm::vec3(50.0f, 50.0f, 50.0f);
        ray.direction = dir;
        rays.push_back(ray);
    }

    size_t rayIndex = 0;
    for (auto _ : state) {
        benchmark::DoNotOptimize(RayCaster::castRay(rays[rayIndex], gridMin, gridMax));
        rayIndex = (rayIndex + 1) % rays.size();
    }
}
BENCHMARK_REGISTER_F(RayCasterFixture, BM_RayCastParallel)
    ->Unit(benchmark::kMicrosecond)
    ->Iterations(1000);

BENCHMARK_DEFINE_F(RayCasterFixture, BM_RayCastPrecision)(benchmark::State& state) {
    // Test rays with high precision requirements
    std::vector<RayCaster::Ray> rays;
    for (int i = 0; i < 1000; ++i) {
        RayCaster::Ray ray;
        ray.origin = glm::vec3(-10.0f, 50.0f, 50.0f);
        ray.direction = glm::normalize(glm::vec3(1.0f, 0.0001f, 0.0001f));
        rays.push_back(ray);
    }

    size_t rayIndex = 0;
    for (auto _ : state) {
        benchmark::DoNotOptimize(RayCaster::castRay(rays[rayIndex], gridMin, gridMax));
        rayIndex = (rayIndex + 1) % rays.size();
    }
}
BENCHMARK_REGISTER_F(RayCasterFixture, BM_RayCastPrecision)
    ->Unit(benchmark::kMicrosecond)
    ->Iterations(1000);

BENCHMARK_DEFINE_F(RayCasterFixture, BM_RayCastAllFaces)(benchmark::State& state) {
    // Test rays hitting all faces of the grid
    std::vector<RayCaster::Ray> rays;
    std::vector<std::pair<glm::vec3, glm::vec3>> rayTests = {
        {glm::vec3(-10.0f, 50.0f, 50.0f), glm::vec3(1.0f, 0.0f, 0.0f)},   // Left face
        {glm::vec3(110.0f, 50.0f, 50.0f), glm::vec3(-1.0f, 0.0f, 0.0f)},  // Right face
        {glm::vec3(50.0f, -10.0f, 50.0f), glm::vec3(0.0f, 1.0f, 0.0f)},   // Bottom face
        {glm::vec3(50.0f, 110.0f, 50.0f), glm::vec3(0.0f, -1.0f, 0.0f)},  // Top face
        {glm::vec3(50.0f, 50.0f, -10.0f), glm::vec3(0.0f, 0.0f, 1.0f)},   // Front face
        {glm::vec3(50.0f, 50.0f, 110.0f), glm::vec3(0.0f, 0.0f, -1.0f)}   // Back face
    };

    for (const auto& test : rayTests) {
        RayCaster::Ray ray;
        ray.origin = test.first;
        ray.direction = glm::normalize(test.second);
        rays.push_back(ray);
    }

    size_t rayIndex = 0;
    for (auto _ : state) {
        benchmark::DoNotOptimize(RayCaster::castRay(rays[rayIndex], gridMin, gridMax));
        rayIndex = (rayIndex + 1) % rays.size();
    }
}
BENCHMARK_REGISTER_F(RayCasterFixture, BM_RayCastAllFaces)
    ->Unit(benchmark::kMicrosecond)
    ->Iterations(1000);

BENCHMARK_MAIN(); 
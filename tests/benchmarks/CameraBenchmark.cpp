#include <benchmark/benchmark.h>
#include "Camera.h"
#include <GLFW/glfw3.h>
#include <memory>

class CameraFixture : public benchmark::Fixture {
public:
    void SetUp(const ::benchmark::State& state) override {
        if (!glfwInit()) {
            throw std::runtime_error("Failed to initialize GLFW");
        }
        window = glfwCreateWindow(800, 600, "Camera Benchmark", nullptr, nullptr);
        if (!window) {
            glfwTerminate();
            throw std::runtime_error("Failed to create GLFW window");
        }
        camera = std::make_unique<Camera>(window, 45.0f, 0.1f, 1000.0f);
    }

    void TearDown(const ::benchmark::State& state) override {
        camera.reset();
        glfwDestroyWindow(window);
        glfwTerminate();
    }

    GLFWwindow* window;
    std::unique_ptr<Camera> camera;
};

BENCHMARK_DEFINE_F(CameraFixture, BM_ViewMatrixCalculation)(benchmark::State& state) {
    for (auto _ : state) {
        benchmark::DoNotOptimize(camera->getViewMatrix());
    }
}
BENCHMARK_REGISTER_F(CameraFixture, BM_ViewMatrixCalculation)
    ->Unit(benchmark::kMicrosecond)
    ->Iterations(1000);

BENCHMARK_DEFINE_F(CameraFixture, BM_ProjectionMatrixCalculation)(benchmark::State& state) {
    for (auto _ : state) {
        benchmark::DoNotOptimize(camera->getProjectionMatrix());
    }
}
BENCHMARK_REGISTER_F(CameraFixture, BM_ProjectionMatrixCalculation)
    ->Unit(benchmark::kMicrosecond)
    ->Iterations(1000);

BENCHMARK_DEFINE_F(CameraFixture, BM_Movement)(benchmark::State& state) {
    for (auto _ : state) {
        camera->moveForward(1.0f);
        camera->moveRight(1.0f);
        camera->moveUp(1.0f);
    }
}
BENCHMARK_REGISTER_F(CameraFixture, BM_Movement)
    ->Unit(benchmark::kMicrosecond)
    ->Iterations(1000);

BENCHMARK_DEFINE_F(CameraFixture, BM_Rotation)(benchmark::State& state) {
    for (auto _ : state) {
        camera->rotate(1.0f, 1.0f);
    }
}
BENCHMARK_REGISTER_F(CameraFixture, BM_Rotation)
    ->Unit(benchmark::kMicrosecond)
    ->Iterations(1000);

BENCHMARK_DEFINE_F(CameraFixture, BM_MouseInputProcessing)(benchmark::State& state) {
    for (auto _ : state) {
        camera->processMouseMovement(1.0f, 1.0f, true);
    }
}
BENCHMARK_REGISTER_F(CameraFixture, BM_MouseInputProcessing)
    ->Unit(benchmark::kMicrosecond)
    ->Iterations(1000);

BENCHMARK_DEFINE_F(CameraFixture, BM_Zoom)(benchmark::State& state) {
    for (auto _ : state) {
        camera->processMouseScroll(1.0f);
    }
}
BENCHMARK_REGISTER_F(CameraFixture, BM_Zoom)
    ->Unit(benchmark::kMicrosecond)
    ->Iterations(1000);

BENCHMARK_DEFINE_F(CameraFixture, BM_OrbitMode)(benchmark::State& state) {
    camera->setMode(CameraMode::Orbit);
    for (auto _ : state) {
        camera->orbit(1.0f, 1.0f);
    }
}
BENCHMARK_REGISTER_F(CameraFixture, BM_OrbitMode)
    ->Unit(benchmark::kMicrosecond)
    ->Iterations(1000);

BENCHMARK_DEFINE_F(CameraFixture, BM_PanMode)(benchmark::State& state) {
    camera->setMode(CameraMode::Pan);
    for (auto _ : state) {
        camera->pan(1.0f, 1.0f);
    }
}
BENCHMARK_REGISTER_F(CameraFixture, BM_PanMode)
    ->Unit(benchmark::kMicrosecond)
    ->Iterations(1000);

BENCHMARK_DEFINE_F(CameraFixture, BM_CameraModeSwitch)(benchmark::State& state) {
    for (auto _ : state) {
        camera->setMode(CameraMode::Fly);
        camera->setMode(CameraMode::Orbit);
        camera->setMode(CameraMode::Pan);
        camera->setMode(CameraMode::FirstPerson);
    }
}
BENCHMARK_REGISTER_F(CameraFixture, BM_CameraModeSwitch)
    ->Unit(benchmark::kMicrosecond)
    ->Iterations(1000);

BENCHMARK_DEFINE_F(CameraFixture, BM_CameraVectorUpdates)(benchmark::State& state) {
    for (auto _ : state) {
        camera->updateCameraVectors();
    }
}
BENCHMARK_REGISTER_F(CameraFixture, BM_CameraVectorUpdates)
    ->Unit(benchmark::kMicrosecond)
    ->Iterations(1000);

BENCHMARK_DEFINE_F(CameraFixture, BM_OrbitPositionUpdate)(benchmark::State& state) {
    camera->setMode(CameraMode::Orbit);
    for (auto _ : state) {
        camera->updateOrbitPosition();
    }
}
BENCHMARK_REGISTER_F(CameraFixture, BM_OrbitPositionUpdate)
    ->Unit(benchmark::kMicrosecond)
    ->Iterations(1000);

BENCHMARK_MAIN(); 
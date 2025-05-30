cmake_minimum_required(VERSION 3.20)

# Enable testing
enable_testing()

# Find required testing packages
find_package(GTest REQUIRED)
find_package(Benchmark REQUIRED)
find_package(Threads REQUIRED)

# Add test executables
add_subdirectory(unit)
add_subdirectory(integration)
add_subdirectory(benchmarks)
add_subdirectory(performance)
add_subdirectory(vulkan_performance)

# Add custom targets for different types of tests
add_custom_target(run-unit-tests
    COMMAND ctest -R "unit.*" --output-on-failure
    DEPENDS unit_tests
    COMMENT "Running unit tests"
)

add_custom_target(run-integration-tests
    COMMAND ctest -R "integration.*" --output-on-failure
    DEPENDS integration_tests
    COMMENT "Running integration tests"
)

add_custom_target(run-benchmarks
    COMMAND ctest -R "benchmark.*" --output-on-failure
    DEPENDS benchmark_tests
    COMMENT "Running benchmarks"
)

add_custom_target(run-performance-tests
    COMMAND ctest -R "performance.*" --output-on-failure
    DEPENDS performance_tests
    COMMENT "Running performance tests"
)

add_custom_target(run-vulkan-performance-tests
    COMMAND ctest -R "vulkan_performance.*" --output-on-failure
    DEPENDS vulkan_performance_tests
    COMMENT "Running Vulkan performance tests"
)

# Add a target to run all tests
add_custom_target(run-all-tests
    COMMAND ctest --output-on-failure
    DEPENDS unit_tests integration_tests benchmark_tests performance_tests vulkan_performance_tests
    COMMENT "Running all tests"
)

# Find required packages
find_package(Vulkan REQUIRED)
find_package(HIP QUIET)

# Add test executable
add_executable(engine_tests
    VulkanEngineTest.cpp
    MemoryTest.cpp
    UtilityTest.cpp
    PipelineTest.cpp
    SwapchainTest.cpp
    QueueFamilyTest.cpp
)

target_include_directories(engine_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/tests
    ${Vulkan_INCLUDE_DIRS}
)

target_link_libraries(engine_tests PRIVATE
    GTest::GTest
    GTest::Main
    ${Vulkan_LIBRARIES}
    Threads::Threads
    cpp-vulkan-hip-engine
)

# Enable test discovery
gtest_discover_tests(engine_tests)

# Add compiler warnings
target_compile_options(engine_tests PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -Werror
)

# Add test coverage if available
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(engine_tests PRIVATE --coverage)
    target_link_options(engine_tests PRIVATE --coverage)
endif() 
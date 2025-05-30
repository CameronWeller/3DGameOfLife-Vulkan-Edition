# Benchmarks CMakeLists.txt

# Create a benchmark executable
add_executable(benchmark_tests
    VulkanEngineBenchmark.cpp
    VulkanContextBenchmark.cpp
    MemoryManagerBenchmark.cpp
)

# Link against Google Benchmark and the main project
target_link_libraries(benchmark_tests PRIVATE
    benchmark::benchmark
    benchmark::benchmark_main
    cpp-vulkan-hip-engine
)

# Include directories
target_include_directories(benchmark_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/include
)

# Add benchmark test
add_test(NAME benchmark_tests COMMAND benchmark_tests)

# Set test properties
set_tests_properties(benchmark_tests PROPERTIES
    ENVIRONMENT "BENCHMARK_COLOR=1"
    TIMEOUT 600
) 
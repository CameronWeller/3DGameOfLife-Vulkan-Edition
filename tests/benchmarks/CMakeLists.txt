# Benchmarks CMakeLists.txt

# Find Google Benchmark package
find_package(benchmark REQUIRED)

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

# Add benchmark executable
add_executable(compute_shader_benchmark
    ComputeShaderBenchmark.cpp
)

# Link against required libraries
target_link_libraries(compute_shader_benchmark PRIVATE
    benchmark::benchmark
    benchmark::benchmark_main
    cpp-vulkan-hip-engine
)

# Include directories
target_include_directories(compute_shader_benchmark PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/include
)

# Add compiler warnings
target_compile_options(compute_shader_benchmark PRIVATE
    -Wall
    -Wextra
    -Wpedantic
)

# Add benchmark target
add_custom_target(run-benchmarks
    COMMAND compute_shader_benchmark
    DEPENDS compute_shader_benchmark
    COMMENT "Running compute shader benchmarks"
)

# Add benchmark target with specific options
add_custom_target(run-benchmarks-detailed
    COMMAND compute_shader_benchmark
        --benchmark_format=json
        --benchmark_out=benchmark_results.json
        --benchmark_out_format=json
        --benchmark_counters_tabular=true
        --benchmark_repetitions=3
        --benchmark_report_aggregates_only=false
    DEPENDS compute_shader_benchmark
    COMMENT "Running detailed compute shader benchmarks"
)

# Add benchmark target with memory profiling
add_custom_target(run-benchmarks-memory
    COMMAND compute_shader_benchmark
        --benchmark_format=json
        --benchmark_out=benchmark_memory_results.json
        --benchmark_out_format=json
        --benchmark_counters_tabular=true
        --benchmark_repetitions=3
        --benchmark_report_aggregates_only=false
        --benchmark_memory_usage=true
    DEPENDS compute_shader_benchmark
    COMMENT "Running memory profiling benchmarks"
) 
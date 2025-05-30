# Vulkan Performance Tests CMakeLists.txt

# Create a performance test executable
add_executable(vulkan_performance_tests
    PipelinePerformanceTest.cpp
    MemoryPerformanceTest.cpp
    ShaderPerformanceTest.cpp
    CommandBufferPerformanceTest.cpp
)

# Link against required libraries
target_link_libraries(vulkan_performance_tests PRIVATE
    GTest::GTest
    GTest::Main
    benchmark::benchmark
    benchmark::benchmark_main
    cpp-vulkan-hip-engine
    Vulkan::Vulkan
)

# Include directories
target_include_directories(vulkan_performance_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/include
    ${Vulkan_INCLUDE_DIRS}
)

# Add test
add_test(NAME vulkan_performance_tests COMMAND vulkan_performance_tests)

# Set test properties
set_tests_properties(vulkan_performance_tests PROPERTIES
    ENVIRONMENT "GTEST_COLOR=1"
    TIMEOUT 600
)

# Add benchmark visualization library
add_library(vulkan_benchmark_visualizer INTERFACE)
target_include_directories(vulkan_benchmark_visualizer INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_sources(vulkan_benchmark_visualizer INTERFACE BenchmarkVisualizer.hpp)

# Add test executables
add_executable(vulkan_pipeline_tests PipelinePerformanceTest.cpp)
add_executable(vulkan_memory_tests MemoryPerformanceTest.cpp)
add_executable(vulkan_shader_tests ShaderPerformanceTest.cpp)
add_executable(vulkan_command_buffer_tests CommandBufferPerformanceTest.cpp)

# Link test executables with required libraries
foreach(test_target
    vulkan_pipeline_tests
    vulkan_memory_tests
    vulkan_shader_tests
    vulkan_command_buffer_tests
)
    target_link_libraries(${test_target}
        PRIVATE
        vulkan_benchmark_visualizer
        GTest::gtest
        GTest::gtest_main
        benchmark::benchmark
        benchmark::benchmark_main
        Vulkan::Vulkan
    )

    # Include directories
    target_include_directories(${test_target} PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/include
        ${Vulkan_INCLUDE_DIRS}
    )

    # Add test
    add_test(NAME ${test_target} COMMAND ${test_target})

    # Set test properties
    set_tests_properties(${test_target} PROPERTIES
        ENVIRONMENT "GTEST_COLOR=1"
        TIMEOUT 600
    )
endforeach()

# Copy test runner script to build directory
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/run_performance_tests.py
    ${CMAKE_BINARY_DIR}/run_performance_tests.py
    COPYONLY
)

# Make the script executable
if(UNIX)
    execute_process(
        COMMAND chmod +x ${CMAKE_BINARY_DIR}/run_performance_tests.py
    )
endif()

# Add custom target to run all performance tests
add_custom_target(run_vulkan_performance_tests
    COMMAND ${CMAKE_BINARY_DIR}/run_performance_tests.py
        --build-dir ${CMAKE_BINARY_DIR}
        --output-dir ${CMAKE_BINARY_DIR}/benchmark_results
    DEPENDS
        vulkan_pipeline_tests
        vulkan_memory_tests
        vulkan_shader_tests
        vulkan_command_buffer_tests
    COMMENT "Running all Vulkan performance tests and generating reports"
) 
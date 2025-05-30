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
# Testing Configuration
# This module sets up testing infrastructure for the project

include(CTest)
include(GoogleTest)
find_package(benchmark CONFIG REQUIRED)

# Enable testing
enable_testing()

# Set test properties
set(GTEST_DISCOVER_TESTS_TIMEOUT 120)

# Function to add tests
function(add_project_tests)
    # Add test executable
    add_executable(${PROJECT_NAME}_tests
        tests/main.cpp
        tests/VulkanEngineTest.cpp
        tests/VulkanContextTest.cpp
        tests/WindowManagerTest.cpp
        tests/CameraTest.cpp
        tests/RayCasterTest.cpp
        tests/VulkanMemoryManagerTest.cpp
        tests/SaveManagerTest.cpp
        tests/LoggerTest.cpp
    )

    # Link test executable with project library and testing frameworks
    target_link_libraries(${PROJECT_NAME}_tests PRIVATE
        ${PROJECT_NAME}
        GTest::gtest
        GTest::gtest_main
        benchmark::benchmark
    )

    # Include directories for tests
    target_include_directories(${PROJECT_NAME}_tests PRIVATE
        ${CMAKE_SOURCE_DIR}/tests
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/include
    )

    # Discover and add tests
    gtest_discover_tests(${PROJECT_NAME}_tests
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        PROPERTIES
            ENVIRONMENT "GTEST_COLOR=1"
            TIMEOUT 120
    )

    # Add benchmark tests
    add_executable(${PROJECT_NAME}_benchmarks
        tests/benchmarks/main.cpp
        tests/benchmarks/VulkanEngineBenchmark.cpp
        tests/benchmarks/VulkanContextBenchmark.cpp
        tests/benchmarks/WindowManagerBenchmark.cpp
        tests/benchmarks/CameraBenchmark.cpp
        tests/benchmarks/RayCasterBenchmark.cpp
        tests/benchmarks/VulkanMemoryManagerBenchmark.cpp
        tests/benchmarks/SaveManagerBenchmark.cpp
        tests/benchmarks/LoggerBenchmark.cpp
    )

    # Link benchmark executable
    target_link_libraries(${PROJECT_NAME}_benchmarks PRIVATE
        ${PROJECT_NAME}
        benchmark::benchmark
        benchmark::benchmark_main
    )

    # Include directories for benchmarks
    target_include_directories(${PROJECT_NAME}_benchmarks PRIVATE
        ${CMAKE_SOURCE_DIR}/tests/benchmarks
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/include
    )

    # Add benchmark tests
    add_custom_target(run_benchmarks
        COMMAND ${PROJECT_NAME}_benchmarks
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        COMMENT "Running benchmarks"
    )
endfunction()

# Add test coverage if enabled
option(ENABLE_COVERAGE "Enable test coverage reporting" OFF)
if(ENABLE_COVERAGE)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(${PROJECT_NAME}_tests PRIVATE
            --coverage
            -fprofile-arcs
            -ftest-coverage
        )
        target_link_options(${PROJECT_NAME}_tests PRIVATE
            --coverage
        )
    endif()
endif() 
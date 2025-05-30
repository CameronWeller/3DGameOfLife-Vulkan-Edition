# Testing Configuration Module
# This module handles all testing setup and configuration

include(CTest)
include(GoogleTest)

# Enable testing if requested
if(BUILD_TESTING)
    enable_testing()
    
    # Set test properties
    set_property(GLOBAL PROPERTY CTEST_TARGETS_ADDED 1)
    
    # Test timeout (in seconds)
    set(TEST_TIMEOUT 300)
    
    # Test output on failure
    set(CTEST_OUTPUT_ON_FAILURE ON)
endif()

# Function to add unit tests
function(add_unit_tests)
    if(NOT BUILD_TESTING)
        return()
    endif()
    
    # Find all unit test source files
    file(GLOB UNIT_TEST_SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/unit/*.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/CameraTest.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/LoggerTest.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/SaveManagerTest.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/RayCasterTest.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/UtilityTest.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/VulkanEngineTest.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/VulkanContextTest.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/VulkanMemoryManagerTest.cpp"
    )
    
    add_executable(unit_tests 
        ${UNIT_TEST_SOURCES}
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/main.cpp"
    )
    
    target_link_libraries(unit_tests PRIVATE
        ${PROJECT_NAME}_lib
        testing_dependencies
    )
    
    apply_common_settings(unit_tests)
    
    # Discover and add tests
    gtest_discover_tests(unit_tests
        TIMEOUT ${TEST_TIMEOUT}
        PROPERTIES
            LABELS "unit"
    )
endfunction()

# Function to add integration tests
function(add_integration_tests)
    if(NOT BUILD_TESTING)
        return()
    endif()
    
    # Find all integration test source files
    file(GLOB INTEGRATION_TEST_SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/*.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_vulkan_integration.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/PipelineTest.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/QueueFamilyTest.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/SwapchainTest.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/WindowManagerTest.cpp"
    )
    
    add_executable(integration_tests 
        ${INTEGRATION_TEST_SOURCES}
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/main.cpp"
    )
    
    target_link_libraries(integration_tests PRIVATE
        ${PROJECT_NAME}_lib
        testing_dependencies
    )
    
    apply_common_settings(integration_tests)
    
    # Discover and add tests
    gtest_discover_tests(integration_tests
        TIMEOUT ${TEST_TIMEOUT}
        PROPERTIES
            LABELS "integration"
    )
endfunction()

# Function to add benchmark tests
function(add_benchmark_tests)
    if(NOT BUILD_TESTING)
        return()
    endif()
    
    # Find all benchmark source files
    file(GLOB BENCHMARK_SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/benchmarks/*.cpp"
    )
    
    # Remove main.cpp from sources as we'll add it separately
    list(FILTER BENCHMARK_SOURCES EXCLUDE REGEX ".*main\.cpp$")
    
    add_executable(benchmark_tests 
        ${BENCHMARK_SOURCES}
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/benchmarks/main.cpp"
    )
    
    target_link_libraries(benchmark_tests PRIVATE
        ${PROJECT_NAME}_lib
        testing_dependencies
    )
    
    apply_common_settings(benchmark_tests)
    
    # Add benchmark as test
    add_test(NAME benchmark_tests COMMAND benchmark_tests)
    set_tests_properties(benchmark_tests PROPERTIES
        TIMEOUT ${TEST_TIMEOUT}
        LABELS "benchmark"
    )
endfunction()

# Function to add performance tests
function(add_performance_tests)
    if(NOT BUILD_TESTING)
        return()
    endif()
    
    # Find all performance test source files
    file(GLOB PERFORMANCE_TEST_SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/performance/*.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/vulkan_performance/*.cpp"
    )
    
    # Exclude Python scripts
    list(FILTER PERFORMANCE_TEST_SOURCES EXCLUDE REGEX ".*\.py$")
    
    add_executable(performance_tests ${PERFORMANCE_TEST_SOURCES})
    
    target_link_libraries(performance_tests PRIVATE
        ${PROJECT_NAME}_lib
        testing_dependencies
    )
    
    apply_common_settings(performance_tests)
    
    # Add performance test as test
    add_test(NAME performance_tests COMMAND performance_tests)
    set_tests_properties(performance_tests PROPERTIES
        TIMEOUT ${TEST_TIMEOUT}
        LABELS "performance"
    )
    
    # Create performance test targets
    add_custom_target(run_benchmarks
        COMMAND benchmark_tests --benchmark_out=benchmark_results.json --benchmark_out_format=json
        DEPENDS benchmark_tests
        COMMENT "Running performance benchmarks"
    )
    
    add_custom_target(run_benchmarks_detailed
        COMMAND benchmark_tests --benchmark_counters_tabular=true --benchmark_out=benchmark_detailed.json --benchmark_out_format=json
        DEPENDS benchmark_tests
        COMMENT "Running detailed performance benchmarks"
    )
    
    add_custom_target(run_memory_benchmarks
        COMMAND benchmark_tests --benchmark_memory_usage=true --benchmark_out=memory_benchmark.json --benchmark_out_format=json
        DEPENDS benchmark_tests
        COMMENT "Running memory usage benchmarks"
    )
    
    # Add Vulkan-specific performance tests
    add_custom_target(run_vulkan_performance
        COMMAND performance_tests
        DEPENDS performance_tests
        COMMENT "Running Vulkan performance tests"
    )
endfunction()

# Function to add memory leak tests
function(add_memory_leak_tests)
    if(NOT BUILD_TESTING)
        return()
    endif()
    
    # Find all memory leak test source files
    file(GLOB MEMORY_LEAK_TEST_SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/MemoryLeakTest.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/MemoryTest.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/unit/MemoryLeakTest.cpp"
    )
    
    add_executable(memory_leak_tests 
        ${MEMORY_LEAK_TEST_SOURCES}
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/main.cpp"
    )
    
    target_link_libraries(memory_leak_tests PRIVATE
        ${PROJECT_NAME}_lib
        testing_dependencies
    )
    
    apply_common_settings(memory_leak_tests)
    
    # Discover and add tests
    gtest_discover_tests(memory_leak_tests
        TIMEOUT ${TEST_TIMEOUT}
        PROPERTIES
            LABELS "memory_leak"
    )
endfunction()

# Function to add test coverage
function(add_test_coverage)
    if(NOT BUILD_TESTING OR NOT ENABLE_COVERAGE)
        return()
    endif()
    
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        # Add coverage flags
        target_compile_options(${PROJECT_NAME}_lib PRIVATE --coverage)
        target_link_options(${PROJECT_NAME}_lib PRIVATE --coverage)
        
        # Find gcov or llvm-cov
        if(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
            find_program(GCOV_EXECUTABLE gcov)
            if(GCOV_EXECUTABLE)
                add_custom_target(coverage
                    COMMAND gcov -r ${CMAKE_CURRENT_BINARY_DIR}
                    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
                    COMMENT "Generating coverage report"
                )
            endif()
        elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
            find_program(LLVM_COV_EXECUTABLE llvm-cov)
            if(LLVM_COV_EXECUTABLE)
                add_custom_target(coverage
                    COMMAND ${LLVM_COV_EXECUTABLE} gcov ${CMAKE_CURRENT_BINARY_DIR}
                    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
                    COMMENT "Generating coverage report"
                )
            endif()
        endif()
    endif()
endfunction()

# Function to setup all testing
function(setup_testing)
    add_unit_tests()
    add_integration_tests()
    add_benchmark_tests()
    add_performance_tests()
    add_memory_leak_tests()
    add_test_coverage()
endfunction()

# Function to add test labels and organization
function(organize_tests)
    if(NOT BUILD_TESTING)
        return()
    endif()
    
    # Create test suites
    add_custom_target(test_unit
        COMMAND ${CMAKE_CTEST_COMMAND} -L unit
        COMMENT "Running unit tests"
    )
    
    add_custom_target(test_integration
        COMMAND ${CMAKE_CTEST_COMMAND} -L integration
        COMMENT "Running integration tests"
    )
    
    add_custom_target(test_memory_leak
        COMMAND ${CMAKE_CTEST_COMMAND} -L memory_leak
        COMMENT "Running memory leak tests"
    )
    
    add_custom_target(test_all
        COMMAND ${CMAKE_CTEST_COMMAND}
        COMMENT "Running all tests"
    )
endfunction()
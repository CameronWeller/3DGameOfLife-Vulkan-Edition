# Performance Tests CMakeLists.txt

add_executable(performance_tests
    GameOfLifePerformanceTest.cpp
    StressTest.cpp
)

target_link_libraries(performance_tests
    PRIVATE
    GTest::GTest
    GTest::Main
    cpp-vulkan-hip-engine
)

target_include_directories(performance_tests
    PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/include
)

# Add compiler warnings
target_compile_options(performance_tests
    PRIVATE
    -Wall
    -Wextra
    -Wpedantic
)

# Add test
add_test(
    NAME performance_tests
    COMMAND performance_tests
)

# Set test properties
set_tests_properties(performance_tests
    PROPERTIES
    ENVIRONMENT "VK_LAYER_PATH=${Vulkan_LAYER_PATH}"
    ENVIRONMENT "VK_INSTANCE_LAYERS=VK_LAYER_KHRONOS_validation"
    TIMEOUT 600  # 10 minutes timeout for stress tests
) 
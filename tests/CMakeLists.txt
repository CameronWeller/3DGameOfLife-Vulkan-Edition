cmake_minimum_required(VERSION 3.15)
project(VulkanEngineTests)

# Find required packages
find_package(GTest REQUIRED)
find_package(Threads REQUIRED)
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
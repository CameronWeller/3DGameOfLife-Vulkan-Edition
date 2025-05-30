# Unit Tests CMakeLists.txt

# Create a test executable
add_executable(unit_tests
    VulkanEngineTest.cpp
    VulkanContextTest.cpp
    WindowManagerTest.cpp
)

# Link against GTest and the main project
target_link_libraries(unit_tests PRIVATE
    GTest::GTest
    GTest::Main
    cpp-vulkan-hip-engine
)

# Include directories
target_include_directories(unit_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/include
)

# Add test
add_test(NAME unit_tests COMMAND unit_tests)

# Set test properties
set_tests_properties(unit_tests PROPERTIES
    ENVIRONMENT "GTEST_COLOR=1"
    TIMEOUT 300
)

find_package(GTest REQUIRED)

add_executable(vulkan_validation_test
    VulkanValidationTest.cpp
)

target_link_libraries(vulkan_validation_test
    PRIVATE
    GTest::GTest
    GTest::Main
    cpp-vulkan-hip-engine
)

target_include_directories(vulkan_validation_test
    PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/include
)

# Add compiler warnings
target_compile_options(vulkan_validation_test
    PRIVATE
    -Wall
    -Wextra
    -Wpedantic
)

# Add test
add_test(
    NAME vulkan_validation_test
    COMMAND vulkan_validation_test
)

# Set test properties
set_tests_properties(vulkan_validation_test
    PROPERTIES
    ENVIRONMENT "VK_LAYER_PATH=${Vulkan_LAYER_PATH}"
    ENVIRONMENT "VK_INSTANCE_LAYERS=VK_LAYER_KHRONOS_validation"
) 
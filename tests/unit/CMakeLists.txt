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
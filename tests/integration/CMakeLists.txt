add_executable(vulkan_integration_tests
    ../test_vulkan_integration.cpp
)

target_link_libraries(vulkan_integration_tests
    PRIVATE
    gtest
    gtest_main
    gmock
    gmock_main
    ${PROJECT_NAME}
)

add_test(NAME vulkan_integration_tests COMMAND vulkan_integration_tests) 
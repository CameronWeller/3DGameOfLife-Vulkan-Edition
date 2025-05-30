add_executable(vulkan_performance_tests
    ../VulkanEngineTest.cpp
    ../MemoryLeakTest.cpp
)

target_link_libraries(vulkan_performance_tests
    PRIVATE
    benchmark
    benchmark_main
    ${PROJECT_NAME}
)

add_test(NAME vulkan_performance_tests COMMAND vulkan_performance_tests) 
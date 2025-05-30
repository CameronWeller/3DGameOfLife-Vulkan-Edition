add_executable(vulkan_engine_performance_tests
    ../VulkanEngineTest.cpp
    ../MemoryLeakTest.cpp
)

target_link_libraries(vulkan_engine_performance_tests
    PRIVATE
    benchmark
    benchmark_main
    ${PROJECT_NAME}
)

add_test(NAME vulkan_engine_performance_tests COMMAND vulkan_engine_performance_tests) 
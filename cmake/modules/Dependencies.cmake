# Dependencies Configuration
# This module manages all external dependencies for the project

# Find required packages
find_package(Vulkan REQUIRED)
find_package(HIP QUIET)  # Make HIP optional
find_package(glfw3 REQUIRED)
find_package(glm REQUIRED)
find_package(spdlog REQUIRED)
find_package(VulkanMemoryAllocator CONFIG REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(GTest REQUIRED)
find_package(benchmark REQUIRED)

# Find glslc compiler
find_program(GLSLC_EXECUTABLE glslc HINTS ${Vulkan_GLSLC_EXECUTABLE} ENV VULKAN_SDK PATH_SUFFIXES bin)
if(NOT GLSLC_EXECUTABLE)
    message(FATAL_ERROR "glslc compiler not found. Please install Vulkan SDK.")
endif()

# Check for required programs
find_program(GIT_EXECUTABLE git)
if(NOT GIT_EXECUTABLE)
    message(WARNING "git not found. Some features may not work.")
endif()

# Create a unified dependencies target
add_library(project_dependencies INTERFACE)
target_link_libraries(project_dependencies INTERFACE
    Vulkan::Vulkan
    glfw
    glm::glm
    spdlog::spdlog
    GPUOpen::VulkanMemoryAllocator
    nlohmann_json::nlohmann_json
    imgui::imgui
)

# Add VMA submodule include for documentation access by agents
# This provides full source code with comprehensive comments while still using vcpkg for actual linking
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/VulkanMemoryAllocator/include")
    target_include_directories(project_dependencies INTERFACE
        "${CMAKE_CURRENT_SOURCE_DIR}/third_party/VulkanMemoryAllocator/include"
    )
    message(STATUS "VMA Documentation: Added VMA submodule include path for agent documentation access")
endif()

# Create testing dependencies target
add_library(testing_dependencies INTERFACE)
target_link_libraries(testing_dependencies INTERFACE
    GTest::gtest
    GTest::gtest_main
    benchmark::benchmark
    benchmark::benchmark_main
)

# Set common include directories
set(PROJECT_INCLUDE_DIRS
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${Vulkan_INCLUDE_DIRS}
    ${GLFW_INCLUDE_DIRS}
    ${GLM_INCLUDE_DIRS}
)

# Function to apply common settings to targets
function(apply_common_settings target_name)
    target_include_directories(${target_name} PUBLIC ${PROJECT_INCLUDE_DIRS})
    
    # Add VMA submodule include for documentation access by agents
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/VulkanMemoryAllocator/include")
        target_include_directories(${target_name} PUBLIC
            "${CMAKE_CURRENT_SOURCE_DIR}/third_party/VulkanMemoryAllocator/include"
        )
    endif()
    
    # Add compiler definitions
    target_compile_definitions(${target_name} PRIVATE
        $<$<CONFIG:Debug>:DEBUG>
        $<$<CONFIG:Debug>:_DEBUG>
        $<$<CONFIG:Release>:NDEBUG>
        $<$<CONFIG:Release>:_NDEBUG>
    )
    
    # Add compiler warnings
    if(MSVC)
        target_compile_options(${target_name} PRIVATE
            /W4
            /wd4100  # Unreferenced formal parameter
            /wd4189  # Local variable is initialized but not referenced
            /wd4251  # DLL interface warning
            /wd4275  # DLL interface warning
        )
    else()
        target_compile_options(${target_name} PRIVATE
            -Wall
            -Wextra
            -Wpedantic
            -Wno-unused-parameter
            -Wno-unused-variable
        )
    endif()
endfunction()
# Minimal CMakeLists.txt for 3D Game of Life - Vulkan Edition
cmake_minimum_required(VERSION 3.20)

project(3DGameOfLife-Vulkan-Edition-Minimal
    VERSION 1.0.0
    DESCRIPTION "Minimal 3D Conway's Game of Life with Vulkan"
    LANGUAGES CXX
)

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# vcpkg integration
if(DEFINED ENV{VCPKG_ROOT} AND NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake" CACHE STRING "")
endif()

# Find packages using vcpkg
find_package(Vulkan REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)

# Create minimal executable
add_executable(minimal_vulkan_app
    src/main_minimal.cpp
    src/VulkanContext.cpp
    src/DeviceManager.cpp
    src/WindowManager.cpp
    src/Logger.cpp
    src/Camera.cpp
)

# Link libraries
target_link_libraries(minimal_vulkan_app PRIVATE
    Vulkan::Vulkan
    glfw
    glm::glm
)

# Include directories
target_include_directories(minimal_vulkan_app PRIVATE
    include
    include/vulkan
    include/vulkan/resources
    ${Vulkan_INCLUDE_DIRS}
)

# Compiler settings
if(MSVC)
    target_compile_options(minimal_vulkan_app PRIVATE /W4)
else()
    target_compile_options(minimal_vulkan_app PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Add definitions
target_compile_definitions(minimal_vulkan_app PRIVATE
    $<$<CONFIG:Debug>:DEBUG>
    $<$<CONFIG:Release>:NDEBUG>
)

message(STATUS "Minimal Vulkan App configured successfully") 
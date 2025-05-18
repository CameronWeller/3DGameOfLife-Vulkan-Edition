cmake_minimum_required(VERSION 3.15)
project(CPPVulkanHIPEngine LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find required packages
find_package(Vulkan REQUIRED)
find_package(HIP QUIET)  # Make HIP optional

# Add GLFW
include(FetchContent)
FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG 3.3.8
)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(glfw)

# Add GLM
FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG 0.9.9.8
)
FetchContent_MakeAvailable(glm)

# Add source files
add_executable(engine
    src/main.cpp
    src/VulkanEngine.cpp
    src/VulkanEngine.h
)

target_include_directories(engine PRIVATE 
    ${Vulkan_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${glm_SOURCE_DIR}
)

target_link_libraries(engine PRIVATE 
    ${Vulkan_LIBRARIES}
    glfw
    glm
)

# Only link HIP if it was found
if(HIP_FOUND)
    target_link_libraries(engine PRIVATE HIP::hip)
    target_compile_definitions(engine PRIVATE WITH_HIP)
endif() 
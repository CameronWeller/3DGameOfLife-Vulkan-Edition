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

# Add nlohmann_json
FetchContent_Declare(
    json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.2
)
FetchContent_MakeAvailable(json)

# Find glslc compiler
find_program(GLSLC_EXECUTABLE glslc HINTS ${Vulkan_GLSLC_EXECUTABLE} ENV VULKAN_SDK PATH_SUFFIXES bin)
if(NOT GLSLC_EXECUTABLE)
    message(FATAL_ERROR "glslc compiler not found. Please install Vulkan SDK.")
endif()

# Compile shaders
set(SHADER_DIR ${CMAKE_CURRENT_SOURCE_DIR}/shaders)
set(SHADER_OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/shaders)

file(MAKE_DIRECTORY ${SHADER_OUTPUT_DIR})

# Initialize shader list
set(SHADER_SPV_FILES "")

# Function to compile shaders
function(compile_shader SHADER_NAME)
    set(SHADER_PATH ${SHADER_DIR}/${SHADER_NAME})
    set(OUTPUT_PATH ${SHADER_OUTPUT_DIR}/${SHADER_NAME}.spv)
    
    add_custom_command(
        OUTPUT ${OUTPUT_PATH}
        COMMAND ${GLSLC_EXECUTABLE} -o ${OUTPUT_PATH} ${SHADER_PATH}
        DEPENDS ${SHADER_PATH}
        COMMENT "Compiling shader ${SHADER_NAME}"
    )
    
    list(APPEND SHADER_SPV_FILES ${OUTPUT_PATH})
    set(SHADER_SPV_FILES ${SHADER_SPV_FILES} PARENT_SCOPE)
endfunction()

# Compile all shaders
compile_shader(basic.vert)
compile_shader(basic.frag)
compile_shader(game_of_life_3d.comp)
compile_shader(population_reduction.comp)

# Add source files
add_executable(vulkan-engine
    src/main.cpp
    src/VulkanEngine.cpp
    src/VulkanEngine.h
    src/MemoryPool.h
    src/QueueFamilyIndices.h
    src/Grid3D.cpp
    src/Grid3D.h
    src/UI.cpp
    src/UI.h
    src/RayCaster.cpp
    src/RayCaster.h
    src/Camera.cpp
    src/Camera.h
    src/logging/Logger.cpp
    src/logging/Logger.h
    ${SHADER_SPV_FILES}
)

target_include_directories(vulkan-engine PRIVATE 
    ${Vulkan_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${glm_SOURCE_DIR}
    ${SHADER_OUTPUT_DIR}
    ${json_SOURCE_DIR}/include
)

target_link_libraries(vulkan-engine PRIVATE 
    ${Vulkan_LIBRARIES}
    glfw
    glm
)

# Only link HIP if it was found
if(HIP_FOUND)
    target_link_libraries(vulkan-engine PRIVATE HIP::hip)
    target_compile_definitions(vulkan-engine PRIVATE WITH_HIP)
endif()

# Make scripts executable
file(GLOB SCRIPTS "scripts/*.sh")
foreach(SCRIPT ${SCRIPTS})
    execute_process(COMMAND chmod +x ${SCRIPT})
endforeach() 
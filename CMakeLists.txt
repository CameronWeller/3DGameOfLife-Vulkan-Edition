cmake_minimum_required(VERSION 3.10)
cmake_policy(VERSION 3.10)
project(CPPVulkanHIPEngine LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set policy for GLM compatibility
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)

# Windows-specific settings
if(WIN32)
    # Use MT instead of MD when building with MSVC
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    
    # Disable specific warnings on Windows
    if(MSVC)
        add_compile_options(/W4 /wd4100 /wd4189)
    endif()
endif()

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
set(GLM_FORCE_CTOR_INIT ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(glm)

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

# Define source files
set(SOURCE_FILES
    src/main.cpp
    src/VulkanEngine.cpp
    src/VulkanEngine.h
    src/Grid3D.cpp
    src/Grid3D.h
    src/MemoryPool.cpp
    src/MemoryPool.h
    src/Vertex.h
)

# Add the executable
add_executable(vulkan-engine
    ${SOURCE_FILES}
    ${SHADER_SPV_FILES}
)

target_include_directories(vulkan-engine PRIVATE 
    ${Vulkan_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${glm_SOURCE_DIR}
    ${SHADER_OUTPUT_DIR}
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

# Post-build copy of shaders for Windows
if(WIN32)
    add_custom_command(TARGET vulkan-engine POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:vulkan-engine>/shaders/"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${SHADER_OUTPUT_DIR}" "$<TARGET_FILE_DIR:vulkan-engine>/shaders/"
        COMMENT "Copying shader files to output directory"
    )
endif()

# Out-of-source build check
if(${CMAKE_SOURCE_DIR} STREQUAL ${CMAKE_BINARY_DIR})
  message(FATAL_ERROR "In-source builds are not allowed. Please use a separate build directory.")
endif()

# Compiler warnings
if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic)
    # add_compile_options(-Werror) # Uncomment to treat warnings as errors
endif()

# Export compile commands
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Add a distclean target
add_custom_target(distclean
    COMMAND ${CMAKE_COMMAND} -E rm -rf ${CMAKE_BINARY_DIR}/*
    COMMENT "Remove all build files"
)

# Check for required programs
find_program(GIT_EXECUTABLE git)
if(NOT GIT_EXECUTABLE)
    message(WARNING "git not found. Some features may not work.")
endif() 
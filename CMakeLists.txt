cmake_minimum_required(VERSION 3.20)
cmake_policy(VERSION 3.10)

# Set vcpkg toolchain file if it exists
if(DEFINED ENV{VCPKG_ROOT} AND NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
        CACHE STRING "")
    # Add vcpkg installed directory to prefix path
    list(APPEND CMAKE_PREFIX_PATH "$ENV{VCPKG_ROOT}/installed/x64-windows")
endif()

project(cpp-vulkan-hip-engine LANGUAGES CXX)

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
find_package(glfw3 REQUIRED)
find_package(glm REQUIRED)
find_package(imgui REQUIRED)
find_package(spdlog REQUIRED)
find_package(VulkanMemoryAllocator REQUIRED)

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
function(compile_shader SHADER_PATH)
    get_filename_component(SHADER_NAME ${SHADER_PATH} NAME_WE)
    set(OUTPUT_PATH "${SHADER_OUTPUT_DIR}/${SHADER_NAME}.spv")
    
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
file(GLOB SHADER_FILES "${SHADER_DIR}/*.glsl")
foreach(SHADER ${SHADER_FILES})
    compile_shader(${SHADER})
endforeach()

# Add executable
add_executable(${PROJECT_NAME} 
    src/main.cpp
    src/VulkanEngine.cpp
    src/VulkanContext.cpp
    src/WindowManager.cpp
    src/Camera.cpp
    src/RayCaster.cpp
    src/VulkanMemoryManager.cpp
    src/SaveManager.cpp
    src/Logger.cpp
)

# Link libraries
target_link_libraries(${PROJECT_NAME} PRIVATE
    Vulkan::Vulkan
    glfw
    glm::glm
    imgui::imgui
    spdlog::spdlog
    GPUOpen::VulkanMemoryAllocator
    vma
)

# Include directories
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${Vulkan_INCLUDE_DIRS}
    ${GLFW_INCLUDE_DIRS}
    ${GLM_INCLUDE_DIRS}
    ${IMGUI_DIR}
    ${VMA_DIR}/include
)

# Add shader files to the build
add_custom_target(shaders DEPENDS ${SHADER_SPV_FILES})
add_dependencies(${PROJECT_NAME} shaders)

# Copy shaders to build directory on Windows
if(WIN32)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${SHADER_OUTPUT_DIR}
        $<TARGET_FILE_DIR:${PROJECT_NAME}>/shaders
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
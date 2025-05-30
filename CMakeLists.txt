cmake_minimum_required(VERSION 3.20)
cmake_policy(VERSION 3.10)

# Vcpkg integration
if(DEFINED ENV{VCPKG_ROOT} AND NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
        CACHE STRING "Vcpkg toolchain file")
endif()

project(cpp-vulkan-hip-engine LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Require MSVC compiler for best Vulkan compatibility on Windows
if(WIN32)
    if(NOT MSVC)
        message(FATAL_ERROR "This project requires Microsoft Visual C++ (MSVC) compiler on Windows for optimal Vulkan support. "
                            "Please install Visual Studio 2022 with C++ development tools or run scripts/install-msvc-compiler.ps1")
    endif()
    
    # Set MSVC-specific flags for better Vulkan compatibility
    if(MSVC_VERSION LESS 1930)
        message(FATAL_ERROR "Visual Studio 2022 or later is required (MSVC version 19.30+)")
    endif()
endif()

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
find_package(spdlog REQUIRED)
find_package(VulkanMemoryAllocator CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(GTest REQUIRED)
find_package(benchmark REQUIRED)

# Find glslc compiler
find_program(GLSLC_EXECUTABLE glslc HINTS ${Vulkan_GLSLC_EXECUTABLE} ENV VULKAN_SDK PATH_SUFFIXES bin)
if(NOT GLSLC_EXECUTABLE)
    message(FATAL_ERROR "glslc compiler not found. Please install Vulkan SDK.")
endif()

# Include CMake modules
include(.cmake/StaticAnalysis.cmake)
include(.cmake/Testing.cmake)

# Compile shaders
set(SHADER_DIR ${CMAKE_CURRENT_SOURCE_DIR}/shaders)
set(SHADER_OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/shaders)

file(MAKE_DIRECTORY ${SHADER_OUTPUT_DIR})

# Initialize shader list
set(SHADER_SPV_FILES "")

# Function to compile shaders
function(compile_shader SHADER_PATH)
    get_filename_component(SHADER_FILENAME ${SHADER_PATH} NAME)
    set(OUTPUT_PATH "${SHADER_OUTPUT_DIR}/${SHADER_FILENAME}.spv")
    
    add_custom_command(
        OUTPUT ${OUTPUT_PATH}
        COMMAND ${GLSLC_EXECUTABLE} -o ${OUTPUT_PATH} ${SHADER_PATH}
        DEPENDS ${SHADER_PATH}
        COMMENT "Compiling shader ${SHADER_FILENAME}"
        VERBATIM
    )
    
    if(NOT EXISTS ${OUTPUT_PATH})
        message(FATAL_ERROR "Failed to compile shader: ${SHADER_PATH}")
    endif()
    
    list(APPEND SHADER_SPV_FILES ${OUTPUT_PATH})
    set(SHADER_SPV_FILES ${SHADER_SPV_FILES} PARENT_SCOPE)
endfunction()

# Compile all shaders
file(GLOB SHADER_FILES 
    "${SHADER_DIR}/*.glsl"
    "${SHADER_DIR}/*.vert"
    "${SHADER_DIR}/*.frag"
    "${SHADER_DIR}/*.comp"
)
foreach(SHADER ${SHADER_FILES})
    compile_shader(${SHADER})
endforeach()

# Add library target for the main project
add_library(${PROJECT_NAME} STATIC
    # Core Engine
    src/VulkanEngine.cpp
    include/VulkanEngine.h
    src/Engine.cpp
    include/Engine.h
    include/AppState.h
    
    # Vulkan Components
    src/VulkanContext.cpp
    include/VulkanContext.h
    src/VulkanMemoryManager.cpp
    include/VulkanMemoryManager.h
    src/DeviceManager.cpp
    include/DeviceManager.h
    include/VulkanError.h
    include/SwapChainSupportDetails.h
    include/QueueFamilyIndices.h
    
    # Window and Input
    src/WindowManager.cpp
    include/WindowManager.h
    src/Camera.cpp
    include/Camera.h
    src/RayCaster.cpp
    include/RayCaster.h
    
    # Game Logic
    src/Grid3D.cpp
    include/Grid3D.h
    src/SparseGrid3D.cpp
    include/SparseGrid3D.h
    src/PatternManager.cpp
    include/PatternManager.h
    include/GameRules.h
    src/RuleAnalyzer.cpp
    include/RuleAnalyzer.h
    
    # UI and Rendering
    src/UI.cpp
    include/UI.h
    src/Vertex.cpp
    include/Vertex.h
    src/VoxelData.cpp
    include/VoxelData.h
    src/imgui_impl_glfw.cpp
    include/imgui_impl_glfw.h
    src/imgui_impl_vulkan.cpp
    include/imgui_impl_vulkan.h
    
    # Utilities
    src/SaveManager.cpp
    include/SaveManager.h
    src/Logger.cpp
    include/Logger.h
    include/ThreadSafe.h
)

# Add executable target
add_executable(${PROJECT_NAME}_exe src/main.cpp)
target_link_libraries(${PROJECT_NAME}_exe PRIVATE ${PROJECT_NAME})

# Link libraries
target_link_libraries(${PROJECT_NAME} PUBLIC
    Vulkan::Vulkan
    glfw
    glm::glm
    spdlog::spdlog
    GPUOpen::VulkanMemoryAllocator
    nlohmann_json::nlohmann_json
    imgui::imgui
    GTest::gtest
    GTest::gtest_main
    benchmark::benchmark
)

# Include directories
target_include_directories(${PROJECT_NAME} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${Vulkan_INCLUDE_DIRS}
    ${GLFW_INCLUDE_DIRS}
    ${GLM_INCLUDE_DIRS}
    $<TARGET_PROPERTY:nlohmann_json::nlohmann_json,INTERFACE_INCLUDE_DIRECTORIES>
    $<TARGET_PROPERTY:imgui::imgui,INTERFACE_INCLUDE_DIRECTORIES>
)

# Add compiler definitions
target_compile_definitions(${PROJECT_NAME} PRIVATE
    $<$<CONFIG:Debug>:DEBUG>
    $<$<CONFIG:Debug>:_DEBUG>
    $<$<CONFIG:Release>:NDEBUG>
    $<$<CONFIG:Release>:_NDEBUG>
)

# Add compiler warnings
if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE
        /W4
        /wd4100  # Unreferenced formal parameter
        /wd4189  # Local variable is initialized but not referenced
        /wd4251  # DLL interface warning
        /wd4275  # DLL interface warning
    )
else()
    target_compile_options(${PROJECT_NAME} PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -Wno-unused-parameter
        -Wno-unused-variable
    )
endif()

# Add shader files to the build
add_custom_target(shaders DEPENDS ${SHADER_SPV_FILES})
add_dependencies(${PROJECT_NAME} shaders)

# Copy shaders to build directory on Windows
if(WIN32)
    add_custom_command(TARGET ${PROJECT_NAME}_exe POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${SHADER_OUTPUT_DIR}
        $<TARGET_FILE_DIR:${PROJECT_NAME}_exe>/shaders
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

# Add testing
option(BUILD_TESTING "Build tests" ON)
if(BUILD_TESTING)
    add_subdirectory(tests)
    add_project_tests()
    add_static_analysis_targets()
endif()

# Add sanitizers
option(ENABLE_SANITIZERS "Enable sanitizers" OFF)
if(ENABLE_SANITIZERS)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(${PROJECT_NAME} PRIVATE
            -fsanitize=address
            -fsanitize=undefined
            -fno-omit-frame-pointer
        )
        target_link_options(${PROJECT_NAME} PRIVATE
            -fsanitize=address
            -fsanitize=undefined
        )
    endif()
endif()

# Add custom targets for development workflow
add_custom_target(check
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
    DEPENDS ${PROJECT_NAME}_tests
    COMMENT "Running tests"
)

add_custom_target(bench
    COMMAND ${PROJECT_NAME}_benchmarks
    DEPENDS ${PROJECT_NAME}_benchmarks
    COMMENT "Running benchmarks"
)

add_custom_target(analyze
    DEPENDS static-analysis
    COMMENT "Running static analysis"
)

add_custom_target(quality
    DEPENDS check bench analyze
    COMMENT "Running all quality checks"
) 
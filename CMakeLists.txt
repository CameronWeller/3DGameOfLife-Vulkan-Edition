# 3D Game of Life - Vulkan Edition
# Modular CMake Configuration

cmake_minimum_required(VERSION 3.20)

# Configure vcpkg
set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/scripts/buildsystems/vcpkg.cmake" CACHE STRING "")

# Project definition
project(3DGameOfLife-Vulkan-Edition
    VERSION 1.0.0
    DESCRIPTION "3D Conway's Game of Life implemented with Vulkan compute shaders"
    LANGUAGES CXX
)

# Add module path
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules")

# Include modular configuration
include(Options)           # Build options and compiler settings
include(Dependencies)      # External dependencies
include(Shaders)          # Shader compilation
include(Components)       # Component libraries
include(Testing)          # Testing configuration
include(StaticAnalysis)   # Static analysis tools

# Create all component libraries
create_all_components()

# Create unified library
create_unified_library()

# Main executable
add_executable(${PROJECT_NAME}
    src/main.cpp
)

# Link main executable
target_link_libraries(${PROJECT_NAME} PRIVATE
    ${PROJECT_NAME}_lib
)

# Apply common settings to main executable
apply_common_settings(${PROJECT_NAME})

# Apply static analysis to main executable
apply_static_analysis(${PROJECT_NAME})

# Enable sanitizers for main executable
enable_sanitizers(${PROJECT_NAME})

# Compile shaders
compile_shaders()
copy_shaders_to_output()
validate_shaders()
add_shader_clean_target()

# Setup testing
setup_testing()
organize_tests()

# Add static analysis targets
add_static_analysis_targets()

# Custom targets for development
add_custom_target(distclean
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_BINARY_DIR}
    COMMENT "Removing build directory"
)

add_custom_target(rebuild
    COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target clean
    COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR}
    COMMENT "Clean rebuild"
)

# Development convenience targets
add_custom_target(dev_build
    COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target ${PROJECT_NAME}
    COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target compile_shaders
    DEPENDS ${PROJECT_NAME} compile_shaders
    COMMENT "Development build (executable + shaders)"
)

add_custom_target(quick_test
    COMMAND ${CMAKE_CTEST_COMMAND} -L unit --output-on-failure
    DEPENDS unit_tests
    COMMENT "Quick unit test run"
)

# Print configuration summary
message(STATUS "")
message(STATUS "=== 3D Game of Life - Vulkan Edition Configuration ===")
message(STATUS "Project Version: ${PROJECT_VERSION}")
message(STATUS "Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "Compiler: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "Build Testing: ${BUILD_TESTING}")
message(STATUS "Static Analysis: ${ENABLE_STATIC_ANALYSIS}")
message(STATUS "Sanitizers: ${ENABLE_SANITIZERS}")
message(STATUS "Coverage: ${ENABLE_COVERAGE}")
if(GLSLC_EXECUTABLE)
    message(STATUS "Shader Compiler: ${GLSLC_EXECUTABLE}")
else()
    message(WARNING "Shader Compiler: NOT FOUND")
endif()
message(STATUS "")

# Component summary
message(STATUS "=== Component Libraries ===")
message(STATUS "Core Engine: core_engine")
message(STATUS "Rendering: rendering")
message(STATUS "Compute: compute")
message(STATUS "Game Logic: game_logic")
message(STATUS "Camera: camera")
message(STATUS "UI: ui")
message(STATUS "Utilities: utilities")
message(STATUS "Unified Library: ${PROJECT_NAME}_lib")
message(STATUS "")

# Available targets summary
message(STATUS "=== Available Targets ===")
message(STATUS "Main executable: ${PROJECT_NAME}")
message(STATUS "Development: dev_build, rebuild, distclean")
if(BUILD_TESTING)
    message(STATUS "Testing: test_all, test_unit, test_integration, quick_test")
    message(STATUS "Benchmarks: run_benchmarks, run_benchmarks_detailed")
endif()
if(ENABLE_STATIC_ANALYSIS)
    message(STATUS "Static Analysis: clang-tidy, cppcheck, iwyu")
endif()
message(STATUS "Shaders: compile_shaders, copy_shaders, validate_shaders, clean_shaders")
message(STATUS "")
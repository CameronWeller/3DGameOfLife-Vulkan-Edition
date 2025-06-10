cmake_minimum_required(VERSION 3.20)
project(UXMirrorMinimalOptimizer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

<<<<<<< HEAD
# Build flags
if(MSVC)
    set(CMAKE_CXX_FLAGS_RELEASE "/O2 /DNDEBUG")
    set(CMAKE_CXX_FLAGS_DEBUG "/Od /Zi /DDEBUG")
=======
# Add module path
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules")

# vcpkg integration
if(DEFINED ENV{VCPKG_ROOT} AND NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake" CACHE STRING "")
endif()

# Ensure vcpkg packages are found - explicit path for local vcpkg installation
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg_installed")
    list(APPEND CMAKE_PREFIX_PATH "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg_installed/x64-windows")
    message(STATUS "Added vcpkg packages to CMAKE_PREFIX_PATH: ${CMAKE_CURRENT_SOURCE_DIR}/vcpkg_installed/x64-windows")
endif()

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

# Minimal executable using only working components
add_executable(${PROJECT_NAME}_minimal
    src/main_minimal.cpp
)

# Link minimal executable with only working components
target_link_libraries(${PROJECT_NAME}_minimal PRIVATE
    core_engine
    utilities
    vulkan_resources
    # memory_management  # Temporarily disabled due to VMA linking issues
    rendering
    camera  # Add camera for 3D functionality
    # GPUOpen::VulkanMemoryAllocator  # Not needed without memory_management
    # Temporarily skip: game_logic, ui, vulkan_ui, vulkan_rendering (broken)
)

# Self-contained minimal target that doesn't depend on main project libraries
add_executable(minimal_3d_vulkan
    src/main_minimal.cpp
    src/VulkanContext.cpp
    src/DeviceManager.cpp
    src/WindowManager.cpp
    src/Logger.cpp
    src/Camera.cpp
    src/vulkan/resources/ShaderManager.cpp
)

# Link only essential libraries for the self-contained target
target_link_libraries(minimal_3d_vulkan PRIVATE
    Vulkan::Vulkan
    glfw
    glm::glm
)

target_include_directories(minimal_3d_vulkan PRIVATE
    include
    include/vulkan
    include/vulkan/resources
)

# Apply common settings to executables
apply_common_settings(${PROJECT_NAME})
apply_common_settings(${PROJECT_NAME}_minimal)

# Apply static analysis to executables
apply_static_analysis(${PROJECT_NAME})
apply_static_analysis(${PROJECT_NAME}_minimal)

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
>>>>>>> 741179b1238ce7a6caba735329b32bdfd4d7e660
else()
    set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")
    set(CMAKE_CXX_FLAGS_DEBUG "-O0 -g -DDEBUG")
endif()

# Create the minimal overnight optimizer
add_executable(minimal_overnight_optimizer
    src/OvernightOptimizer_minimal.cpp
)

# No external dependencies needed - just standard library

# Install target
install(TARGETS minimal_overnight_optimizer
    RUNTIME DESTINATION bin
)

# Custom target for running overnight optimization
add_custom_target(run_minimal_overnight
    COMMAND ${CMAKE_CURRENT_BINARY_DIR}/minimal_overnight_optimizer
    DEPENDS minimal_overnight_optimizer
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Running minimal overnight optimization..."
) 
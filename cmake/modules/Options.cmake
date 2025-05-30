# Build Options Configuration
# This module defines all build options and configurations for the project

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Build type options
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Choose the type of build" FORCE)
endif()

# Project options
option(BUILD_TESTING "Build tests" ON)
option(ENABLE_STATIC_ANALYSIS "Enable static analysis tools" ON)
option(ENABLE_SANITIZERS "Enable sanitizers" OFF)
option(ENABLE_COVERAGE "Enable test coverage reporting" OFF)
option(ENABLE_SONARQUBE "Enable SonarQube analysis" OFF)

# Windows-specific settings
if(WIN32)
    # Require MSVC compiler for best Vulkan compatibility on Windows
    if(NOT MSVC)
        message(FATAL_ERROR "This project requires Microsoft Visual C++ (MSVC) compiler on Windows for optimal Vulkan support. "
                            "Please install Visual Studio 2022 with C++ development tools or run scripts/install-msvc-compiler.ps1")
    endif()
    
    # Set MSVC-specific flags for better Vulkan compatibility
    if(MSVC_VERSION LESS 1930)
        message(FATAL_ERROR "Visual Studio 2022 or later is required (MSVC version 19.30+)")
    endif()
    
    # Use MT instead of MD when building with MSVC
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

# Set policy for GLM compatibility
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)

# Export compile commands
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Out-of-source build check
if(${CMAKE_SOURCE_DIR} STREQUAL ${CMAKE_BINARY_DIR})
    message(FATAL_ERROR "In-source builds are not allowed. Please use a separate build directory.")
endif()

# Compiler warnings
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic)
    # add_compile_options(-Werror) # Uncomment to treat warnings as errors
endif()
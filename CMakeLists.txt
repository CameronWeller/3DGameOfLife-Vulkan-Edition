cmake_minimum_required(VERSION 3.20)
project(UXMirrorPrototype LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add module path
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules")

# vcpkg integration
if(DEFINED ENV{VCPKG_ROOT} AND NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake" CACHE STRING "")
endif()

# Build flags
if(MSVC)
    set(CMAKE_CXX_FLAGS_RELEASE "/O2 /DNDEBUG")
    set(CMAKE_CXX_FLAGS_DEBUG "/Od /Zi /DDEBUG")
else()
    set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")
    set(CMAKE_CXX_FLAGS_DEBUG "-O0 -g -DDEBUG")
endif()

# Ensure vcpkg packages are found - explicit path for local vcpkg installation
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg_installed")
    list(APPEND CMAKE_PREFIX_PATH "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg_installed/x64-windows")
    message(STATUS "Added vcpkg packages to CMAKE_PREFIX_PATH: ${CMAKE_CURRENT_SOURCE_DIR}/vcpkg_installed/x64-windows")
endif()

# Try to include modular configuration (if available)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules/Options.cmake")
    include(Options)           # Build options and compiler settings
    include(Dependencies)      # External dependencies  
    include(Components)        # Component libraries
    
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
        rendering
        camera
    )
# UX-Mirror Prototype Components
add_executable(ux_mirror_prototype
    src/OvernightOptimizer_minimal.cpp
    src/SharedMemoryInterface.cpp
    src/HIPCellularAutomata.cpp
    # Add the stub files that we have  
    src/MetricsPipeline_stub.cpp
    src/AgentCommunication_stub.cpp
)

# Find required packages for prototype
find_package(Vulkan QUIET)
find_package(glfw3 QUIET)
find_package(glm QUIET)

if(Vulkan_FOUND)
    target_link_libraries(ux_mirror_prototype PRIVATE Vulkan::Vulkan)
    target_compile_definitions(ux_mirror_prototype PRIVATE VULKAN_AVAILABLE)
endif()

if(glfw3_FOUND)
    target_link_libraries(ux_mirror_prototype PRIVATE glfw)
endif()

if(glm_FOUND)
    target_link_libraries(ux_mirror_prototype PRIVATE glm::glm)
endif()

# Try to find HIP (optional for now)
find_package(hip QUIET)
if(hip_FOUND)
    target_link_libraries(ux_mirror_prototype PRIVATE hip::host)
    target_compile_definitions(ux_mirror_prototype PRIVATE HIP_AVAILABLE)
endif()

target_include_directories(ux_mirror_prototype PRIVATE
    include
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Handle compilation flags for different systems
if(WIN32)
    target_compile_definitions(ux_mirror_prototype PRIVATE _WIN32)

# UX-Mirror Launcher with Dashboard
add_executable(ux_mirror_launcher
    src/UXMirrorLauncher.cpp
)

target_compile_features(ux_mirror_launcher PRIVATE cxx_std_17)

# Create the minimal overnight optimizer (keeping existing functionality)
add_executable(minimal_overnight_optimizer
    src/OvernightOptimizer_minimal.cpp
)

# No external dependencies needed - just standard library for minimal optimizer

# Install target
install(TARGETS minimal_overnight_optimizer ux_mirror_prototype ux_mirror_launcher
    RUNTIME DESTINATION bin
)

# Custom targets for running different components
add_custom_target(run_minimal_overnight
    COMMAND ${CMAKE_CURRENT_BINARY_DIR}/minimal_overnight_optimizer
    DEPENDS minimal_overnight_optimizer
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Running minimal overnight optimization..."
)

add_custom_target(run_prototype
    COMMAND ${CMAKE_CURRENT_BINARY_DIR}/ux_mirror_prototype
    DEPENDS ux_mirror_prototype
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Running UX-Mirror prototype..."
)

add_custom_target(run_launcher
    COMMAND ${CMAKE_CURRENT_BINARY_DIR}/ux_mirror_launcher
    DEPENDS ux_mirror_launcher
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Running UX-Mirror Launcher Dashboard..."
)

# Print configuration summary
message(STATUS "")
message(STATUS "=== UX-Mirror Prototype Configuration ===")
message(STATUS "Project: ${PROJECT_NAME}")
message(STATUS "Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "Vulkan Found: ${Vulkan_FOUND}")
message(STATUS "HIP Found: ${hip_FOUND}") 
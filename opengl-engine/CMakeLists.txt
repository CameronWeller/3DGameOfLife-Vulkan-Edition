# OpenGL Engine CMake Configuration

cmake_minimum_required(VERSION 3.20)

# Find OpenGL dependencies
find_package(glfw3 CONFIG REQUIRED)
find_package(glm REQUIRED)
find_package(spdlog REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(GTest REQUIRED)
find_package(benchmark REQUIRED)

# Find GLAD (via vcpkg or system)
find_path(GLAD_INCLUDE_DIR glad/glad.h)
if(NOT GLAD_INCLUDE_DIR)
    message(FATAL_ERROR "GLAD not found. Please install via vcpkg: vcpkg install glad")
endif()

# Include directories
set(OPENGL_ENGINE_INCLUDE_DIRS
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
    ${CMAKE_CURRENT_SOURCE_DIR}/../src
    ${GLAD_INCLUDE_DIR}
)

# Source files
set(OPENGL_ENGINE_SOURCES
    src/main_opengl.cpp
    
    # Context and window
    src/opengl/OpenGLContext.cpp
    src/opengl/OpenGLWindowManager.cpp
    
    # Diagnostics
    src/opengl/diagnostics/OpenGLDebugCallback.cpp
    src/opengl/diagnostics/OpenGLProfiler.cpp
    src/opengl/diagnostics/HealthMonitor.cpp
    
    # Memory management
    src/opengl/memory/OpenGLMemoryManager.cpp
    
    # Compute pipeline
    src/opengl/compute/OpenGLCompute.cpp
    
    # Rendering
    src/opengl/rendering/OpenGLRenderer.cpp
    src/opengl/rendering/OpenGLVoxelRenderer.cpp
    
    # Game logic
    src/opengl/Grid3D.cpp
    
    # UI
    src/opengl/ui/OpenGLImGui.cpp
)

# Header files (for IDE support)
set(OPENGL_ENGINE_HEADERS
    include/opengl/OpenGLContext.h
    include/opengl/OpenGLWindowManager.h
    include/opengl/diagnostics/OpenGLDebugCallback.h
    include/opengl/diagnostics/OpenGLProfiler.h
    include/opengl/diagnostics/HealthMonitor.h
    include/opengl/memory/OpenGLMemoryManager.h
    include/opengl/compute/OpenGLCompute.h
    include/opengl/rendering/OpenGLRenderer.h
    include/opengl/rendering/OpenGLVoxelRenderer.h
    include/opengl/Grid3D.h
    include/opengl/ui/OpenGLImGui.h
)

# Create OpenGL engine executable
add_executable(${PROJECT_NAME}-OpenGL
    ${OPENGL_ENGINE_SOURCES}
    ${OPENGL_ENGINE_HEADERS}
)

# Include directories
target_include_directories(${PROJECT_NAME}-OpenGL PRIVATE
    ${OPENGL_ENGINE_INCLUDE_DIRS}
)

# Link shared components (from parent project)
target_link_libraries(${PROJECT_NAME}-OpenGL PRIVATE
    glfw
    glm::glm
    spdlog::spdlog
    nlohmann_json::nlohmann_json
    imgui::imgui
)

# Link shared components from parent (if available)
# Note: These may need to be linked as static libraries or headers-only
# For now, we'll link against the source files directly
if(TARGET game_logic)
    target_link_libraries(${PROJECT_NAME}-OpenGL PRIVATE game_logic)
endif()

if(TARGET camera)
    target_link_libraries(${PROJECT_NAME}-OpenGL PRIVATE camera)
endif()

if(TARGET utilities)
    target_link_libraries(${PROJECT_NAME}-OpenGL PRIVATE utilities)
endif()

# Apply common settings from parent
if(COMMAND apply_common_settings)
    apply_common_settings(${PROJECT_NAME}-OpenGL)
endif()

# Platform-specific OpenGL linking
if(WIN32)
    # Windows: OpenGL is provided by system (opengl32.lib)
    # GLAD will load OpenGL functions dynamically
    # No need to link opengl32.lib when using GLAD
elseif(APPLE)
    # macOS: OpenGL framework (deprecated, but still used)
    find_library(OPENGL_LIBRARY OpenGL)
    if(OPENGL_LIBRARY)
        target_link_libraries(${PROJECT_NAME}-OpenGL PRIVATE ${OPENGL_LIBRARY})
    endif()
else()
    # Linux: Use pkg-config or system OpenGL
    find_package(OpenGL REQUIRED)
    if(OpenGL_FOUND)
        target_link_libraries(${PROJECT_NAME}-OpenGL PRIVATE OpenGL::GL)
    else()
        # Fallback: try to find OpenGL libraries manually
        find_library(OPENGL_LIBRARY GL)
        if(OPENGL_LIBRARY)
            target_link_libraries(${PROJECT_NAME}-OpenGL PRIVATE ${OPENGL_LIBRARY})
        endif()
    endif()
endif()

# Link GLAD (if available as library)
# Note: GLAD is typically header-only, but some packages provide it as a library
find_library(GLAD_LIBRARY glad)
if(GLAD_LIBRARY)
    target_link_libraries(${PROJECT_NAME}-OpenGL PRIVATE ${GLAD_LIBRARY})
endif()

# Compiler definitions
target_compile_definitions(${PROJECT_NAME}-OpenGL PRIVATE
    GLFW_INCLUDE_NONE
    # GLAD_GL_IMPLEMENTATION should be defined in a source file, not here
)

# GLAD implementation should be in a source file
# Create glad.c if needed, or use GLAD generated code
# For now, we'll assume GLAD is header-only with implementation in source

message(STATUS "OpenGL Engine executable: ${PROJECT_NAME}-OpenGL")

# Testing configuration
if(BUILD_TESTING)
    enable_testing()
    
    # Find test sources
    file(GLOB_RECURSE OPENGL_SMOKE_TESTS
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/smoke/*.cpp"
    )
    
    file(GLOB_RECURSE OPENGL_UNIT_TESTS
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/unit/*.cpp"
    )
    
    file(GLOB_RECURSE OPENGL_INTEGRATION_TESTS
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/*.cpp"
    )
    
    file(GLOB_RECURSE OPENGL_E2E_TESTS
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/e2e/*.cpp"
    )
    
    file(GLOB_RECURSE OPENGL_PERFORMANCE_TESTS
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/performance/*.cpp"
    )
    
    file(GLOB_RECURSE OPENGL_VISUAL_REGRESSION_TESTS
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/visual_regression/*.cpp"
    )
    
    # Create test executables
    if(OPENGL_SMOKE_TESTS)
        add_executable(opengl_smoke_tests ${OPENGL_SMOKE_TESTS})
        target_link_libraries(opengl_smoke_tests PRIVATE
            GTest::gtest
            GTest::gtest_main
            glfw
            glm::glm
        )
        target_include_directories(opengl_smoke_tests PRIVATE ${OPENGL_ENGINE_INCLUDE_DIRS})
        if(COMMAND apply_common_settings)
            apply_common_settings(opengl_smoke_tests)
        endif()
        add_test(NAME OpenGLSmokeTests COMMAND opengl_smoke_tests)
        set_tests_properties(OpenGLSmokeTests PROPERTIES LABELS "smoke;opengl")
    endif()
    
    if(OPENGL_UNIT_TESTS)
        add_executable(opengl_unit_tests ${OPENGL_UNIT_TESTS})
        target_link_libraries(opengl_unit_tests PRIVATE
            GTest::gtest
            GTest::gtest_main
            glfw
            glm::glm
        )
        target_include_directories(opengl_unit_tests PRIVATE ${OPENGL_ENGINE_INCLUDE_DIRS})
        # Link against OpenGL engine sources for unit testing
        target_sources(opengl_unit_tests PRIVATE ${OPENGL_ENGINE_SOURCES})
        if(COMMAND apply_common_settings)
            apply_common_settings(opengl_unit_tests)
        endif()
        add_test(NAME OpenGLUnitTests COMMAND opengl_unit_tests)
        set_tests_properties(OpenGLUnitTests PROPERTIES LABELS "unit;opengl")
    endif()
    
    if(OPENGL_INTEGRATION_TESTS)
        add_executable(opengl_integration_tests ${OPENGL_INTEGRATION_TESTS})
        target_link_libraries(opengl_integration_tests PRIVATE
            GTest::gtest
            GTest::gtest_main
            glfw
            glm::glm
        )
        target_include_directories(opengl_integration_tests PRIVATE ${OPENGL_ENGINE_INCLUDE_DIRS})
        target_sources(opengl_integration_tests PRIVATE ${OPENGL_ENGINE_SOURCES})
        if(COMMAND apply_common_settings)
            apply_common_settings(opengl_integration_tests)
        endif()
        add_test(NAME OpenGLIntegrationTests COMMAND opengl_integration_tests)
        set_tests_properties(OpenGLIntegrationTests PROPERTIES LABELS "integration;opengl")
    endif()
    
    if(OPENGL_E2E_TESTS)
        add_executable(opengl_e2e_tests ${OPENGL_E2E_TESTS})
        target_link_libraries(opengl_e2e_tests PRIVATE
            GTest::gtest
            GTest::gtest_main
            glfw
            glm::glm
            imgui::imgui
        )
        target_include_directories(opengl_e2e_tests PRIVATE ${OPENGL_ENGINE_INCLUDE_DIRS})
        target_sources(opengl_e2e_tests PRIVATE ${OPENGL_ENGINE_SOURCES})
        if(COMMAND apply_common_settings)
            apply_common_settings(opengl_e2e_tests)
        endif()
        add_test(NAME OpenGLE2ETests COMMAND opengl_e2e_tests)
        set_tests_properties(OpenGLE2ETests PROPERTIES LABELS "e2e;opengl")
    endif()
    
    if(OPENGL_PERFORMANCE_TESTS AND benchmark_FOUND)
        add_executable(opengl_performance_tests ${OPENGL_PERFORMANCE_TESTS})
        target_link_libraries(opengl_performance_tests PRIVATE
            benchmark::benchmark
            benchmark::benchmark_main
            glfw
            glm::glm
        )
        target_include_directories(opengl_performance_tests PRIVATE ${OPENGL_ENGINE_INCLUDE_DIRS})
        target_sources(opengl_performance_tests PRIVATE ${OPENGL_ENGINE_SOURCES})
        if(COMMAND apply_common_settings)
            apply_common_settings(opengl_performance_tests)
        endif()
        add_test(NAME OpenGLPerformanceTests COMMAND opengl_performance_tests)
        set_tests_properties(OpenGLPerformanceTests PROPERTIES LABELS "performance;opengl")
    endif()
    
    if(OPENGL_VISUAL_REGRESSION_TESTS)
        add_executable(opengl_visual_regression_tests ${OPENGL_VISUAL_REGRESSION_TESTS})
        target_link_libraries(opengl_visual_regression_tests PRIVATE
            GTest::gtest
            GTest::gtest_main
            glfw
            glm::glm
        )
        target_include_directories(opengl_visual_regression_tests PRIVATE ${OPENGL_ENGINE_INCLUDE_DIRS})
        target_sources(opengl_visual_regression_tests PRIVATE ${OPENGL_ENGINE_SOURCES})
        if(COMMAND apply_common_settings)
            apply_common_settings(opengl_visual_regression_tests)
        endif()
        add_test(NAME OpenGLVisualRegressionTests COMMAND opengl_visual_regression_tests)
        set_tests_properties(OpenGLVisualRegressionTests PROPERTIES LABELS "visual_regression;opengl")
    endif()
    
    message(STATUS "OpenGL Engine tests configured")
endif()

# Copy shaders to output directory
file(GLOB_RECURSE OPENGL_SHADERS
    "${CMAKE_CURRENT_SOURCE_DIR}/shaders/*.comp"
    "${CMAKE_CURRENT_SOURCE_DIR}/shaders/*.vert"
    "${CMAKE_CURRENT_SOURCE_DIR}/shaders/*.frag"
)

foreach(shader ${OPENGL_SHADERS})
    get_filename_component(shader_name ${shader} NAME)
    configure_file(${shader} ${CMAKE_CURRENT_BINARY_DIR}/shaders/${shader_name} COPYONLY)
endforeach()

message(STATUS "OpenGL Engine shaders configured")

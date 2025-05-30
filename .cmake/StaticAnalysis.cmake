# Static Analysis Configuration
# This module sets up various static analysis tools for the project

include(CMakeDependentOption)

# Enable static analysis tools
option(ENABLE_STATIC_ANALYSIS "Enable static analysis tools" ON)

# Clang-Tidy configuration
find_program(CLANG_TIDY_EXE NAMES "clang-tidy" DOC "Path to clang-tidy executable")
if(CLANG_TIDY_EXE)
    set(CMAKE_CXX_CLANG_TIDY
        ${CLANG_TIDY_EXE};
        -checks=*,-fuchsia-*,-google-*,-zircon-*,-abseil-*,-modernize-use-trailing-return-type;
        -warnings-as-errors=*,-fuchsia-*,-google-*,-zircon-*,-abseil-*,-modernize-use-trailing-return-type;
        -header-filter=.*
    )
endif()

# Cppcheck configuration
find_program(CPPCHECK_EXE NAMES "cppcheck" DOC "Path to cppcheck executable")
if(CPPCHECK_EXE)
    set(CMAKE_CXX_CPPCHECK
        ${CPPCHECK_EXE};
        --enable=all;
        --suppress=missingIncludeSystem;
        --suppress=unusedFunction;
        --suppress=unmatchedSuppression;
        --inline-suppr;
        --std=c++17;
        -I${CMAKE_SOURCE_DIR}/include
    )
endif()

# Include-what-you-use configuration
find_program(IWYU_EXE NAMES "include-what-you-use" DOC "Path to include-what-you-use executable")
if(IWYU_EXE)
    set(CMAKE_CXX_INCLUDE_WHAT_YOU_USE
        ${IWYU_EXE};
        -Xiwyu;--mapping_file=${CMAKE_SOURCE_DIR}/.cmake/iwyu.imp;
        -Xiwyu;--no_fwd_decls;
        -Xiwyu;--max_line_length=100
    )
endif()

# SonarQube configuration
option(ENABLE_SONARQUBE "Enable SonarQube analysis" OFF)
if(ENABLE_SONARQUBE)
    find_program(SONAR_SCANNER_EXE NAMES "sonar-scanner" DOC "Path to sonar-scanner executable")
    if(SONAR_SCANNER_EXE)
        set(SONAR_PROJECT_KEY "3DGameOfLife-Vulkan-Edition")
        set(SONAR_PROJECT_NAME "3D Game of Life - Vulkan Edition")
        set(SONAR_PROJECT_VERSION "1.0.0")
        set(SONAR_SOURCES "${CMAKE_SOURCE_DIR}/src")
        set(SONAR_TESTS "${CMAKE_SOURCE_DIR}/tests")
        set(SONAR_CPP_COMPILER_REPORT "${CMAKE_BINARY_DIR}/compile_commands.json")
    endif()
endif()

# Function to add static analysis targets
function(add_static_analysis_targets)
    if(ENABLE_STATIC_ANALYSIS)
        # Clang-Tidy target
        if(CLANG_TIDY_EXE)
            add_custom_target(clang-tidy
                COMMAND ${CLANG_TIDY_EXE}
                -p ${CMAKE_BINARY_DIR}
                -checks=*,-fuchsia-*,-google-*,-zircon-*,-abseil-*,-modernize-use-trailing-return-type
                -warnings-as-errors=*,-fuchsia-*,-google-*,-zircon-*,-abseil-*,-modernize-use-trailing-return-type
                -header-filter=.*
                ${CMAKE_SOURCE_DIR}/src
                ${CMAKE_SOURCE_DIR}/include
                ${CMAKE_SOURCE_DIR}/tests
                COMMENT "Running clang-tidy"
            )
        endif()

        # Cppcheck target
        if(CPPCHECK_EXE)
            add_custom_target(cppcheck
                COMMAND ${CPPCHECK_EXE}
                --enable=all
                --suppress=missingIncludeSystem
                --suppress=unusedFunction
                --suppress=unmatchedSuppression
                --inline-suppr
                --std=c++17
                -I${CMAKE_SOURCE_DIR}/include
                ${CMAKE_SOURCE_DIR}/src
                ${CMAKE_SOURCE_DIR}/include
                ${CMAKE_SOURCE_DIR}/tests
                COMMENT "Running cppcheck"
            )
        endif()

        # Include-what-you-use target
        if(IWYU_EXE)
            add_custom_target(iwyu
                COMMAND ${IWYU_EXE}
                -p ${CMAKE_BINARY_DIR}
                -Xiwyu --mapping_file=${CMAKE_SOURCE_DIR}/.cmake/iwyu.imp
                -Xiwyu --no_fwd_decls
                -Xiwyu --max_line_length=100
                ${CMAKE_SOURCE_DIR}/src
                ${CMAKE_SOURCE_DIR}/include
                ${CMAKE_SOURCE_DIR}/tests
                COMMENT "Running include-what-you-use"
            )
        endif()

        # SonarQube target
        if(ENABLE_SONARQUBE AND SONAR_SCANNER_EXE)
            add_custom_target(sonarqube
                COMMAND ${SONAR_SCANNER_EXE}
                -Dsonar.projectKey=${SONAR_PROJECT_KEY}
                -Dsonar.projectName=${SONAR_PROJECT_NAME}
                -Dsonar.projectVersion=${SONAR_PROJECT_VERSION}
                -Dsonar.sources=${SONAR_SOURCES}
                -Dsonar.tests=${SONAR_TESTS}
                -Dsonar.cpp.compiler.reportPaths=${SONAR_CPP_COMPILER_REPORT}
                COMMENT "Running SonarQube analysis"
            )
        endif()

        # Combined static analysis target
        add_custom_target(static-analysis
            DEPENDS clang-tidy cppcheck iwyu
            COMMENT "Running all static analysis tools"
        )
    endif()
endfunction() 
# Static Analysis Configuration Module
# This module handles all static analysis tools configuration

# Enable static analysis if requested
if(ENABLE_STATIC_ANALYSIS)
    # Find Clang-Tidy
    find_program(CLANG_TIDY_EXECUTABLE NAMES clang-tidy)
    if(CLANG_TIDY_EXECUTABLE)
        message(STATUS "Found clang-tidy: ${CLANG_TIDY_EXECUTABLE}")
        set(CLANG_TIDY_CHECKS "-checks=*,\
            -modernize-use-trailing-return-type,\
            -fuchsia-*,\
            -google-*,\
            -llvm-*,\
            -hicpp-*,\
            -readability-identifier-length,\
            -readability-magic-numbers,\
            -cppcoreguidelines-avoid-magic-numbers,\
            -cppcoreguidelines-pro-bounds-pointer-arithmetic,\
            -cppcoreguidelines-pro-type-reinterpret-cast,\
            -cppcoreguidelines-pro-type-union-access,\
            -cppcoreguidelines-pro-bounds-constant-array-index\
        ")
        set(CMAKE_CXX_CLANG_TIDY "${CLANG_TIDY_EXECUTABLE};${CLANG_TIDY_CHECKS}")
    else()
        message(STATUS "clang-tidy not found")
    endif()
    
    # Find Cppcheck
    find_program(CPPCHECK_EXECUTABLE NAMES cppcheck)
    if(CPPCHECK_EXECUTABLE)
        message(STATUS "Found cppcheck: ${CPPCHECK_EXECUTABLE}")
        set(CPPCHECK_ARGS "--enable=all --suppress=missingIncludeSystem --inline-suppr --inconclusive")
        set(CMAKE_CXX_CPPCHECK "${CPPCHECK_EXECUTABLE};${CPPCHECK_ARGS}")
    else()
        message(STATUS "cppcheck not found")
    endif()
    
    # Find Include-what-you-use
    find_program(IWYU_EXECUTABLE NAMES include-what-you-use iwyu)
    if(IWYU_EXECUTABLE)
        message(STATUS "Found include-what-you-use: ${IWYU_EXECUTABLE}")
        set(CMAKE_CXX_INCLUDE_WHAT_YOU_USE ${IWYU_EXECUTABLE})
    else()
        message(STATUS "include-what-you-use not found")
    endif()
    
    # SonarQube analysis
    if(ENABLE_SONARQUBE)
        find_program(SONAR_SCANNER_EXECUTABLE NAMES sonar-scanner)
        if(SONAR_SCANNER_EXECUTABLE)
            message(STATUS "Found sonar-scanner: ${SONAR_SCANNER_EXECUTABLE}")
            
            # Create SonarQube analysis target
            add_custom_target(sonarqube
                COMMAND ${SONAR_SCANNER_EXECUTABLE}
                    -Dsonar.projectKey=${PROJECT_NAME}
                    -Dsonar.projectName=${PROJECT_NAME}
                    -Dsonar.projectVersion=${PROJECT_VERSION}
                    -Dsonar.sources=${CMAKE_CURRENT_SOURCE_DIR}/src,${CMAKE_CURRENT_SOURCE_DIR}/include
                    -Dsonar.cfamily.build-wrapper-output=${CMAKE_CURRENT_BINARY_DIR}/bw-output
                    -Dsonar.cfamily.threads=${CMAKE_BUILD_PARALLEL_LEVEL}
                    -Dsonar.cfamily.cache.enabled=true
                    -Dsonar.cfamily.cache.path=${CMAKE_CURRENT_BINARY_DIR}/sonar-cache
                WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                COMMENT "Running SonarQube analysis"
            )
        else()
            message(STATUS "sonar-scanner not found")
        endif()
    endif()
endif()

# Function to apply static analysis to a target
function(apply_static_analysis target_name)
    if(NOT ENABLE_STATIC_ANALYSIS)
        return()
    endif()
    
    # Apply clang-tidy to target
    if(CLANG_TIDY_EXECUTABLE)
        set_target_properties(${target_name} PROPERTIES
            CXX_CLANG_TIDY "${CMAKE_CXX_CLANG_TIDY}"
        )
    endif()
    
    # Apply cppcheck to target
    if(CPPCHECK_EXECUTABLE)
        set_target_properties(${target_name} PROPERTIES
            CXX_CPPCHECK "${CMAKE_CXX_CPPCHECK}"
        )
    endif()
    
    # Apply include-what-you-use to target
    if(IWYU_EXECUTABLE)
        set_target_properties(${target_name} PROPERTIES
            CXX_INCLUDE_WHAT_YOU_USE "${CMAKE_CXX_INCLUDE_WHAT_YOU_USE}"
        )
    endif()
endfunction()

# Function to add static analysis targets
function(add_static_analysis_targets)
    if(NOT ENABLE_STATIC_ANALYSIS)
        return()
    endif()
    
    # Add clang-tidy target
    if(CLANG_TIDY_EXECUTABLE)
        add_custom_target(clang-tidy
            COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target ${PROJECT_NAME} -- -j1
            COMMENT "Running clang-tidy on all source files"
        )
    endif()
    
    # Add cppcheck target
    if(CPPCHECK_EXECUTABLE)
        add_custom_target(cppcheck
            COMMAND ${CPPCHECK_EXECUTABLE} --enable=all --suppress=missingIncludeSystem --xml --xml-version=2 
                    --output-file=${CMAKE_BINARY_DIR}/cppcheck-report.xml 
                    ${CMAKE_CURRENT_SOURCE_DIR}/src ${CMAKE_CURRENT_SOURCE_DIR}/include
            COMMENT "Running cppcheck on all source files"
        )
    endif()
    
    # Add include-what-you-use target
    if(IWYU_EXECUTABLE)
        add_custom_target(iwyu
            COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target ${PROJECT_NAME} -- -j1
            COMMENT "Running include-what-you-use on all source files"
        )
    endif()
endfunction()

# Function to enable sanitizers
function(enable_sanitizers target_name)
    if(NOT ENABLE_SANITIZERS)
        return()
    endif()
    
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        # Address sanitizer
        option(ENABLE_ASAN "Enable address sanitizer" ON)
        if(ENABLE_ASAN)
            target_compile_options(${target_name} PRIVATE -fsanitize=address -fno-omit-frame-pointer)
            target_link_options(${target_name} PRIVATE -fsanitize=address)
        endif()
        
        # Undefined behavior sanitizer
        option(ENABLE_UBSAN "Enable undefined behavior sanitizer" ON)
        if(ENABLE_UBSAN)
            target_compile_options(${target_name} PRIVATE -fsanitize=undefined)
            target_link_options(${target_name} PRIVATE -fsanitize=undefined)
        endif()
        
        # Thread sanitizer
        option(ENABLE_TSAN "Enable thread sanitizer" OFF)
        if(ENABLE_TSAN)
            target_compile_options(${target_name} PRIVATE -fsanitize=thread)
            target_link_options(${target_name} PRIVATE -fsanitize=thread)
        endif()
        
        # Memory sanitizer (Clang only)
        if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
            option(ENABLE_MSAN "Enable memory sanitizer" OFF)
            if(ENABLE_MSAN)
                target_compile_options(${target_name} PRIVATE -fsanitize=memory -fno-omit-frame-pointer)
                target_link_options(${target_name} PRIVATE -fsanitize=memory)
            endif()
        endif()
    endif()
endfunction()
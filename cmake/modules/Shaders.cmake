# Shader Compilation Module
# This module handles all shader compilation and management

# Function to compile shaders
function(compile_shaders)
    set(SHADER_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/shaders)
    set(SHADER_BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR}/shaders)
    
    # Create output directory
    file(MAKE_DIRECTORY ${SHADER_BINARY_DIR})
    
    # Find all shader files
    file(GLOB_RECURSE SHADER_FILES
        "${SHADER_SOURCE_DIR}/*.vert"
        "${SHADER_SOURCE_DIR}/*.frag"
        "${SHADER_SOURCE_DIR}/*.comp"
        "${SHADER_SOURCE_DIR}/*.geom"
        "${SHADER_SOURCE_DIR}/*.tesc"
        "${SHADER_SOURCE_DIR}/*.tese"
    )
    
    set(COMPILED_SHADERS)
    
    foreach(SHADER_FILE ${SHADER_FILES})
        # Get relative path and create output path
        file(RELATIVE_PATH SHADER_REL_PATH ${SHADER_SOURCE_DIR} ${SHADER_FILE})
        set(SHADER_OUTPUT ${SHADER_BINARY_DIR}/${SHADER_REL_PATH}.spv)
        
        # Get output directory and create it
        get_filename_component(SHADER_OUTPUT_DIR ${SHADER_OUTPUT} DIRECTORY)
        file(MAKE_DIRECTORY ${SHADER_OUTPUT_DIR})
        
        # Add compilation command
        add_custom_command(
            OUTPUT ${SHADER_OUTPUT}
            COMMAND ${GLSLC_EXECUTABLE} -o ${SHADER_OUTPUT} ${SHADER_FILE}
            DEPENDS ${SHADER_FILE}
            COMMENT "Compiling shader: ${SHADER_REL_PATH}"
            VERBATIM
        )
        
        list(APPEND COMPILED_SHADERS ${SHADER_OUTPUT})
    endforeach()
    
    # Create shader compilation target
    add_custom_target(compile_shaders ALL
        DEPENDS ${COMPILED_SHADERS}
        COMMENT "Compiling all shaders"
    )
    
    # Set shader binary directory for use by other modules
    set(SHADER_BINARY_DIR ${SHADER_BINARY_DIR} PARENT_SCOPE)
endfunction()

# Function to copy shaders to output directory
function(copy_shaders_to_output)
    if(NOT DEFINED SHADER_BINARY_DIR)
        message(WARNING "SHADER_BINARY_DIR not defined. Call compile_shaders() first.")
        return()
    endif()
    
    # Copy compiled shaders to output directory
    add_custom_target(copy_shaders ALL
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${SHADER_BINARY_DIR}
            $<TARGET_FILE_DIR:${PROJECT_NAME}>/shaders
        DEPENDS compile_shaders
        COMMENT "Copying shaders to output directory"
    )
endfunction()

# Function to validate shader compilation
function(validate_shaders)
    if(NOT GLSLC_EXECUTABLE)
        message(FATAL_ERROR "glslc compiler not found. Cannot validate shaders.")
    endif()
    
    # Add a test target for shader validation
    add_custom_target(validate_shaders
        COMMAND ${CMAKE_COMMAND} -E echo "Validating shader compilation..."
        DEPENDS compile_shaders
        COMMENT "Validating compiled shaders"
    )
endfunction()

# Function to clean compiled shaders
function(add_shader_clean_target)
    add_custom_target(clean_shaders
        COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_CURRENT_BINARY_DIR}/shaders
        COMMENT "Cleaning compiled shaders"
    )
endfunction()
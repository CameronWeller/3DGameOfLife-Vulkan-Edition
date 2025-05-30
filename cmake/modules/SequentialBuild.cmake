# Sequential build and test configuration

# Function to build and test a single component
function(build_and_test_component COMPONENT_NAME)
    message(STATUS "Building component: ${COMPONENT_NAME}")
    
    # Build the component
    add_custom_target(build_${COMPONENT_NAME}
        COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target ${COMPONENT_NAME}
        COMMENT "Building ${COMPONENT_NAME} component"
    )
    
    # Test the component if tests exist
    if(BUILD_TESTING)
        add_custom_target(test_${COMPONENT_NAME}
            COMMAND ${CMAKE_CTEST_COMMAND} -R "${COMPONENT_NAME}" --output-on-failure
            DEPENDS build_${COMPONENT_NAME}
            COMMENT "Testing ${COMPONENT_NAME} component"
        )
    endif()
endfunction()

# Function to create sequential build targets
function(create_sequential_build_targets)
    set(COMPONENTS
        utilities
        core_engine
        memory_management
        rendering
        game_logic
        camera
        ui
    )
    
    set(PREV_TARGET "")
    
    foreach(COMPONENT ${COMPONENTS})
        build_and_test_component(${COMPONENT})
        
        if(PREV_TARGET)
            if(BUILD_TESTING)
                add_dependencies(build_${COMPONENT} test_${PREV_TARGET})
            else()
                add_dependencies(build_${COMPONENT} build_${PREV_TARGET})
            endif()
        endif()
        
        set(PREV_TARGET ${COMPONENT})
    endforeach()
    
    # Create master sequential build target
    add_custom_target(build_sequential
        DEPENDS build_ui
        COMMENT "Building all components sequentially"
    )
    
    if(BUILD_TESTING)
        add_custom_target(test_sequential
            DEPENDS test_ui
            COMMENT "Building and testing all components sequentially"
        )
    endif()
endfunction()
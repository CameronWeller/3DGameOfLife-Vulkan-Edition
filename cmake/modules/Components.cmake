# This module defines all component libraries for modular architecture

# Core Engine Component
function(create_core_engine_component)
    set(CORE_ENGINE_SOURCES
        # src/VulkanEngine.cpp  # Temporarily disabled for minimal build
        src/VulkanContext.cpp
        src/DeviceManager.cpp
        src/WindowManager.cpp
    )
    
    set(CORE_ENGINE_HEADERS
        include/VulkanEngine.h
        include/VulkanContext.h
        include/DeviceManager.h
        include/WindowManager.h
        include/QueueFamilyIndices.h
        include/SwapChainSupportDetails.h
        include/VulkanError.h
        include/AppState.h
    )
    
    add_library(core_engine STATIC ${CORE_ENGINE_SOURCES} ${CORE_ENGINE_HEADERS})
    apply_common_settings(core_engine)
    target_link_libraries(core_engine PUBLIC project_dependencies)
    
    # Export target
    set_target_properties(core_engine PROPERTIES EXPORT_NAME CoreEngine)
endfunction()

# Memory Management Component
function(create_memory_component)
    set(MEMORY_SOURCES
        src/VulkanMemoryManager.cpp
    )
    
    set(MEMORY_HEADERS
        include/VulkanMemoryManager.h
        include/ThreadSafe.h
    )
    
    add_library(memory_management STATIC ${MEMORY_SOURCES} ${MEMORY_HEADERS})
    apply_common_settings(memory_management)
    target_link_libraries(memory_management PUBLIC core_engine project_dependencies)
    
    set_target_properties(memory_management PROPERTIES EXPORT_NAME MemoryManagement)
endfunction()

# Rendering Component
function(create_rendering_component)
    set(RENDERING_SOURCES
        src/RayCaster.cpp
    )
    
    set(RENDERING_HEADERS
        include/Vertex.h
        include/RayCaster.h
    )
    
    add_library(rendering STATIC ${RENDERING_SOURCES} ${RENDERING_HEADERS})
    apply_common_settings(rendering)
    target_link_libraries(rendering PUBLIC core_engine memory_management project_dependencies)
    
    set_target_properties(rendering PROPERTIES EXPORT_NAME Rendering)
endfunction()

# Game Logic Component
function(create_game_logic_component)
    set(GAME_LOGIC_SOURCES
        src/Grid3D.cpp
        src/SparseGrid3D.cpp
        src/VoxelData.cpp
        src/RuleAnalyzer.cpp
        src/PatternManager.cpp
        src/SaveManager.cpp
    )
    
    set(GAME_LOGIC_HEADERS
        include/Grid3D.h
        include/SparseGrid3D.h
        include/VoxelData.h
        include/GameRules.h
        include/RuleAnalyzer.h
        include/PatternManager.h
        include/PatternMetadata.h
        include/SaveManager.h
    )
    
    add_library(game_logic STATIC ${GAME_LOGIC_SOURCES} ${GAME_LOGIC_HEADERS})
    apply_common_settings(game_logic)
    target_link_libraries(game_logic PUBLIC core_engine memory_management project_dependencies)
    
    set_target_properties(game_logic PROPERTIES EXPORT_NAME GameLogic)
endfunction()

# Camera Component
function(create_camera_component)
    set(CAMERA_SOURCES
        src/Camera.cpp
    )
    
    set(CAMERA_HEADERS
        include/Camera.h
    )
    
    add_library(camera STATIC ${CAMERA_SOURCES} ${CAMERA_HEADERS})
    apply_common_settings(camera)
    target_link_libraries(camera PUBLIC game_logic project_dependencies)
    
    set_target_properties(camera PROPERTIES EXPORT_NAME Camera)
endfunction()

# UI Component
function(create_ui_component)
    set(UI_SOURCES
        src/UI.cpp
        src/imgui_impl_glfw.cpp
        src/imgui_impl_vulkan.cpp
    )
    
    set(UI_HEADERS
        include/UI.h
        include/imgui_impl_glfw.h
        include/imgui_impl_vulkan.h
    )
    
    add_library(ui STATIC ${UI_SOURCES} ${UI_HEADERS})
    apply_common_settings(ui)
    target_link_libraries(ui PUBLIC rendering project_dependencies)
    
    set_target_properties(ui PROPERTIES EXPORT_NAME UI)
endfunction()

# Utilities Component
function(create_utilities_component)
    set(UTILITIES_SOURCES
        src/Logger.cpp
    )
    
    set(UTILITIES_HEADERS
        include/Logger.h
    )
    
    add_library(utilities STATIC ${UTILITIES_SOURCES} ${UTILITIES_HEADERS})
    apply_common_settings(utilities)
    target_link_libraries(utilities PUBLIC project_dependencies)
    
    set_target_properties(utilities PROPERTIES EXPORT_NAME Utilities)
endfunction()

# Create vulkan resources component
function(create_vulkan_resources_component)
    set(VULKAN_RESOURCES_SOURCES
        src/vulkan/resources/VulkanBufferManager.cpp
        src/vulkan/resources/VulkanImageManager.cpp
        src/vulkan/resources/VulkanSwapChain.cpp
        src/vulkan/resources/VulkanFramebuffer.cpp
    )
    
    add_library(vulkan_resources STATIC ${VULKAN_RESOURCES_SOURCES})
    target_include_directories(vulkan_resources PUBLIC ${CMAKE_SOURCE_DIR}/include)
    target_link_libraries(vulkan_resources PUBLIC Vulkan::Vulkan glm::glm GPUOpen::VulkanMemoryAllocator)
endfunction()

# Add after existing vulkan_resources component

# Vulkan Image Management Component
function(create_vulkan_image_component)
    add_library(vulkan_image STATIC
        src/vulkan/resources/VulkanImageManager.cpp
    )
    
    target_include_directories(vulkan_image PUBLIC
        include/vulkan/resources
        ${CMAKE_SOURCE_DIR}/include
    )
    
    target_link_libraries(vulkan_image PUBLIC
        core_engine
        memory_management
        ${Vulkan_LIBRARIES}
    )
endfunction()

# Vulkan Rendering Components
function(create_vulkan_rendering_component)
    add_library(vulkan_rendering STATIC
        src/vulkan/rendering/VulkanSwapChain.cpp
        src/vulkan/rendering/VulkanRenderer.cpp
        src/vulkan/rendering/VoxelRenderer.cpp
    )
    
    target_include_directories(vulkan_rendering PUBLIC
        include/vulkan/rendering
        ${CMAKE_SOURCE_DIR}/include
    )
    
    target_link_libraries(vulkan_rendering PUBLIC
        core_engine
        vulkan_resources
        vulkan_image
        rendering
        ${Vulkan_LIBRARIES}
    )
endfunction()

# Vulkan Compute Component
function(create_vulkan_compute_component)
    add_library(vulkan_compute STATIC
        src/vulkan/compute/VulkanCompute.cpp
    )
    
    target_include_directories(vulkan_compute PUBLIC
        include/vulkan/compute
        ${CMAKE_SOURCE_DIR}/include
    )
    
    target_link_libraries(vulkan_compute PUBLIC
        core_engine
        game_logic
        ${Vulkan_LIBRARIES}
    )
endfunction()

# Vulkan UI Component
function(create_vulkan_ui_component)
    add_library(vulkan_ui STATIC
        src/vulkan/ui/VulkanImGui.cpp
    )
    
    target_include_directories(vulkan_ui PUBLIC
        include/vulkan/ui
        ${CMAKE_SOURCE_DIR}/include
    )
    
    target_link_libraries(vulkan_ui PUBLIC
        core_engine
        ${Vulkan_LIBRARIES}
        imgui
    )
endfunction()

# Update create_all_components function
function(create_all_components)
    create_core_engine_component()
    create_memory_management_component()
    create_rendering_component()
    create_game_logic_component()
    create_vulkan_resources_component()
    create_vulkan_image_component()
    create_vulkan_rendering_component()
    create_vulkan_compute_component()
    create_vulkan_ui_component()
endfunction()

function(create_vulkan_ui_component)
    set(VULKAN_UI_SOURCES
        src/vulkan/ui/VulkanImGui.cpp
        src/ui/MenuSystem.cpp
    )
    
    add_library(vulkan_ui STATIC ${VULKAN_UI_SOURCES})
    target_include_directories(vulkan_ui PUBLIC ${CMAKE_SOURCE_DIR}/include)
    target_link_libraries(vulkan_ui PUBLIC imgui::imgui glfw)
endfunction()

# Function to create all components
function(create_all_components)
    create_utilities_component()
    create_core_engine_component()
    create_memory_component()
    create_rendering_component()
    create_game_logic_component()
    create_camera_component()
    create_ui_component()
    create_vulkan_resources_component()
    create_vulkan_rendering_component()
    create_vulkan_compute_component()
    create_vulkan_ui_component()
endfunction()

# Function to create a unified library from all components
function(create_unified_library)
    add_library(${PROJECT_NAME}_lib STATIC)
    target_link_libraries(${PROJECT_NAME}_lib PUBLIC
        utilities
        core_engine
        memory_management
        rendering
        game_logic
        camera
        ui
        vulkan_resources
        vulkan_rendering
        vulkan_compute
        vulkan_ui
    )
    
    apply_common_settings(${PROJECT_NAME}_lib)
    set_target_properties(${PROJECT_NAME}_lib PROPERTIES EXPORT_NAME UnifiedLib)
endfunction()
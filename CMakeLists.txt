# gol3d: the 3D Game of Life, built with GLFW, GLM, Dear ImGui and volk. Vulkan is
# loaded at runtime through volk, so building needs only the Vulkan headers
# (fetched) and a GLSL compiler; one binary runs on any machine with a Vulkan
# driver. Dependencies come from pinned FetchContent downloads unless system
# GLFW/GLM are found and GOL3D_BUNDLE_DEPS is OFF.
#
#   cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release   (or: cmake --preset release)
#   cmake --build build
#   ctest --test-dir build -LE gpu
#   ./build/gol3d
#
# Packages: cpack --config build/CPackConfig.cmake -G "DEB;RPM;TGZ"
# (NSIS/ZIP on Windows, DragNDrop on macOS). See docs/PROTOTYPE.md.
cmake_minimum_required(VERSION 3.20)
if(POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW) # extracted files get the extraction time
endif()

set(GOL_ROOT ${CMAKE_CURRENT_SOURCE_DIR})
file(STRINGS ${GOL_ROOT}/VERSION GOL3D_DEFAULT_VERSION LIMIT_COUNT 1)
string(STRIP "${GOL3D_DEFAULT_VERSION}" GOL3D_DEFAULT_VERSION)
set(GOL3D_VERSION ${GOL3D_DEFAULT_VERSION} CACHE STRING "Version stamped into packages (x.y.z)")
project(gol3d VERSION ${GOL3D_VERSION} LANGUAGES C CXX
    DESCRIPTION "Minecraft-style 3D Game of Life on Vulkan"
    HOMEPAGE_URL "https://github.com/CameronWeller/3DGameOfLife-Vulkan-Edition")

option(GOL3D_BUNDLE_DEPS "Always build GLFW and GLM from pinned sources (portable release builds)" OFF)
set(GOL3D_SHADER_DIR "" CACHE PATH "Use prebuilt .spv shaders from this directory instead of compiling with glslc")
set(GOL3D_MOLTENVK_DYLIB "" CACHE FILEPATH "macOS: libMoltenVK.dylib to bundle into the app")
set(GOL3D_PACKAGE_PLATFORM "${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}" CACHE STRING
    "Platform suffix for archive and installer names")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# compile_commands.json for clangd, clang-tidy and other tools.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release)
endif()
if(APPLE AND NOT CMAKE_OSX_DEPLOYMENT_TARGET)
    set(CMAKE_OSX_DEPLOYMENT_TARGET 12.0) # the bundled MoltenVK 1.4 needs macOS 12
endif()

# Release builds: link-time optimization across the game and its libraries, and
# unused functions and data dropped at link time.
option(GOL3D_LTO "Link-time optimization for optimized builds" ON)
if(GOL3D_LTO)
    include(CheckIPOSupported)
    check_ipo_supported(RESULT GOL3D_IPO_SUPPORTED LANGUAGES C CXX)
    if(GOL3D_IPO_SUPPORTED)
        set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)
        set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_MINSIZEREL ON)
        set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELWITHDEBINFO ON)
    endif()
endif()
if(APPLE)
    add_link_options($<$<NOT:$<CONFIG:Debug>>:-Wl,-dead_strip>)
elseif(NOT MSVC)
    add_compile_options(-ffunction-sections -fdata-sections)
    add_link_options($<$<NOT:$<CONFIG:Debug>>:-Wl,--gc-sections>)
endif()

include(FetchContent)

# ---------------------------------------------------------------- dependencies

FetchContent_Declare(vulkan_headers
    URL https://github.com/KhronosGroup/Vulkan-Headers/archive/refs/tags/vulkan-sdk-1.4.363.0.tar.gz
    URL_HASH SHA256=4a078be12bef21cfebc09d878b77a63cff9d68f899254a0b00d0e37ef73e7f7e)
FetchContent_Declare(volk
    URL https://github.com/zeux/volk/archive/refs/tags/vulkan-sdk-1.4.363.0.tar.gz
    URL_HASH SHA256=1547d8d74395d4048fb3f4a6313da56db626e89b55db238d0d7e8944c8a645f3
    SOURCE_SUBDIR no-cmake) # only volk.c/volk.h are used, not volk's own CMake
FetchContent_Declare(imgui
    URL https://github.com/ocornut/imgui/archive/refs/tags/v1.92.9.tar.gz
    URL_HASH SHA256=af97ed649182c39314320514a672b82008ab462b9293fe23d37b30bfa5d05519)
FetchContent_MakeAvailable(vulkan_headers volk imgui)

if(NOT GOL3D_BUNDLE_DEPS)
    find_package(glfw3 3.3 CONFIG QUIET)
    find_package(glm CONFIG QUIET)
endif()
if(NOT glfw3_FOUND)
    set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(glfw
        URL https://github.com/glfw/glfw/releases/download/3.5.1/glfw-3.5.1.zip
        URL_HASH SHA256=ea79bc5feffc254c87291980c2d0bce9acebb68c4983b79f961dcd2cb8a611a0)
    FetchContent_MakeAvailable(glfw)
endif()
if(NOT glm_FOUND)
    FetchContent_Declare(glm
        URL https://github.com/g-truc/glm/archive/refs/tags/1.0.3.tar.gz
        URL_HASH SHA256=6775e47231a446fd086d660ecc18bcd076531cfedd912fbd66e576b118607001)
    FetchContent_MakeAvailable(glm)
endif()

# volk: Vulkan entry points as runtime-loaded function pointers.
add_library(gol3d_volk STATIC ${volk_SOURCE_DIR}/volk.c)
target_include_directories(gol3d_volk PUBLIC ${volk_SOURCE_DIR})
target_compile_definitions(gol3d_volk PUBLIC VK_NO_PROTOTYPES)
target_link_libraries(gol3d_volk PUBLIC Vulkan::Headers ${CMAKE_DL_LIBS})

# Dear ImGui (menus); its Vulkan backend loads functions through volk's loader.
add_library(gol3d_imgui STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp)
target_include_directories(gol3d_imgui PUBLIC ${imgui_SOURCE_DIR} ${imgui_SOURCE_DIR}/backends)
# GLFW_INCLUDE_NONE: nothing here uses OpenGL, so the build needs no GL headers.
# The game embeds its own font and never shows ImGui's demo or debug windows, so
# those (and ImGui's shell and default-font code) are left out of the binary.
target_compile_definitions(gol3d_imgui PUBLIC IMGUI_IMPL_VULKAN_NO_PROTOTYPES GLFW_INCLUDE_NONE
    IMGUI_DISABLE_DEMO_WINDOWS IMGUI_DISABLE_DEBUG_TOOLS IMGUI_DISABLE_DEFAULT_FONT
    IMGUI_DISABLE_DEFAULT_SHELL_FUNCTIONS IMGUI_DISABLE_OBSOLETE_FUNCTIONS)
target_link_libraries(gol3d_imgui PUBLIC Vulkan::Headers glfw)

# ------------------------------------------------------------------------ game

# Warnings for the project's own code. Vulkan structs are initialized as
# `VkFoo info{VK_STRUCTURE_TYPE_FOO}`, leaving the other fields zero, hence the
# missing-field-initializers exception.
if(MSVC)
    set(GOL3D_WARNINGS /W3)
else()
    set(GOL3D_WARNINGS -Wall -Wextra -Wno-missing-field-initializers)
endif()

# Everything that needs neither a GPU nor a window: rules, player physics,
# stamps, saves, settings, the command line, the tick governor's arithmetic and
# the updater. The game and all CPU tests link it.
add_library(gol3d_core STATIC
    ${GOL_ROOT}/src/game/CommandLine.cpp
    ${GOL_ROOT}/src/game/Player.cpp
    ${GOL_ROOT}/src/game/Settings.cpp
    ${GOL_ROOT}/src/game/Stamps.cpp
    ${GOL_ROOT}/src/game/TickGovernor.cpp
    ${GOL_ROOT}/src/platform/Paths.cpp
    ${GOL_ROOT}/src/tutorial/TutorialLessons.cpp
    ${GOL_ROOT}/src/update/Sha256.cpp
    ${GOL_ROOT}/src/update/Updater.cpp
    ${GOL_ROOT}/src/util/Png.cpp
    ${GOL_ROOT}/src/world/SaveFile.cpp)
target_include_directories(gol3d_core PUBLIC ${GOL_ROOT}/src ${GOL_ROOT}/shaders)
target_link_libraries(gol3d_core PUBLIC glm::glm)
# Vulkan's clip-space depth is 0..1; every file must agree on GLM's convention.
target_compile_definitions(gol3d_core PUBLIC GLM_FORCE_DEPTH_ZERO_TO_ONE)
target_compile_options(gol3d_core PRIVATE ${GOL3D_WARNINGS})
if(WIN32)
    target_link_libraries(gol3d_core PUBLIC shell32)
    target_compile_definitions(gol3d_core PUBLIC _CRT_SECURE_NO_WARNINGS)
endif()

set(GOL3D_SOURCES
    ${GOL_ROOT}/src/main.cpp
    ${GOL_ROOT}/src/game/Game.cpp
    ${GOL_ROOT}/src/game/GameChecks.cpp
    ${GOL_ROOT}/src/game/GameHud.cpp
    ${GOL_ROOT}/src/game/GameInput.cpp
    ${GOL_ROOT}/src/game/GameMenus.cpp
    ${GOL_ROOT}/src/game/GameSimulation.cpp
    ${GOL_ROOT}/src/gpu/GpuBuffer.cpp
    ${GOL_ROOT}/src/gpu/GpuContext.cpp
    ${GOL_ROOT}/src/gpu/ImmediateCommands.cpp
    ${GOL_ROOT}/src/gpu/ShaderModule.cpp
    ${GOL_ROOT}/src/gpu/Swapchain.cpp
    ${GOL_ROOT}/src/gpu/VulkanLoader.cpp
    ${GOL_ROOT}/src/render/Renderer.cpp
    ${GOL_ROOT}/src/tutorial/Tutorial.cpp
    ${GOL_ROOT}/src/ui/Theme.cpp
    ${GOL_ROOT}/src/ui/Widgets.cpp
    ${GOL_ROOT}/src/world/ChunkWorld.cpp
    ${GOL_ROOT}/src/world/SimulationPasses.cpp)
if(WIN32)
    list(APPEND GOL3D_SOURCES ${GOL_ROOT}/packaging/windows/gol3d.rc)
elseif(APPLE)
    set(GOL3D_ICNS ${GOL_ROOT}/packaging/icons/gol3d.icns)
    set(GOL3D_FONT_LICENSE ${GOL_ROOT}/packaging/licenses/Karla-OFL.txt)
    set_source_files_properties(${GOL3D_ICNS} ${GOL3D_FONT_LICENSE} PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
    list(APPEND GOL3D_SOURCES ${GOL3D_ICNS} ${GOL3D_FONT_LICENSE})
endif()
# Menu font: Karla (SIL Open Font License 1.1) from Dear ImGui's misc/fonts,
# embedded in the binary so every package finds it.
set(GOL3D_MENU_FONT ${imgui_SOURCE_DIR}/misc/fonts/Karla-Regular.ttf)
file(READ ${GOL3D_MENU_FONT} GOL3D_MENU_FONT_HEX HEX)
string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," GOL3D_MENU_FONT_BYTES "${GOL3D_MENU_FONT_HEX}")
file(CONFIGURE OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/generated/MenuFont.cpp CONTENT
"// Generated from Karla-Regular.ttf by CMakeLists.txt. Do not edit.
#include \"ui/MenuFont.h\"
const unsigned char MENU_FONT_TTF[] = {@GOL3D_MENU_FONT_BYTES@};
const int MENU_FONT_TTF_SIZE = static_cast<int>(sizeof(MENU_FONT_TTF));
" @ONLY)
list(APPEND GOL3D_SOURCES ${CMAKE_CURRENT_BINARY_DIR}/generated/MenuFont.cpp)

add_executable(gol3d ${GOL3D_SOURCES})
target_include_directories(gol3d PRIVATE ${imgui_SOURCE_DIR} ${imgui_SOURCE_DIR}/backends)
target_link_libraries(gol3d PRIVATE gol3d_core glfw glm::glm gol3d_imgui gol3d_volk)
target_compile_definitions(gol3d PRIVATE GOL3D_VERSION_STRING="${PROJECT_VERSION}")
target_compile_options(gol3d PRIVATE ${GOL3D_WARNINGS})
# Every translation unit sees volk's function pointers instead of Vulkan
# prototypes, including Dear ImGui's Vulkan backend header.
if(MSVC)
    target_compile_options(gol3d PRIVATE $<$<COMPILE_LANGUAGE:CXX>:/FIvolk.h /utf-8>)
    # A GUI app without a console window that still starts at main().
    set_target_properties(gol3d PROPERTIES WIN32_EXECUTABLE TRUE)
    target_link_options(gol3d PRIVATE /ENTRY:mainCRTStartup)
else()
    target_compile_options(gol3d PRIVATE "$<$<COMPILE_LANGUAGE:CXX>:SHELL:-include volk.h>")
endif()
if(APPLE)
    set_target_properties(gol3d PROPERTIES
        MACOSX_BUNDLE TRUE
        OUTPUT_NAME "3D Life"
        MACOSX_BUNDLE_BUNDLE_NAME "3D Life"
        MACOSX_BUNDLE_GUI_IDENTIFIER "io.github.cameronweller.gol3d"
        MACOSX_BUNDLE_ICON_FILE gol3d.icns
        MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
        MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION})
endif()

# Shaders: compiled with glslc, or copied from GOL3D_SHADER_DIR (SPIR-V is portable).
set(GOL_SHADERS life3d_step.comp life3d_build.comp life3d_blocks.vert life3d_boxes.vert life3d_world.frag
    life3d_screen.vert life3d_screen.frag)
set(GOL_SHADER_INCLUDES life3d_frame.glsl life3d_storage.glsl life3d_bits.glsl life3d_instances.glsl)
list(TRANSFORM GOL_SHADER_INCLUDES PREPEND ${GOL_ROOT}/shaders/)
set(GOL_SPV_DIR ${CMAKE_CURRENT_BINARY_DIR}/shaders)
set(GOL_SPV)
if(GOL3D_SHADER_DIR)
    foreach(shader ${GOL_SHADERS})
        set(spv ${GOL_SPV_DIR}/${shader}.spv)
        add_custom_command(OUTPUT ${spv}
            COMMAND ${CMAKE_COMMAND} -E make_directory ${GOL_SPV_DIR}
            COMMAND ${CMAKE_COMMAND} -E copy ${GOL3D_SHADER_DIR}/${shader}.spv ${spv}
            DEPENDS ${GOL3D_SHADER_DIR}/${shader}.spv
            COMMENT "Using prebuilt ${shader}.spv")
        list(APPEND GOL_SPV ${spv})
    endforeach()
else()
    # glslc (shaderc, Vulkan SDK) or glslangValidator (glslang) both work.
    find_program(GLSLC_EXECUTABLE glslc HINTS ENV VULKAN_SDK PATH_SUFFIXES bin)
    if(GLSLC_EXECUTABLE)
        set(GOL_GLSL_COMMAND ${GLSLC_EXECUTABLE} --target-env=vulkan1.3 -O -o)
    else()
        find_program(GLSLANG_EXECUTABLE glslangValidator HINTS ENV VULKAN_SDK PATH_SUFFIXES bin)
        if(NOT GLSLANG_EXECUTABLE)
            message(FATAL_ERROR "Need glslc or glslangValidator to compile shaders (or set GOL3D_SHADER_DIR).")
        endif()
        set(GOL_GLSL_COMMAND ${GLSLANG_EXECUTABLE} -V --target-env vulkan1.3 -o)
    endif()
    foreach(shader ${GOL_SHADERS})
        set(spv ${GOL_SPV_DIR}/${shader}.spv)
        add_custom_command(OUTPUT ${spv}
            COMMAND ${CMAKE_COMMAND} -E make_directory ${GOL_SPV_DIR}
            COMMAND ${GOL_GLSL_COMMAND} ${spv} ${GOL_ROOT}/shaders/${shader}
            DEPENDS ${GOL_ROOT}/shaders/${shader} ${GOL_SHADER_INCLUDES}
            COMMENT "Compiling ${shader}")
        list(APPEND GOL_SPV ${spv})
    endforeach()
endif()
add_custom_target(gol3d_shaders DEPENDS ${GOL_SPV})
add_dependencies(gol3d gol3d_shaders)

# Put shaders (and MoltenVK on macOS) where the executable looks for them, so the
# build tree runs directly.
if(APPLE)
    add_custom_command(TARGET gol3d POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${GOL_SPV_DIR} $<TARGET_BUNDLE_CONTENT_DIR:gol3d>/Resources/shaders)
    if(GOL3D_MOLTENVK_DYLIB)
        add_custom_command(TARGET gol3d POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_BUNDLE_CONTENT_DIR:gol3d>/Frameworks
            COMMAND ${CMAKE_COMMAND} -E copy ${GOL3D_MOLTENVK_DYLIB} $<TARGET_BUNDLE_CONTENT_DIR:gol3d>/Frameworks/)
    endif()
elseif(CMAKE_CONFIGURATION_TYPES)
    add_custom_command(TARGET gol3d POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${GOL_SPV_DIR} $<TARGET_FILE_DIR:gol3d>/shaders)
endif()

# --------------------------------------------------------------------- install

include(GNUInstallDirs)
if(APPLE)
    install(TARGETS gol3d BUNDLE DESTINATION .)
    # Ad-hoc signature over the finished bundle (it is not notarized).
    install(CODE [[execute_process(COMMAND codesign --force --deep --sign - "${CMAKE_INSTALL_PREFIX}/3D Life.app")]])
elseif(WIN32)
    install(TARGETS gol3d RUNTIME DESTINATION .)
    install(FILES ${GOL_SPV} DESTINATION shaders)
    install(FILES ${GOL_ROOT}/docs/PROTOTYPE.md DESTINATION . RENAME README.md)
    install(FILES ${GOL_ROOT}/LICENSE DESTINATION . RENAME LICENSE.txt)
    install(FILES ${GOL_ROOT}/packaging/licenses/Karla-OFL.txt DESTINATION .)
else()
    install(TARGETS gol3d RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
    install(FILES ${GOL_SPV} DESTINATION ${CMAKE_INSTALL_DATADIR}/gol3d/shaders)
    install(FILES ${GOL_ROOT}/packaging/linux/gol3d.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
    install(FILES ${GOL_ROOT}/packaging/icons/gol3d.png DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/256x256/apps)
    install(FILES ${GOL_ROOT}/docs/PROTOTYPE.md DESTINATION ${CMAKE_INSTALL_DOCDIR} RENAME README.md)
    install(FILES ${GOL_ROOT}/LICENSE DESTINATION ${CMAKE_INSTALL_DOCDIR})
    install(FILES ${GOL_ROOT}/packaging/licenses/Karla-OFL.txt DESTINATION ${CMAKE_INSTALL_DOCDIR})
endif()

# ----------------------------------------------------------------------- CPack

set(CPACK_PACKAGE_NAME gol3d)
set(CPACK_PACKAGE_VENDOR "Cameron Weller")
set(CPACK_PACKAGE_CONTACT "Cameron Weller")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "${PROJECT_DESCRIPTION}")
set(CPACK_PACKAGE_FILE_NAME "gol3d-${PROJECT_VERSION}-${GOL3D_PACKAGE_PLATFORM}")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "3D Life")
set(CPACK_PACKAGE_EXECUTABLES gol3d "3D Life")
set(CPACK_RESOURCE_FILE_LICENSE ${GOL_ROOT}/LICENSE)
set(CPACK_STRIP_FILES ON)

set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
set(CPACK_DEBIAN_PACKAGE_DEPENDS "libvulkan1")
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
set(CPACK_DEBIAN_PACKAGE_SECTION games)

set(CPACK_RPM_FILE_NAME RPM-DEFAULT)
set(CPACK_RPM_PACKAGE_LICENSE MIT)
set(CPACK_RPM_PACKAGE_GROUP "Amusements/Games")
set(CPACK_RPM_PACKAGE_REQUIRES "vulkan-loader")
set(CPACK_RPM_EXCLUDE_FROM_AUTO_FILELIST_ADDITION
    /usr/share/applications /usr/share/icons /usr/share/icons/hicolor
    /usr/share/icons/hicolor/256x256 /usr/share/icons/hicolor/256x256/apps /usr/share/doc)

set(CPACK_NSIS_PACKAGE_NAME "3D Life")
set(CPACK_NSIS_DISPLAY_NAME "3D Life")
set(CPACK_NSIS_MUI_ICON ${GOL_ROOT}/packaging/icons/gol3d.ico)
set(CPACK_NSIS_MUI_UNIICON ${GOL_ROOT}/packaging/icons/gol3d.ico)
set(CPACK_NSIS_INSTALLED_ICON_NAME gol3d.exe)
set(CPACK_NSIS_INSTALL_ROOT "$PROGRAMFILES64")
# Installing over an existing copy just replaces files, so the in-game updater can
# run a new installer silently (/S).
set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL OFF)
set(CPACK_NSIS_URL_INFO_ABOUT "${PROJECT_HOMEPAGE_URL}")

set(CPACK_DMG_VOLUME_NAME "3D Life")
set(CPACK_DMG_SLA_USE_RESOURCE_FILE_LICENSE OFF) # the SLA tool was removed in macOS 14

include(CPack)

# ----------------------------------------------------------------------- tests

# CPU tests: no GPU or display needed (`ctest -LE gpu`). Each is a plain
# executable that prints what failed and exits non-zero.
enable_testing()
function(gol3d_add_test name)
    add_executable(${name}_test ${GOL_ROOT}/tests/${ARGN})
    target_link_libraries(${name}_test PRIVATE gol3d_core)
    target_compile_options(${name}_test PRIVATE ${GOL3D_WARNINGS})
    add_test(NAME ${name} COMMAND ${name}_test)
endfunction()
gol3d_add_test(life_rules LifeRulesTest.cpp)  # the rule list and the CPU reference
gol3d_add_test(bit_life BitLifeTest.cpp)      # the GPU's bit-sliced arithmetic, run on the CPU
gol3d_add_test(patterns PatternsTest.cpp)     # still lifes, oscillators, gliders and tutorial scenes
gol3d_add_test(player PlayerTest.cpp)         # walking, flying, collision and aiming
gol3d_add_test(stamps StampsTest.cpp)         # stamp shapes, rotation and tilt
gol3d_add_test(save_file SaveFileTest.cpp)    # the .life3d format
gol3d_add_test(settings SettingsTest.cpp)     # options.txt and the command line
gol3d_add_test(tick_governor TickGovernorTest.cpp)
gol3d_add_test(updater UpdaterTest.cpp)       # release JSON, versions, SHA-256, PNG checksums

# The soup search that found the gliders in src/life/Patterns.h; built only on request.
add_executable(find_patterns EXCLUDE_FROM_ALL ${GOL_ROOT}/tools/FindPatterns.cpp)
find_package(Threads REQUIRED)
target_link_libraries(find_patterns PRIVATE gol3d_core Threads::Threads)

# The GPU engine against the CPU reference. Needs a Vulkan GPU and a display;
# run with `ctest -L gpu`.
add_test(NAME gpu_matches_cpu COMMAND gol3d --verify --seed 7)
set_tests_properties(gpu_matches_cpu PROPERTIES LABELS gpu TIMEOUT 300)

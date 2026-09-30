# Playable prototype: gol3d, built from the minimal Vulkan app with GLFW, GLM,
# Dear ImGui and volk. Vulkan is loaded at runtime through volk, so building needs
# only the Vulkan headers (fetched) and a GLSL compiler; one binary runs on any
# machine with a Vulkan driver. Dependencies come from pinned FetchContent
# downloads unless system GLFW/GLM are found and GOL3D_BUNDLE_DEPS is OFF.
#
#   cmake -S prototype -B build/prototype -G Ninja -DCMAKE_BUILD_TYPE=Release
#   cmake --build build/prototype
#   ./build/prototype/gol3d
#
# Packages: cpack --config build/prototype/CPackConfig.cmake -G "DEB;RPM;TGZ"
# (NSIS/ZIP on Windows, DragNDrop on macOS). See docs/PROTOTYPE.md.
cmake_minimum_required(VERSION 3.20)
if(POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW) # extracted files get the extraction time
endif()

set(GOL_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/..)
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
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release)
endif()
if(APPLE AND NOT CMAKE_OSX_DEPLOYMENT_TARGET)
    set(CMAKE_OSX_DEPLOYMENT_TARGET 11.0)
endif()

include(FetchContent)

# ---------------------------------------------------------------- dependencies

FetchContent_Declare(vulkan_headers
    URL https://github.com/KhronosGroup/Vulkan-Headers/archive/refs/tags/v1.4.304.tar.gz
    URL_HASH SHA256=5e1a06b3f27e7581b55d1dea176fd97ee0a2f299406db2f437c1d2f297ceba5b)
FetchContent_Declare(volk
    URL https://github.com/zeux/volk/archive/refs/tags/1.4.304.tar.gz
    URL_HASH SHA256=ab3d4a8ccaeb32652259cdd008399504a41792675b0421d90b67729ee274746f
    SOURCE_SUBDIR no-cmake) # only volk.c/volk.h are used, not volk's own CMake
FetchContent_Declare(imgui
    URL https://github.com/ocornut/imgui/archive/refs/tags/v1.91.9.tar.gz
    URL_HASH SHA256=3872a5f90df78fced023c1945f4466b654fd74573370b77b17742149763a7a7c)
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
        URL https://github.com/glfw/glfw/releases/download/3.4/glfw-3.4.zip
        URL_HASH SHA256=b5ec004b2712fd08e8861dc271428f048775200a2df719ccf575143ba749a3e9)
    FetchContent_MakeAvailable(glfw)
endif()
if(NOT glm_FOUND)
    FetchContent_Declare(glm
        URL https://github.com/g-truc/glm/archive/refs/tags/1.0.1.tar.gz
        URL_HASH SHA256=9f3174561fd26904b23f0db5e560971cbf9b3cbda0b280f04d5c379d03bf234c)
    FetchContent_MakeAvailable(glm)
endif()

# volk: Vulkan entry points as runtime-loaded function pointers.
add_library(gol3d_volk STATIC ${volk_SOURCE_DIR}/volk.c)
target_include_directories(gol3d_volk PUBLIC ${volk_SOURCE_DIR})
target_compile_definitions(gol3d_volk PUBLIC VK_NO_PROTOTYPES)
target_link_libraries(gol3d_volk PUBLIC Vulkan::Headers ${CMAKE_DL_LIBS})

# Dear ImGui (menus); its Vulkan backend loads functions through volk's loader.
# The repository's include/ holds older backend copies, so gol3d lists these first.
add_library(gol3d_imgui STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp)
target_include_directories(gol3d_imgui PUBLIC ${imgui_SOURCE_DIR} ${imgui_SOURCE_DIR}/backends)
target_compile_definitions(gol3d_imgui PUBLIC IMGUI_IMPL_VULKAN_NO_PROTOTYPES)
target_link_libraries(gol3d_imgui PUBLIC Vulkan::Headers glfw)

# ------------------------------------------------------------------------ game

set(GOL3D_SOURCES
    ${GOL_ROOT}/src/main_minimal.cpp
    ${GOL_ROOT}/src/VulkanContext.cpp
    ${GOL_ROOT}/src/DeviceManager.cpp
    ${GOL_ROOT}/src/WindowManager.cpp
    ${GOL_ROOT}/src/Logger.cpp
    ${GOL_ROOT}/src/engine/vulkan/resources/ShaderManager.cpp)
if(WIN32)
    list(APPEND GOL3D_SOURCES ${GOL_ROOT}/packaging/windows/gol3d.rc)
elseif(APPLE)
    set(GOL3D_ICNS ${GOL_ROOT}/packaging/icons/gol3d.icns)
    set_source_files_properties(${GOL3D_ICNS} PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
    list(APPEND GOL3D_SOURCES ${GOL3D_ICNS})
endif()
add_executable(gol3d ${GOL3D_SOURCES})
target_include_directories(gol3d PRIVATE ${imgui_SOURCE_DIR} ${imgui_SOURCE_DIR}/backends ${GOL_ROOT}/include)
target_link_libraries(gol3d PRIVATE glfw glm::glm gol3d_imgui gol3d_volk)
target_compile_definitions(gol3d PRIVATE GLM_FORCE_DEPTH_ZERO_TO_ONE)
# Every translation unit sees volk's function pointers instead of Vulkan prototypes;
# the engine sources keep including <vulkan/vulkan.h> unchanged.
if(MSVC)
    target_compile_options(gol3d PRIVATE $<$<COMPILE_LANGUAGE:CXX>:/FIvolk.h /utf-8>)
    target_compile_definitions(gol3d PRIVATE _CRT_SECURE_NO_WARNINGS)
    # A GUI app without a console window that still starts at main().
    set_target_properties(gol3d PROPERTIES WIN32_EXECUTABLE TRUE)
    target_link_options(gol3d PRIVATE /ENTRY:mainCRTStartup)
else()
    target_compile_options(gol3d PRIVATE "$<$<COMPILE_LANGUAGE:CXX>:SHELL:-include volk.h>" -Wall -Wextra)
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
set(GOL_SHADERS life3d_chunks.comp life3d_world.vert life3d_world.frag life3d_screen.vert life3d_screen.frag)
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
    find_program(GLSLC_EXECUTABLE glslc HINTS ENV VULKAN_SDK PATH_SUFFIXES bin REQUIRED)
    foreach(shader ${GOL_SHADERS})
        set(spv ${GOL_SPV_DIR}/${shader}.spv)
        add_custom_command(OUTPUT ${spv}
            COMMAND ${CMAKE_COMMAND} -E make_directory ${GOL_SPV_DIR}
            COMMAND ${GLSLC_EXECUTABLE} -o ${spv} ${GOL_ROOT}/shaders/${shader}
            DEPENDS ${GOL_ROOT}/shaders/${shader} ${GOL_ROOT}/shaders/life3d_frame.glsl
            COMMENT "glslc ${shader}")
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
else()
    install(TARGETS gol3d RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
    install(FILES ${GOL_SPV} DESTINATION ${CMAKE_INSTALL_DATADIR}/gol3d/shaders)
    install(FILES ${GOL_ROOT}/packaging/linux/gol3d.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
    install(FILES ${GOL_ROOT}/packaging/icons/gol3d.png DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/256x256/apps)
    install(FILES ${GOL_ROOT}/docs/PROTOTYPE.md DESTINATION ${CMAKE_INSTALL_DOCDIR} RENAME README.md)
    install(FILES ${GOL_ROOT}/LICENSE DESTINATION ${CMAKE_INSTALL_DOCDIR})
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

enable_testing()
add_executable(life3d_rules_test life3d_rules_test.cpp)
target_include_directories(life3d_rules_test PRIVATE ${GOL_ROOT}/include)
add_test(NAME life3d_rules COMMAND life3d_rules_test)
# Needs a Vulkan GPU and a display; run with `ctest -L gpu`.
add_test(NAME life3d_gpu_matches_cpu COMMAND gol3d --verify --seed 7)
set_tests_properties(life3d_gpu_matches_cpu PROPERTIES LABELS gpu TIMEOUT 120)

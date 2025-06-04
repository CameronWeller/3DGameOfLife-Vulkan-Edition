cmake_minimum_required(VERSION 3.20)

set(CMAKE_PREFIX_PATH "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg_installed/x64-windows")
message(STATUS "CMAKE_PREFIX_PATH: ${CMAKE_PREFIX_PATH}")

find_package(glfw3 CONFIG REQUIRED)
message(STATUS "Found glfw3: ${glfw3_FOUND}")
message(STATUS "glfw3 targets: ${glfw3_TARGETS}") 
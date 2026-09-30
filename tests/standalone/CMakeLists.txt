cmake_minimum_required(VERSION 3.20)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(SparseGrid3DTests LANGUAGES CXX)
    enable_testing()
endif()

add_executable(sparse_grid_standalone_test
    ../SparseGrid3DStandaloneTest.cpp
    ../../src/SparseGrid3D.cpp
)
target_compile_features(sparse_grid_standalone_test PRIVATE cxx_std_17)
target_include_directories(sparse_grid_standalone_test PRIVATE ../../include)
add_test(NAME SparseGrid3DStandalone COMMAND sparse_grid_standalone_test)
set_tests_properties(SparseGrid3DStandalone PROPERTIES LABELS "unit;sparse")

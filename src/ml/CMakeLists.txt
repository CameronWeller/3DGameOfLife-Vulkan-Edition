set(ML_SOURCES
    training/TrainingLoop.cpp
    models/ModelFactory.cpp
    data/Dataset.cpp
    utils/MLUtils.cpp
)

set(ML_HEADERS
    ${CMAKE_SOURCE_DIR}/include/ml/MLComponent.h
    ${CMAKE_SOURCE_DIR}/include/ml/training/TrainingConfig.h
    ${CMAKE_SOURCE_DIR}/include/ml/training/TrainingMetrics.h
    ${CMAKE_SOURCE_DIR}/include/ml/models/Model.h
    ${CMAKE_SOURCE_DIR}/include/ml/data/Dataset.h
    ${CMAKE_SOURCE_DIR}/include/ml/utils/MLUtils.h
)

add_library(ml STATIC ${ML_SOURCES} ${ML_HEADERS})

target_include_directories(ml
    PUBLIC
        ${CMAKE_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(ml
    PUBLIC
        glm
        fmt::fmt
    PRIVATE
        spdlog::spdlog
)

if(USE_CUDA)
    target_compile_definitions(ml PRIVATE USE_CUDA)
    target_link_libraries(ml PRIVATE CUDA::cudart CUDA::cublas)
endif()

if(USE_HIP)
    target_compile_definitions(ml PRIVATE USE_HIP)
    target_link_libraries(ml PRIVATE hip::host)
endif()

# Add ML component to the main project
target_link_libraries(${PROJECT_NAME} PRIVATE ml) 
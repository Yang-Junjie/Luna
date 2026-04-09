add_library(
    LunaSampleCommon
    STATIC
    SampleCameraLayer.h
    SampleCommon.h
    SampleCommon.cpp
)

target_include_directories(
    LunaSampleCommon
    PUBLIC
        ${PROJECT_SOURCE_DIR}
)

target_link_libraries(
    LunaSampleCommon
    PUBLIC
        LunaCore
)

target_compile_definitions(
    LunaSampleCommon
    PUBLIC
        LUNA_PROJECT_ROOT="${PROJECT_SOURCE_DIR}"
)

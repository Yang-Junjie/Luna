add_executable(
    TaskSystemSmoke
    TaskSystemSmoke.cpp
)

target_include_directories(
    TaskSystemSmoke
    PUBLIC
        ${PROJECT_SOURCE_DIR}
)

target_link_libraries(
    TaskSystemSmoke
    PRIVATE
        LunaCore
)

add_executable(
    TaskSystemResourcePipeline
    TaskSystemResourcePipeline.cpp
)

target_include_directories(
    TaskSystemResourcePipeline
    PUBLIC
        ${PROJECT_SOURCE_DIR}
)

target_link_libraries(
    TaskSystemResourcePipeline
    PRIVATE
        LunaCore
)

target_compile_definitions(
    TaskSystemResourcePipeline
    PUBLIC
        LUNA_PROJECT_ROOT="${PROJECT_SOURCE_DIR}"
)

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

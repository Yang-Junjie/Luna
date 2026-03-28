add_library(
    LunaEditor
    STATIC
    editor_app.h
    editor_app.cpp
)

target_include_directories(
    LunaEditor
    PUBLIC
        ${PROJECT_SOURCE_DIR}
)

target_link_libraries(
    LunaEditor
    PUBLIC
        LunaCore
)

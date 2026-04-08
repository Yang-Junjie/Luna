add_library(
    LunaEditor
    STATIC
    EditorApp.h
    EditorApp.cpp
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


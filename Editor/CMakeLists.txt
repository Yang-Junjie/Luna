add_library(
    LunaEditorFramework
    STATIC
    EditorPanel.h
    EditorRegistry.h
    EditorRegistry.cpp
    EditorShellLayer.h
    EditorShellLayer.cpp
)

target_include_directories(
    LunaEditorFramework
    PUBLIC
        ${PROJECT_SOURCE_DIR}
)

target_link_libraries(
    LunaEditorFramework
    PUBLIC
        LunaCore
)

target_compile_definitions(
    LunaEditorFramework
    PUBLIC
        LUNA_PROJECT_ROOT="${PROJECT_SOURCE_DIR}"
)

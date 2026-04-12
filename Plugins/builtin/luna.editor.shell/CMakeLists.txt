add_library(
    LunaEditorShellPlugin
    STATIC
    luna.plugin.toml
    src/EditorShellPlugin.cpp
)

target_include_directories(
    LunaEditorShellPlugin
    PUBLIC
        ${PROJECT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(
    LunaEditorShellPlugin
    PUBLIC
        LunaEditorFramework
)

add_library(
    LunaBuiltinEditorCorePlugin
    STATIC
    luna.plugin.toml
    src/BuiltinEditorCorePlugin.cpp
    src/EditorCameraControllerLayer.h
    src/RendererInfoPanel.h
    src/RendererInfoPanel.cpp
)

target_include_directories(
    LunaBuiltinEditorCorePlugin
    PUBLIC
        ${PROJECT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(
    LunaBuiltinEditorCorePlugin
    PUBLIC
        LunaEditorFramework
)

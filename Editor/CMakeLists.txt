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

set(LUNA_PLUGIN_TARGETS "")
set(LUNA_EDITOR_GENERATED_DIR ${PROJECT_SOURCE_DIR}/Plugins/Generated/editor)
include(${LUNA_EDITOR_GENERATED_DIR}/PluginList.cmake OPTIONAL)

add_library(
    LunaEditor
    STATIC
    EditorApp.h
    EditorApp.cpp
    ${LUNA_EDITOR_GENERATED_DIR}/ResolvedPlugins.h
    ${LUNA_EDITOR_GENERATED_DIR}/ResolvedPlugins.cpp
)

target_include_directories(
    LunaEditor
    PUBLIC
        ${PROJECT_SOURCE_DIR}
)

target_link_libraries(
    LunaEditor
    PUBLIC
        LunaEditorFramework
)

if(LUNA_PLUGIN_TARGETS)
    target_link_libraries(LunaEditor PUBLIC ${LUNA_PLUGIN_TARGETS})
endif()

add_executable(LunaEditorApp ${PROJECT_SOURCE_DIR}/Luna/Core/Main.cpp)
target_link_libraries(LunaEditorApp PRIVATE LunaEditor)

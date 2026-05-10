add_library(
    LunaEditorProtocol
    INTERFACE
)

target_include_directories(
    LunaEditorProtocol
    INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(
    LunaEditorProtocol
    INTERFACE
        LunaCore
        nlohmann_json::nlohmann_json
)

add_executable(
    LunaEditor
    ${PROJECT_SOURCE_DIR}/Luna/Core/main.cpp
    Source/Panels/AssetLoadingPanel.h
    Source/Panels/AssetLoadingPanel.cpp
    Source/Panels/BackendCapabilitiesPanel.h
    Source/Panels/BackendCapabilitiesPanel.cpp
    Source/Panels/BuiltinMaterialsPanel.h
    Source/Panels/BuiltinMaterialsPanel.cpp
    Source/Panels/ContentBrowserPanel.h
    Source/Panels/ContentBrowserPanel.cpp
    Source/EditorCamera.h
    Source/EditorCamera.cpp
    Source/EditorContext.h
    Source/EditorAssetDragDrop.h
    Source/EditorRuntime.h
    Source/EditorRuntime.cpp
    Source/EditorStyle.h
    Source/EditorStyle.cpp
    Source/EditorUI.h
    Source/EditorUI.cpp
    Source/Viewport/EditorViewportSession.h
    Source/Viewport/EditorViewportSession.cpp
    Source/Panels/InspectorPanel.h
    Source/Panels/InspectorPanel.cpp
    Source/LunaEditorApp.h
    Source/LunaEditorApp.cpp
    Source/LunaEditorLayer.h
    Source/LunaEditorLayer.cpp
    Source/Panels/RenderDebugPanel.h
    Source/Panels/RenderDebugPanel.cpp
    Source/Panels/RenderFeaturesPanel.h
    Source/Panels/RenderFeaturesPanel.cpp
    Source/Panels/RenderProfilerPanel.h
    Source/Panels/RenderProfilerPanel.cpp
    Source/ScriptComponentInspector.h
    Source/ScriptComponentInspector.cpp
    Source/Panels/SceneSettingPanel.h
    Source/Panels/SceneSettingPanel.cpp
    Source/Panels/ScriptPluginsPanel.h
    Source/Panels/ScriptPluginsPanel.cpp
    Source/Panels/SceneHierarchyPanel.h
    Source/Panels/SceneHierarchyPanel.cpp
    ${PROJECT_SOURCE_DIR}/third_party/ImGuizmo/ImGuizmo.cpp
)

target_include_directories(
    LunaEditor
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/Source
        ${PROJECT_SOURCE_DIR}/third_party/ImGuizmo
)

target_link_libraries(
    LunaEditor
    PRIVATE
        LunaEditorProtocol
        LunaCore
)

set_target_properties(
    LunaEditor
    PROPERTIES
        VS_DEBUGGER_WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
)

add_executable(
    LunaEditorViewHost
    Source/Viewport/EditorViewportHost.cpp
)

target_include_directories(
    LunaEditorViewHost
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(
    LunaEditorViewHost
    PRIVATE
        LunaCore
        nlohmann_json::nlohmann_json
)

set_target_properties(
    LunaEditorViewHost
    PROPERTIES
        VS_DEBUGGER_WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
)

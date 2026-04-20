add_executable(
    LunaEditor
    ${PROJECT_SOURCE_DIR}/Luna/Core/main.cpp
    AssetLoadingPanel.h
    AssetLoadingPanel.cpp
    ContentBrowserPanel.h
    ContentBrowserPanel.cpp
    EditorAssetDragDrop.h
    InspectorPanel.h
    InspectorPanel.cpp
    LunaEditorApp.h
    LunaEditorApp.cpp
    LunaEditorLayer.h
    LunaEditorLayer.cpp
    SceneHierarchyPanel.h
    SceneHierarchyPanel.cpp
)

target_include_directories(
    LunaEditor
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(
    LunaEditor
    PRIVATE
        LunaCore
)

set_target_properties(
    LunaEditor
    PROPERTIES
        VS_DEBUGGER_WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
)

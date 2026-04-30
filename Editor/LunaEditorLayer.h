#pragma once

#include "AssetLoadingPanel.h"
#include "BackendCapabilitiesPanel.h"
#include "BuiltinMaterialsPanel.h"
#include "ContentBrowserPanel.h"
#include "Core/Layer.h"
#include "EditorCamera.h"
#include "Events/Event.h"
#include "InspectorPanel.h"
#include "RenderDebugPanel.h"
#include "RenderFeaturesPanel.h"
#include "RenderProfilerPanel.h"
#include "Scene/Entity.h"
#include "Scene/Scene.h"
#include "SceneHierarchyPanel.h"
#include "SceneSettingPanel.h"

#include <cstdint>

#include <filesystem>
#include <string>

struct ImVec2;

namespace luna::RHI {
struct Extent2D;
}

namespace luna {

class LunaEditorApplication;

enum class GizmoOperation : uint8_t {
    Translate,
    Rotate,
    Scale,
};

enum class GizmoMode : uint8_t {
    Local,
    World,
};

class LunaEditorLayer final : public Layer {
public:
    explicit LunaEditorLayer(LunaEditorApplication& application);

    void onAttach() override;
    void onDetach() override;
    void onUpdate(Timestep dt) override;
    void onEvent(Event& event) override;
    void onImGuiRender() override;

    const std::string& getAssetLabel() const;
    Scene& getScene();
    Entity getSelectedEntity() const;
    void setSelectedEntity(Entity entity);
    void markSceneDirty();
    bool openSceneFile(const std::filesystem::path& scene_file_path);
    Entity createEntityFromModelAsset(AssetHandle model_handle, Entity parent = {});
    Entity createEntityFromMeshAsset(AssetHandle mesh_handle, Entity parent = {});
    Entity createPrimitiveEntity(AssetHandle mesh_handle, Entity parent = {});
    Entity createCameraEntity(Entity parent = {});
    Entity createDirectionalLightEntity(Entity parent = {});
    Entity createPointLightEntity(Entity parent = {});
    Entity createSpotLightEntity(Entity parent = {});
    void applyMeshAssetToEntity(Entity entity, AssetHandle mesh_handle);
    void openBuiltinMaterialsPanel(AssetHandle material_handle = AssetHandle(0));

private:
    void consumePendingScenePick();
    void syncPickDebugVisualizationState() const;
    void requestViewportPick(const ImVec2& image_min,
                             const ImVec2& image_max,
                             const ImVec2& uv0,
                             const ImVec2& uv1,
                             const luna::RHI::Extent2D& texture_extent) const;
    void onImGuiMenuBar();
    void drawViewport();
    void updateGizmoShortcuts();
    bool drawViewportGizmo(const ImVec2& viewport_min, const ImVec2& viewport_size);
    void resetEditorState();

    bool syncProjectAssets();
    bool openProject(const std::filesystem::path& project_file_path);

    void createScene();
    bool openScene();
    bool openScene(const std::filesystem::path& scene_file_path, bool update_project_start_scene);
    bool saveScene();
    bool saveSceneAs();
    bool saveSceneAs(const std::filesystem::path& scene_file_path);

    std::filesystem::path sceneDialogDefaultPath() const;
    void updateSceneLabel();
    void syncProjectStartScene(const std::filesystem::path& scene_file_path);
    bool hasProjectLoaded() const;

private:
    LunaEditorApplication* m_application{nullptr};
    EditorCamera m_editor_camera;
    Scene m_scene;
    Entity m_selected_entity;
    std::filesystem::path m_scene_file_path;
    std::string m_asset_label{"No scene loaded"};
    bool m_scene_dirty{false};
    bool m_show_pick_debug_visualization{false};
    bool m_viewport_focused{false};
    bool m_viewport_hovered{false};
    bool m_show_builtin_materials_panel{false};

    bool m_show_render_debug_panel{false};
    bool m_show_render_features_panel{false};
    bool m_show_render_profiler_panel{false};
    bool m_show_scene_setting_panel{true};
    bool m_show_backend_capabilities_panel{false};
    bool m_runtime_viewport_enabled{false};
    GizmoOperation m_gizmo_operation{GizmoOperation::Translate};
    GizmoMode m_gizmo_mode{GizmoMode::Local};

    SceneHierarchyPanel m_scene_hierarchy_panel;
    InspectorPanel m_inspector_panel;
    AssetLoadingPanel m_asset_loading_panel;
    BuiltinMaterialsPanel m_builtin_materials_panel;
    ContentBrowserPanel m_content_browser_panel;
    RenderDebugPanel m_render_debug_panel;
    RenderFeaturesPanel m_render_features_panel;
    RenderProfilerPanel m_render_profiler_panel;
    SceneSettingPanel m_scene_setting_panel;
    BackendCapabilitiesPanel m_backend_capabilities_panel;
};

} // namespace luna

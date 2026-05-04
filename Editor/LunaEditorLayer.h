#pragma once

#include "Core/Layer.h"
#include "EditorCamera.h"
#include "EditorContext.h"
#include "Events/Event.h"
#include "Scene/Entity.h"
#include "Scene/Scene.h"
#include "Scene/SceneRuntime.h"
#include "Script/ScriptPluginManifest.h"

#include <cstdint>

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

struct ImVec2;

namespace luna::RHI {
struct Extent2D;
}

namespace luna {

class AssetLoadingPanel;
class BackendCapabilitiesPanel;
class BuiltinMaterialsPanel;
class ContentBrowserPanel;
class InspectorPanel;
class LunaEditorApplication;
class RenderDebugPanel;
class RenderFeaturesPanel;
class RenderProfilerPanel;
class SceneHierarchyPanel;
class SceneSettingPanel;
class ScriptPluginsPanel;

enum class GizmoOperation : uint8_t {
    Translate,
    Rotate,
    Scale,
};

enum class GizmoMode : uint8_t {
    Local,
    World,
};

class LunaEditorLayer final : public Layer, public EditorContext {
public:
    explicit LunaEditorLayer(LunaEditorApplication& application);
    ~LunaEditorLayer() override;

    void onAttach() override;
    void onDetach() override;
    void onUpdate(Timestep dt) override;
    void onEvent(Event& event) override;
    void onImGuiRender() override;

    const std::string& getAssetLabel() const;
    Scene& getScene() override;
    Scene& getInspectionScene() override;
    bool isRuntimeViewportEnabled() const noexcept override;
    UUID getSelectedEntityId() const noexcept;
    Entity getSelectedEntity() override;
    void setSelectedEntity(Entity entity) override;
    void setSelectedEntityId(UUID entity_id);
    void markSceneDirty() override;
    void patchRuntimeScriptProperty(UUID entity_id, size_t script_index, size_t property_index) override;
    bool openSceneFile(const std::filesystem::path& scene_file_path) override;
    Entity createEntityFromModelAsset(AssetHandle model_handle, Entity parent = {}) override;
    Entity createEntityFromMeshAsset(AssetHandle mesh_handle, Entity parent = {}) override;
    Entity createPrimitiveEntity(AssetHandle mesh_handle, Entity parent = {}) override;
    Entity createCameraEntity(Entity parent = {}) override;
    Entity createDirectionalLightEntity(Entity parent = {}) override;
    Entity createPointLightEntity(Entity parent = {}) override;
    Entity createSpotLightEntity(Entity parent = {}) override;
    void applyMeshAssetToEntity(Entity entity, AssetHandle mesh_handle) override;
    void openBuiltinMaterialsPanel(AssetHandle material_handle = AssetHandle(0)) override;
    bool hasProjectLoaded() const override;
    void refreshProjectScriptPlugins() override;
    const std::vector<ScriptPluginCandidate>& getDiscoveredScriptPlugins() const override;
    const std::string& getScriptPluginStatus() const override;
    const ScriptPluginCandidate* getSelectedScriptPluginCandidate() const override;
    bool selectScriptPlugin(const ScriptPluginCandidate* candidate) override;

private:
    void consumePendingScenePick();
    void syncEditorUiScale();
    void syncPickDebugVisualizationState() const;
    void syncEditorGridFeatureState() const;
    void requestViewportPick(const ImVec2& image_min,
                             const ImVec2& image_max,
                             const ImVec2& uv0,
                             const ImVec2& uv1,
                             const luna::RHI::Extent2D& texture_extent) const;
    void drawDockSpace();
    void onImGuiMenuBar();
    void drawViewport();
    void updateGizmoShortcuts();
    bool drawViewportGizmo(const ImVec2& viewport_min, const ImVec2& viewport_size);
    void resetEditorState();
    void setRuntimeViewportEnabled(bool enabled);
    void beginRuntimeViewport();
    void endRuntimeViewport();
    Scene& activeRenderScene();

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
    void refreshScriptPluginCandidates();
    void resolveProjectScriptPluginSelection(bool persist_changes);
    bool setProjectScriptPluginSelection(const ScriptPluginCandidate* candidate, bool log_changes = true);

private:
    LunaEditorApplication* m_application{nullptr};
    EditorCamera m_editor_camera;
    std::unique_ptr<Scene> m_scene;
    std::unique_ptr<Scene> m_runtime_scene;
    std::unique_ptr<SceneRuntime> m_runtime_scene_runtime;
    UUID m_selected_entity_id{0};
    std::filesystem::path m_scene_file_path;
    std::string m_asset_label{"No scene loaded"};
    bool m_scene_dirty{false};
    bool m_show_pick_debug_visualization{false};
    bool m_viewport_focused{false};
    bool m_viewport_hovered{false};
    bool m_show_editor_grid{true};
    bool m_show_builtin_materials_panel{false};
    float m_editor_ui_scale{0.0f};

    bool m_show_render_debug_panel{false};
    bool m_show_render_features_panel{false};
    bool m_show_render_profiler_panel{false};
    bool m_show_scene_setting_panel{true};
    bool m_show_script_plugins_panel{true};
    bool m_show_backend_capabilities_panel{false};
    bool m_runtime_viewport_enabled{false};
    bool m_runtime_viewport_requested{false};
    GizmoOperation m_gizmo_operation{GizmoOperation::Translate};
    GizmoMode m_gizmo_mode{GizmoMode::Local};
    std::vector<ScriptPluginCandidate> m_script_plugin_candidates;
    std::string m_script_plugin_status;

    std::unique_ptr<SceneHierarchyPanel> m_scene_hierarchy_panel;
    std::unique_ptr<InspectorPanel> m_inspector_panel;
    std::unique_ptr<AssetLoadingPanel> m_asset_loading_panel;
    std::unique_ptr<BuiltinMaterialsPanel> m_builtin_materials_panel;
    std::unique_ptr<ContentBrowserPanel> m_content_browser_panel;
    std::unique_ptr<RenderDebugPanel> m_render_debug_panel;
    std::unique_ptr<RenderFeaturesPanel> m_render_features_panel;
    std::unique_ptr<RenderProfilerPanel> m_render_profiler_panel;
    std::unique_ptr<SceneSettingPanel> m_scene_setting_panel;
    std::unique_ptr<ScriptPluginsPanel> m_script_plugins_panel;
    std::unique_ptr<BackendCapabilitiesPanel> m_backend_capabilities_panel;
};

} // namespace luna

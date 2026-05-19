#pragma once

#include "EditorCamera.h"
#include "EditorDocumentContext.h"
#include "EditorDocumentHost.h"
#include "EditorRuntime.h"
#include "EditorApi/EditorRenderingService.h"
#include "EditorApi/EditorSettingsService.h"
#include "EditorApi/EditorViewportService.h"
#include "Core/Layer.h"
#include "Events/Event.h"
#include "Scene/Entity.h"
#include "Scene/Scene.h"
#include "Scene/SceneRuntime.h"
#include "Viewport/ViewportInteraction.h"
#include "Viewport/SceneViewportInstanceManager.h"
#include "Viewport/TextureViewportInstanceManager.h"
#include "Script/ScriptPluginManifest.h"

#include <array>
#include <cstddef>
#include <cstdint>

#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

struct ImVec2;

namespace luna::RHI {
struct Extent2D;
}

namespace luna {

class LunaEditorApplication;
class Renderer;
class SceneViewportInstance;

namespace editor {
class EditorPluginManager;
class EditorShell;
class Ui;
}

enum class GizmoOperation : uint8_t {
    Translate,
    Rotate,
    Scale,
};

enum class GizmoMode : uint8_t {
    Local,
    World,
};

class LunaEditorLayer final : public Layer, public EditorDocumentHost {
public:
    explicit LunaEditorLayer(LunaEditorApplication& application);
    ~LunaEditorLayer() override;

    void onAttach() override;
    void onDetach() override;
    void onUpdate(Timestep dt) override;
    void onEvent(Event& event) override;
    void onImGuiRender() override;

    const std::string& getAssetLabel() const;
    std::string getRenderingBackendName() const;
    editor::RenderingBackendCapabilities getRenderingBackendCapabilities() const;
    float getFrameTimeMilliseconds() const noexcept;
    float getFramesPerSecond() const noexcept;
    uint32_t getSceneOutputWidth() const noexcept;
    uint32_t getSceneOutputHeight() const noexcept;
    size_t getRuntimeEntityCount() const noexcept;
    std::array<float, 3> getEditorCameraPosition() const noexcept;
    std::string getGizmoOperationName() const;
    std::string getGizmoModeName() const;
    bool isPickDebugVisualizationEnabled() const noexcept;
    void setPickDebugVisualizationEnabled(bool enabled);
    bool isEditorGridEnabled() const noexcept;
    void setEditorGridEnabled(bool enabled);
    editor::RenderGraphProfileSnapshot getRenderGraphProfileSnapshot() const;
    bool isRenderGraphProfilingEnabled() const noexcept;
    void setRenderGraphProfilingEnabled(bool enabled);
    std::filesystem::path defaultRenderProfileExportPath(std::string_view backend_name = {}) const;
    bool exportRenderGraphProfileChromeTraceJson(const editor::RenderGraphProfileSnapshot& profile,
                                                 const std::filesystem::path& output_path,
                                                 std::string* error_message = nullptr) const;
    std::vector<editor::RenderFeatureInfo> getDefaultRenderFeatureInfos() const;
    std::vector<editor::RenderFeatureParameterInfo>
        getDefaultRenderFeatureParameters(std::string_view feature_name) const;
    bool setDefaultRenderFeatureEnabled(std::string_view feature_name, bool enabled);
    bool setDefaultRenderFeatureParameter(std::string_view feature_name,
                                          std::string_view parameter_name,
                                          const editor::RenderFeatureParameterValue& value);
    std::vector<editor::RenderDebugViewModeInfo> getRenderDebugViewModes() const;
    editor::RenderDebugViewMode getRenderDebugViewMode() const noexcept;
    void setRenderDebugViewMode(editor::RenderDebugViewMode mode);
    float getRenderDebugVelocityScale() const noexcept;
    void setRenderDebugVelocityScale(float scale);
    editor::TextureView getRenderDebugTextureView() const;
    Scene& getScene() override;
    Scene& getInspectionScene() override;
    bool isRuntimeViewportEnabled() const noexcept override;
    bool isRuntimeViewportRequested() const noexcept;
    void setRuntimeViewportRequested(bool enabled);
    editor::ViewportId defaultSceneViewportId() const noexcept;
    editor::ViewportId createSceneViewport(std::string_view debug_name = {});
    editor::ViewportId createSceneViewport(std::string_view debug_name, std::string_view owner_id);
    void destroySceneViewport(editor::ViewportId viewport_id);
    bool isSceneViewportValid(editor::ViewportId viewport_id) const noexcept;
    editor::ViewportPresentation syncSceneViewport(editor::ViewportId viewport_id, editor::UVec2 framebuffer_size);
    editor::ViewportPresentation syncSceneViewport(editor::ViewportId viewport_id,
                                                   uint32_t framebuffer_width,
                                                   uint32_t framebuffer_height);
    bool setSceneViewportPreview(editor::ViewportId viewport_id, const editor::SceneViewportPreviewState& state);
    void clearSceneViewportPreview(editor::ViewportId viewport_id);
    editor::ViewportPresentation syncSceneViewport(uint32_t framebuffer_width, uint32_t framebuffer_height);
    editor::TextureView getSceneTextureView(editor::ViewportId viewport_id) const;
    editor::TextureView getSceneTextureView() const;
    editor::ViewportId activeSceneViewportId() const noexcept;
    editor::ViewportId createTextureViewport(std::string_view debug_name = {});
    editor::ViewportId createTextureViewport(std::string_view debug_name, std::string_view owner_id);
    void destroyTextureViewport(editor::ViewportId viewport_id);
    bool isTextureViewportValid(editor::ViewportId viewport_id) const noexcept;
    editor::TextureViewportPresentation syncTextureViewport(editor::ViewportId viewport_id,
                                                            editor::TextureView texture,
                                                            editor::UVec2 framebuffer_size);
    editor::TextureViewportPresentation textureViewportPresentation(editor::ViewportId viewport_id) const;
    void destroyViewportsForOwner(std::string_view owner_id);
    void drawDefaultSceneViewport(editor::Ui& ui, std::string_view owner_id = {});
    const ViewportInteractionState& recordViewportSurfaceInteraction(editor::ViewportId viewport_id,
                                                                     std::string_view owner_id,
                                                                     const ViewportInteractionInput& input);
    bool isViewportInputAllowed(editor::ViewportId viewport_id) const noexcept;
    UUID getSelectedEntityId() const noexcept;
    Entity getSelectedEntity() override;
    void setSelectedEntity(Entity entity) override;
    void setSelectedEntityId(UUID entity_id);
    void markSceneDirty() override;
    void patchRuntimeScriptProperty(UUID entity_id, size_t script_index, size_t property_index) override;
    bool openSceneFile(const std::filesystem::path& scene_file_path) override;
    Entity createEntity(const std::string& name = std::string{}, Entity parent = {}) override;
    Entity createEntityFromModelAsset(AssetHandle model_handle, Entity parent = {}) override;
    Entity createEntityFromMeshAsset(AssetHandle mesh_handle, Entity parent = {}) override;
    Entity createPrimitiveEntity(AssetHandle mesh_handle, Entity parent = {}) override;
    Entity createCameraEntity(Entity parent = {}) override;
    Entity createDirectionalLightEntity(Entity parent = {}) override;
    Entity createPointLightEntity(Entity parent = {}) override;
    Entity createSpotLightEntity(Entity parent = {}) override;
    bool destroyEntity(Entity entity) override;
    bool reparentEntity(Entity entity, Entity parent, bool preserve_world_transform = true) override;
    bool addComponent(Entity entity, authoring::AuthoringComponentKind component_kind) override;
    bool removeComponent(Entity entity, authoring::AuthoringComponentKind component_kind) override;
    void applyMeshAssetToEntity(Entity entity, AssetHandle mesh_handle) override;
    bool setEntityName(Entity entity, std::string name) override;
    bool setEntityTransform(Entity entity, const TransformComponent& transform) override;
    bool setCameraComponent(Entity entity, const CameraComponent& camera_component) override;
    bool setLightComponent(Entity entity, const LightComponent& light_component) override;
    bool setMeshComponent(Entity entity, const MeshComponent& mesh_component) override;
    bool setScriptComponent(Entity entity, const ScriptComponent& script_component) override;
    bool setSceneEnvironmentSettings(const SceneEnvironmentSettings& settings) override;
    bool setSceneShadowSettings(const SceneShadowSettings& settings) override;
    void openBuiltinMaterialsPanel(AssetHandle material_handle = AssetHandle(0)) override;
    std::filesystem::path getProjectRootPath() const override;
    const ProjectInfo* getProjectInfo() const override;
    bool hasProjectLoaded() const override;
    void refreshProjectScriptPlugins() override;
    const std::vector<ScriptPluginCandidate>& getDiscoveredScriptPlugins() const override;
    const std::string& getScriptPluginStatus() const override;
    const ScriptPluginCandidate* getSelectedScriptPluginCandidate() const override;
    bool selectScriptPlugin(const ScriptPluginCandidate* candidate) override;
    bool canUndo() const noexcept;
    bool canRedo() const noexcept;
    bool hasOpenEditorTransaction() const noexcept;
    bool beginEditorTransaction(std::string name);
    bool commitEditorTransaction();
    bool rollbackEditorTransaction();
    bool undoEditorCommand();
    bool redoEditorCommand();

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
    void updateEditorShortcuts();
    void updateGizmoShortcuts();
    bool drawViewportGizmo(const ImVec2& viewport_min, const ImVec2& viewport_size, bool allow_interaction);
    void resetEditorState();
    void setRuntimeViewportEnabled(bool enabled);
    void beginRuntimeViewport();
    void endRuntimeViewport();
    Scene& activeRenderScene();
    SceneViewportInstance& activeSceneViewportInstance() noexcept;
    const SceneViewportInstance& activeSceneViewportInstance() const noexcept;
    editor::ViewportId allocateViewportId() noexcept;
    struct PreviewSceneViewport;
    bool syncPreviewSceneViewport(editor::ViewportId viewport_id,
                                  Renderer& renderer,
                                  SceneViewportInstance& viewport);
    void rebuildPreviewScene(PreviewSceneViewport& preview);
    void processAuthoringEvents();

    bool syncProjectAssets();
    bool openProject(const std::filesystem::path& project_file_path);

    void createScene();
    bool openScene();
    bool openScene(const std::filesystem::path& scene_file_path, bool update_project_start_scene);
    bool saveScene();
    bool saveSceneAs();
    bool saveSceneAs(const std::filesystem::path& scene_file_path);
    bool undo();
    bool redo();
    void syncDefaultViewportMouseCapture() noexcept;

    std::filesystem::path sceneDialogDefaultPath() const;
    void updateSceneLabel();
    void syncProjectStartScene(const std::filesystem::path& scene_file_path);

private:
    LunaEditorApplication* m_application{nullptr};
    EditorCamera m_editor_camera;
    EditorRuntime m_editor_runtime;
    EditorDocumentContext m_authoring_document_context{"luna.document.authoring.scene",
                                                       EditorDocumentKind::AuthoringScene,
                                                       true};
    EditorDocumentContext m_runtime_document_context{"luna.document.runtime.scene",
                                                     EditorDocumentKind::RuntimeSceneSnapshot,
                                                     false};
    std::unique_ptr<Scene> m_runtime_scene;
    std::unique_ptr<SceneRuntime> m_runtime_scene_runtime;
    std::string m_asset_label{"No scene loaded"};
    bool m_show_pick_debug_visualization{false};
    bool m_viewport_focused{false};
    bool m_show_editor_grid{true};
    bool m_gizmo_transform_transaction_active{false};
    float m_editor_ui_scale{0.0f};
    editor::EditorThemePreset m_editor_theme_preset{editor::EditorThemePreset::ModernLightweight};

    bool m_runtime_viewport_enabled{false};
    bool m_runtime_viewport_requested{false};
    GizmoOperation m_gizmo_operation{GizmoOperation::Translate};
    GizmoMode m_gizmo_mode{GizmoMode::Local};
    SceneViewportInstanceManager m_viewport_instances;
    TextureViewportInstanceManager m_texture_viewport_instances;
    ViewportInteractionTracker m_viewport_interactions;
    editor::ViewportId m_next_viewport_id{editor::kDefaultViewportId + 1u};
    std::unordered_map<editor::ViewportId, std::string> m_viewport_owner_by_id;
    std::unordered_map<editor::ViewportId, std::unique_ptr<PreviewSceneViewport>> m_preview_scene_viewports;
    std::unique_ptr<editor::EditorShell> m_editor_shell;
    std::unique_ptr<editor::EditorPluginManager> m_editor_plugin_manager;
};

} // namespace luna

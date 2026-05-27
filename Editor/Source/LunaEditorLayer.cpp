#include "Asset/AssetManager.h"
#include "Core/Log.h"
#include "EditorApi/EditorAssetService.h"
#include "EditorApi/EditorCommandService.h"
#include "EditorApi/EditorScriptPluginService.h"
#include "EditorApi/EditorStandardCommands.h"
#include "EditorApi/EditorUi.h"
#include "EditorApi/EditorWindowService.h"
#include "EditorStyle.h"
#include "Events/KeyEvent.h"
#include "Events/MouseEvent.h"
#include "Imgui/ImGuiContext.h"
#include "LunaEditorApp.h"
#include "LunaEditorLayer.h"
#include "Project/ProjectInfo.h"
#include "Renderer/RenderWorld/RenderWorldExtractor.h"
#include "Scene/Components.h"
#include "Shell/EditorPluginManager.h"
#include "Shell/EditorShell.h"

#include <cctype>
#include <cmath>

#include <algorithm>
#include <filesystem>
#include <imgui.h>
#include <ImGuizmo.h>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace {

constexpr float kUiScaleChangeThreshold = 0.01f;

luna::editor::TextureHandle toEditorTextureHandle(ImTextureID texture_id) noexcept
{
    if constexpr (std::is_pointer_v<ImTextureID>) {
        return reinterpret_cast<luna::editor::TextureHandle>(texture_id);
    } else {
        return static_cast<luna::editor::TextureHandle>(texture_id);
    }
}

} // namespace

namespace luna {

LunaEditorLayer::LunaEditorLayer(LunaEditorApplication& application)
    : Layer("LunaEditorLayer"),
      m_application(&application),
      m_rendering(application.getRenderer()),
      m_main_menu(EditorMainMenuController::Actions{
          .has_project_loaded =
              [this]() {
                  return hasProjectLoaded();
              },
          .open_project =
              [this](const std::filesystem::path& project_file_path) {
                  (void) openProject(project_file_path);
              },
          .sync_project_assets =
              [this]() {
                  (void) syncProjectAssets();
              },
          .refresh_project_script_plugins =
              [this]() {
                  refreshProjectScriptPlugins();
              },
          .create_scene =
              [this]() {
                  createScene();
              },
          .open_scene =
              [this]() {
                  (void) openScene();
              },
          .save_scene =
              [this]() {
                  (void) saveScene();
              },
      })
{
    m_editor_shell = std::make_unique<editor::EditorShell>(*this, application.editorSettings());
    m_editor_plugin_manager = std::make_unique<editor::EditorPluginManager>(*m_editor_shell);
    m_editor_shell->setPluginInfoProvider([this]() {
        return m_editor_plugin_manager ? m_editor_plugin_manager->pluginInfos() : std::vector<editor::PluginInfo>{};
    });
    for (auto& package : editor::createEditorPluginPackages(application.enginePaths())) {
        m_editor_plugin_manager->registerPackage(std::move(package));
    }
    (void) m_editor_plugin_manager->loadRegisteredPackages();
}

LunaEditorLayer::~LunaEditorLayer() = default;

void LunaEditorLayer::onAttach()
{
    if (m_application == nullptr) {
        return;
    }

    m_authoring.scene().setAssetLoadBehavior(Scene::AssetLoadBehavior::NonBlocking);
    m_authoring.documentContext().bindScene(&m_authoring.scene());
    m_authoring.documentContext().setRunning(false);
    m_runtime_viewport.runtimeDocumentContext().bindScene(nullptr);
    m_runtime_viewport.runtimeDocumentContext().setRunning(false);
    createScene();

    if (m_application->getImGuiLayer() != nullptr) {
        activeSceneViewportInstance().configureRenderer(m_application->getRenderer(), true);
        syncEditorGridFeatureState();
        syncPickDebugVisualizationState();
    } else {
        activeSceneViewportInstance().configureRenderer(m_application->getRenderer(), false);
        syncEditorGridFeatureState();
        LUNA_EDITOR_INFO("ImGui overlay disabled for backend '{}'",
                         luna::RHI::BackendTypeToString(m_application->getRenderer().getCapabilities().backend_type));
    }
}

void LunaEditorLayer::onDetach()
{
    if (m_application == nullptr) {
        return;
    }

    m_editor_camera.releaseMouseCapture();
    m_editor_camera.setInputEnabled(false);
    m_lifecycle.resetRuntimeViewportState(m_runtime_viewport, m_viewports);
    m_preview_scene_viewports.clearPreviews();
    m_viewports.clearPluginSceneViewports(m_application->getRenderer());
    m_viewports.clearTextureViewports();
    m_viewports.clearInteractions();
    activeSceneViewportInstance().resetRenderer(m_application->getRenderer());
    m_application->getRenderer().setRenderGraphProfilingEnabled(false);
}

void LunaEditorLayer::onUpdate(Timestep dt)
{
    AssetManager::get().updateAsyncLoads();
    m_authoring.processEvents();
    if (m_editor_shell) {
        m_editor_shell->update(dt.getSeconds());
    }
    consumePendingScenePick();
    setRuntimeViewportEnabled(m_runtime_viewport.isRuntimeViewportRequested());
    syncDefaultViewportMouseCapture();

    const bool runtime_viewport_enabled = m_runtime_viewport.isRuntimeViewportEnabled();
    const bool allow_editor_camera =
        !runtime_viewport_enabled && isViewportInputAllowed(defaultSceneViewportId()) && !ImGuizmo::IsUsing();
    m_editor_camera.setInputEnabled(allow_editor_camera);
    if (!runtime_viewport_enabled) {
        m_editor_camera.onUpdate(dt);
        syncDefaultViewportMouseCapture();
        activeRenderScene().renderFromEditorCamera(m_editor_camera.getCamera());
    } else {
        m_editor_camera.releaseMouseCapture();
        m_editor_camera.setInputEnabled(false);
        syncDefaultViewportMouseCapture();
        m_runtime_viewport.update(dt);
    }
}

void LunaEditorLayer::onEvent(Event& event)
{
    if (event.m_handled) {
        return;
    }

    syncDefaultViewportMouseCapture();
    if (isViewportInputAllowed(defaultSceneViewportId()) && !ImGuizmo::IsUsing()) {
        m_editor_camera.onEvent(event);
    }
}

void LunaEditorLayer::onImGuiRender()
{
    if (m_application == nullptr) {
        return;
    }

    syncEditorUiScale();
    ImGuizmo::BeginFrame();
    m_viewports.beginFrameInteractions();
    m_main_menu.dispatchShortcuts(m_editor_shell.get());

    m_main_menu.drawDockSpace(m_project_session, m_editor_shell.get());

    m_viewport_focused = false;
    if (m_editor_shell) {
        m_editor_shell->drawWindows();
    }
}

void LunaEditorLayer::syncEditorUiScale()
{
    const ImGuiViewport* main_viewport = ImGui::GetMainViewport();

    float ui_scale = 1.0f;
    if (main_viewport != nullptr && std::isfinite(main_viewport->DpiScale) && main_viewport->DpiScale > 0.0f) {
        ui_scale = main_viewport->DpiScale;
    }

    const editor::EditorThemePreset theme_preset = m_application != nullptr
                                                       ? m_application->editorSettings().data().theme_preset
                                                       : editor::EditorThemePreset::ModernLightweight;
    if (theme_preset == m_editor_theme_preset && std::abs(ui_scale - m_editor_ui_scale) <= kUiScaleChangeThreshold) {
        return;
    }

    editor::applyEditorTheme(theme_preset, ui_scale);
    m_editor_theme_preset = theme_preset;
    m_editor_ui_scale = editor::getEditorUiScale();
}

void LunaEditorLayer::drawDefaultSceneViewport(editor::Ui& ui, std::string_view owner_id)
{
    if (m_application == nullptr) {
        return;
    }

    auto& renderer = m_application->getRenderer();
    const EditorDefaultSceneViewportController::DrawResult draw_result =
        m_default_scene_viewport.draw(ui,
                                      owner_id,
                                      renderer,
                                      m_editor_camera,
                                      m_authoring,
                                      m_runtime_viewport,
                                      m_viewports,
                                      m_gizmo,
                                      activeSceneViewportInstance(),
                                      activeSceneViewportId(),
                                      getSelectedEntity());
    m_viewport_focused = draw_result.focused;
}

void LunaEditorLayer::consumePendingScenePick()
{
    if (m_application == nullptr) {
        return;
    }

    const std::optional<uint32_t> picked_id =
        activeSceneViewportInstance().consumeScenePickResult(m_application->getRenderer());
    if (!picked_id.has_value()) {
        return;
    }

    if (*picked_id == 0) {
        setSelectedEntity({});
        return;
    }

    auto& entity_manager = activeRenderScene().entityManager();
    const entt::entity entity_handle = static_cast<entt::entity>(*picked_id - 1u);
    if (!entity_manager.registry().valid(entity_handle)) {
        setSelectedEntity({});
        return;
    }

    setSelectedEntity(Entity(entity_handle, &entity_manager));
}

void LunaEditorLayer::syncPickDebugVisualizationState() const
{
    m_lifecycle.syncPickDebugVisualization(activeSceneViewportInstance(),
                                           m_application != nullptr ? &m_application->getRenderer() : nullptr,
                                           m_show_pick_debug_visualization);
}

void LunaEditorLayer::syncEditorGridFeatureState() const
{
    m_lifecycle.syncEditorGrid(activeSceneViewportInstance(),
                               m_application != nullptr ? &m_application->getRenderer() : nullptr,
                               m_show_editor_grid,
                               m_runtime_viewport);
}

const std::string& LunaEditorLayer::getAssetLabel() const
{
    return m_authoring.assetLabel();
}

std::string LunaEditorLayer::getRenderingBackendName() const
{
    return m_rendering.backendName();
}

editor::RenderingBackendCapabilities LunaEditorLayer::getRenderingBackendCapabilities() const
{
    return m_rendering.backendCapabilities();
}

editor::RenderGraphProfileSnapshot LunaEditorLayer::getRenderGraphProfileSnapshot() const
{
    return m_rendering.renderGraphProfile();
}

bool LunaEditorLayer::isRenderGraphProfilingEnabled() const noexcept
{
    return m_rendering.isRenderGraphProfilingEnabled();
}

void LunaEditorLayer::setRenderGraphProfilingEnabled(bool enabled)
{
    m_rendering.setRenderGraphProfilingEnabled(enabled);
}

std::filesystem::path LunaEditorLayer::defaultRenderProfileExportPath(std::string_view backend_name) const
{
    return m_rendering.defaultRenderProfileExportPath(backend_name);
}

bool LunaEditorLayer::exportRenderGraphProfileChromeTraceJson(const editor::RenderGraphProfileSnapshot& profile,
                                                              const std::filesystem::path& output_path,
                                                              std::string* error_message) const
{
    return m_rendering.exportRenderGraphProfileChromeTraceJson(profile, output_path, error_message);
}

std::vector<editor::RenderFeatureInfo> LunaEditorLayer::getDefaultRenderFeatureInfos() const
{
    return m_rendering.defaultRenderFeatureInfos();
}

std::vector<editor::RenderFeatureParameterInfo>
    LunaEditorLayer::getDefaultRenderFeatureParameters(std::string_view feature_name) const
{
    return m_rendering.defaultRenderFeatureParameters(feature_name);
}

bool LunaEditorLayer::setDefaultRenderFeatureEnabled(std::string_view feature_name, bool enabled)
{
    return m_rendering.setDefaultRenderFeatureEnabled(feature_name, enabled);
}

bool LunaEditorLayer::setDefaultRenderFeatureParameter(std::string_view feature_name,
                                                       std::string_view parameter_name,
                                                       const editor::RenderFeatureParameterValue& value)
{
    return m_rendering.setDefaultRenderFeatureParameter(feature_name, parameter_name, value);
}

std::vector<editor::RenderDebugViewModeInfo> LunaEditorLayer::getRenderDebugViewModes() const
{
    return m_rendering.renderDebugViewModes();
}

editor::RenderDebugViewMode LunaEditorLayer::getRenderDebugViewMode() const noexcept
{
    return m_rendering.renderDebugViewMode();
}

void LunaEditorLayer::setRenderDebugViewMode(editor::RenderDebugViewMode mode)
{
    m_rendering.setRenderDebugViewMode(mode);
}

float LunaEditorLayer::getRenderDebugVelocityScale() const noexcept
{
    return m_rendering.renderDebugVelocityScale();
}

void LunaEditorLayer::setRenderDebugVelocityScale(float scale)
{
    m_rendering.setRenderDebugVelocityScale(scale);
}

editor::TextureView LunaEditorLayer::getRenderDebugTextureView() const
{
    return m_rendering.renderDebugTextureView();
}

float LunaEditorLayer::getFrameTimeMilliseconds() const noexcept
{
    return m_rendering.frameTimeMilliseconds();
}

float LunaEditorLayer::getFramesPerSecond() const noexcept
{
    return m_rendering.framesPerSecond();
}

uint32_t LunaEditorLayer::getSceneOutputWidth() const noexcept
{
    return m_rendering.sceneOutputSize().x;
}

uint32_t LunaEditorLayer::getSceneOutputHeight() const noexcept
{
    return m_rendering.sceneOutputSize().y;
}

size_t LunaEditorLayer::getRuntimeEntityCount() const noexcept
{
    return m_runtime_viewport.runtimeEntityCount();
}

std::array<float, 3> LunaEditorLayer::getEditorCameraPosition() const noexcept
{
    const glm::vec3 camera_position = m_editor_camera.getCamera().getPosition();
    return {camera_position.x, camera_position.y, camera_position.z};
}

std::string LunaEditorLayer::getGizmoOperationName() const
{
    return m_gizmo.operationName();
}

std::string LunaEditorLayer::getGizmoModeName() const
{
    return m_gizmo.modeName();
}

bool LunaEditorLayer::isPickDebugVisualizationEnabled() const noexcept
{
    return m_show_pick_debug_visualization;
}

void LunaEditorLayer::setPickDebugVisualizationEnabled(bool enabled)
{
    if (m_show_pick_debug_visualization == enabled) {
        return;
    }

    m_show_pick_debug_visualization = enabled;
    syncPickDebugVisualizationState();
}

bool LunaEditorLayer::isEditorGridEnabled() const noexcept
{
    return m_show_editor_grid;
}

void LunaEditorLayer::setEditorGridEnabled(bool enabled)
{
    if (m_show_editor_grid == enabled) {
        return;
    }

    m_show_editor_grid = enabled;
    syncEditorGridFeatureState();
}

Scene& LunaEditorLayer::getScene()
{
    return m_authoring.documentScene();
}

Scene& LunaEditorLayer::getInspectionScene()
{
    if (m_runtime_viewport.isRuntimeViewportEnabled()) {
        if (Scene* runtime_scene = m_runtime_viewport.runtimeDocumentContext().scene(); runtime_scene != nullptr) {
            return *runtime_scene;
        }
    }

    return m_authoring.documentScene();
}

bool LunaEditorLayer::isRuntimeViewportEnabled() const noexcept
{
    return m_runtime_viewport.isRuntimeViewportEnabled();
}

bool LunaEditorLayer::isRuntimeViewportRequested() const noexcept
{
    return m_runtime_viewport.isRuntimeViewportRequested();
}

void LunaEditorLayer::setRuntimeViewportRequested(bool enabled)
{
    m_runtime_viewport.setRuntimeViewportRequested(enabled);
}

editor::ViewportId LunaEditorLayer::defaultSceneViewportId() const noexcept
{
    return m_viewports.defaultSceneViewportId();
}

editor::ViewportId LunaEditorLayer::createSceneViewport(std::string_view)
{
    return createSceneViewport({}, {});
}

editor::ViewportId LunaEditorLayer::createSceneViewport(std::string_view, std::string_view owner_id)
{
    if (m_application == nullptr) {
        return editor::kInvalidViewportId;
    }

    return m_viewports.createSceneViewport(owner_id);
}

void LunaEditorLayer::destroySceneViewport(editor::ViewportId viewport_id)
{
    if (m_application == nullptr) {
        return;
    }

    if (m_viewports.destroySceneViewport(viewport_id, m_application->getRenderer())) {
        m_preview_scene_viewports.clearPreview(viewport_id);
    }
}

bool LunaEditorLayer::isSceneViewportValid(editor::ViewportId viewport_id) const noexcept
{
    return m_viewports.isSceneViewportValid(viewport_id);
}

editor::ViewportPresentation LunaEditorLayer::syncSceneViewport(editor::ViewportId viewport_id,
                                                                editor::UVec2 framebuffer_size)
{
    return syncSceneViewport(viewport_id, framebuffer_size.x, framebuffer_size.y);
}

editor::ViewportPresentation LunaEditorLayer::syncSceneViewport(editor::ViewportId viewport_id,
                                                                uint32_t framebuffer_width,
                                                                uint32_t framebuffer_height)
{
    if (m_application == nullptr) {
        return {};
    }

    Renderer& renderer = m_application->getRenderer();
    SceneViewportInstance* viewport = m_viewports.findSceneViewport(viewport_id);
    if (viewport == nullptr) {
        return {};
    }

    const SceneViewportInstanceState& state = viewport->sync(renderer, framebuffer_width, framebuffer_height);
    if (viewport_id != defaultSceneViewportId()) {
        if (!syncPreviewSceneViewport(viewport_id, renderer, *viewport)) {
            RenderWorldExtractor{}.extract(
                activeRenderScene(),
                m_editor_camera.getCamera(),
                renderer.getSceneViewportRenderWorld(viewport->rendererViewportHandle(renderer)));
        }
    }
    editor::TextureView texture = getSceneTextureView(viewport_id);
    return editor::ViewportPresentation{
        .scene_texture = texture,
        .framebuffer_size = editor::UVec2{.x = state.width, .y = state.height},
        .presentable = state.presentable && texture.valid(),
    };
}

bool LunaEditorLayer::setSceneViewportPreview(editor::ViewportId viewport_id,
                                              const editor::SceneViewportPreviewState& state)
{
    if (viewport_id == defaultSceneViewportId() || !isSceneViewportValid(viewport_id)) {
        return false;
    }

    return m_preview_scene_viewports.setPreview(viewport_id, state);
}

void LunaEditorLayer::clearSceneViewportPreview(editor::ViewportId viewport_id)
{
    m_preview_scene_viewports.clearPreview(viewport_id);
}

editor::ViewportPresentation LunaEditorLayer::syncSceneViewport(uint32_t framebuffer_width, uint32_t framebuffer_height)
{
    return syncSceneViewport(defaultSceneViewportId(), framebuffer_width, framebuffer_height);
}

editor::TextureView LunaEditorLayer::getSceneTextureView(editor::ViewportId viewport_id) const
{
    if (m_application == nullptr) {
        return {};
    }

    const auto& renderer = m_application->getRenderer();
    const SceneViewportInstance* viewport = m_viewports.findSceneViewport(viewport_id);
    if (viewport == nullptr) {
        return {};
    }

    const auto& scene_texture = renderer.getSceneViewportOutputTexture(viewport->rendererViewportHandle(renderer));
    if (!scene_texture) {
        return {};
    }

    const ImTextureID texture_id = ImGuiRhiContext::GetTextureId(scene_texture);
    const SceneViewportInstanceState& state = viewport->state();
    return editor::TextureView{
        .id = toEditorTextureHandle(texture_id),
        .size = editor::UVec2{.x = scene_texture->GetWidth(), .y = scene_texture->GetHeight()},
        .y_flip = state.y_flip,
    };
}

editor::TextureView LunaEditorLayer::getSceneTextureView() const
{
    return getSceneTextureView(defaultSceneViewportId());
}

editor::ViewportId LunaEditorLayer::activeSceneViewportId() const noexcept
{
    return m_viewports.activeSceneViewportId(m_runtime_viewport.isRuntimeViewportEnabled());
}

editor::ViewportId LunaEditorLayer::createTextureViewport(std::string_view)
{
    return createTextureViewport({}, {});
}

editor::ViewportId LunaEditorLayer::createTextureViewport(std::string_view, std::string_view owner_id)
{
    return m_viewports.createTextureViewport(owner_id);
}

void LunaEditorLayer::destroyTextureViewport(editor::ViewportId viewport_id)
{
    (void) m_viewports.destroyTextureViewport(viewport_id);
}

bool LunaEditorLayer::isTextureViewportValid(editor::ViewportId viewport_id) const noexcept
{
    return m_viewports.isTextureViewportValid(viewport_id);
}

editor::TextureViewportPresentation LunaEditorLayer::syncTextureViewport(editor::ViewportId viewport_id,
                                                                         editor::TextureView texture,
                                                                         editor::UVec2 framebuffer_size)
{
    return m_viewports.syncTextureViewport(viewport_id, texture, framebuffer_size);
}

editor::TextureViewportPresentation LunaEditorLayer::textureViewportPresentation(editor::ViewportId viewport_id) const
{
    return m_viewports.textureViewportPresentation(viewport_id);
}

void LunaEditorLayer::destroyViewportsForOwner(std::string_view owner_id)
{
    if (m_application == nullptr) {
        return;
    }

    const std::vector<editor::ViewportId> destroyed_scene_viewports =
        m_viewports.destroyViewportsForOwner(owner_id, m_application->getRenderer());
    for (const editor::ViewportId viewport_id : destroyed_scene_viewports) {
        m_preview_scene_viewports.clearPreview(viewport_id);
    }
}

const ViewportInteractionState& LunaEditorLayer::recordViewportSurfaceInteraction(editor::ViewportId viewport_id,
                                                                                  std::string_view owner_id,
                                                                                  const ViewportInteractionInput& input)
{
    return m_viewports.recordViewportSurfaceInteraction(viewport_id, owner_id, input);
}

bool LunaEditorLayer::isViewportInputAllowed(editor::ViewportId viewport_id) const noexcept
{
    return m_viewports.isViewportInputAllowed(viewport_id);
}

void LunaEditorLayer::syncDefaultViewportMouseCapture() noexcept
{
    m_viewports.setDefaultViewportMouseCaptured(m_editor_camera.isMouseCaptured());
}

UUID LunaEditorLayer::getSelectedEntityId() const noexcept
{
    return m_authoring.selectedEntityId();
}

Entity LunaEditorLayer::getSelectedEntity()
{
    return m_authoring.selectedEntity(getInspectionScene());
}

void LunaEditorLayer::setSelectedEntity(Entity entity)
{
    m_authoring.setSelectedEntity(entity);
}

void LunaEditorLayer::setSelectedEntityId(UUID entity_id)
{
    m_authoring.setSelectedEntityId(entity_id);
}

void LunaEditorLayer::markSceneDirty()
{
    m_authoring.markSceneDirty();
}

void LunaEditorLayer::patchRuntimeScriptProperty(UUID entity_id, size_t script_index, size_t property_index)
{
    m_runtime_viewport.patchRuntimeScriptProperty(entity_id, script_index, property_index);
}

bool LunaEditorLayer::openSceneFile(const std::filesystem::path& scene_file_path)
{
    return openScene(scene_file_path, true);
}

Entity LunaEditorLayer::createEntity(const std::string& name, Entity parent)
{
    return m_authoring.createEntity(name, parent);
}

Entity LunaEditorLayer::createEntityFromModelAsset(AssetHandle model_handle, Entity parent)
{
    return m_authoring.createEntityFromModelAsset(model_handle, parent);
}

Entity LunaEditorLayer::createEntityFromMeshAsset(AssetHandle mesh_handle, Entity parent)
{
    return m_authoring.createEntityFromMeshAsset(mesh_handle, parent);
}

Entity LunaEditorLayer::createPrimitiveEntity(AssetHandle mesh_handle, Entity parent)
{
    return m_authoring.createPrimitiveEntity(mesh_handle, parent);
}

Entity LunaEditorLayer::createCameraEntity(Entity parent)
{
    return m_authoring.createCameraEntity(parent);
}

Entity LunaEditorLayer::createDirectionalLightEntity(Entity parent)
{
    return m_authoring.createDirectionalLightEntity(parent);
}

Entity LunaEditorLayer::createPointLightEntity(Entity parent)
{
    return m_authoring.createPointLightEntity(parent);
}

Entity LunaEditorLayer::createSpotLightEntity(Entity parent)
{
    return m_authoring.createSpotLightEntity(parent);
}

bool LunaEditorLayer::destroyEntity(Entity entity)
{
    return m_authoring.destroyEntity(entity);
}

bool LunaEditorLayer::reparentEntity(Entity entity, Entity parent, bool preserve_world_transform)
{
    return m_authoring.reparentEntity(entity, parent, preserve_world_transform);
}

bool LunaEditorLayer::addComponent(Entity entity, authoring::AuthoringComponentKind component_kind)
{
    return m_authoring.addComponent(entity, component_kind);
}

bool LunaEditorLayer::removeComponent(Entity entity, authoring::AuthoringComponentKind component_kind)
{
    return m_authoring.removeComponent(entity, component_kind);
}

void LunaEditorLayer::applyMeshAssetToEntity(Entity entity, AssetHandle mesh_handle)
{
    (void) m_authoring.applyMeshAssetToEntity(entity, mesh_handle);
}

bool LunaEditorLayer::setEntityName(Entity entity, std::string name)
{
    return m_authoring.setEntityName(entity, std::move(name));
}

bool LunaEditorLayer::setEntityTransform(Entity entity, const TransformComponent& transform)
{
    return m_authoring.setEntityTransform(entity, transform);
}

bool LunaEditorLayer::setCameraComponent(Entity entity, const CameraComponent& camera_component)
{
    return m_authoring.setCameraComponent(entity, camera_component);
}

bool LunaEditorLayer::setLightComponent(Entity entity, const LightComponent& light_component)
{
    return m_authoring.setLightComponent(entity, light_component);
}

bool LunaEditorLayer::setMeshComponent(Entity entity, const MeshComponent& mesh_component)
{
    return m_authoring.setMeshComponent(entity, mesh_component);
}

bool LunaEditorLayer::setScriptComponent(Entity entity, const ScriptComponent& script_component)
{
    return m_authoring.setScriptComponent(entity, script_component);
}

bool LunaEditorLayer::setSceneEnvironmentSettings(const SceneEnvironmentSettings& settings)
{
    return m_authoring.setSceneEnvironmentSettings(settings);
}

bool LunaEditorLayer::setSceneShadowSettings(const SceneShadowSettings& settings)
{
    return m_authoring.setSceneShadowSettings(settings);
}

void LunaEditorLayer::openBuiltinMaterialsPanel(AssetHandle material_handle)
{
    if (m_editor_shell) {
        editor::CommandSubject subject = std::nullopt;
        if (material_handle.isValid()) {
            subject = static_cast<uint64_t>(material_handle);
        }
        (void) m_editor_shell->commands().execute(editor::commands::kOpenBuiltinMaterials, std::move(subject));
    }
}

std::filesystem::path LunaEditorLayer::getProjectRootPath() const
{
    return m_project_session.projectRootPath();
}

const ProjectInfo* LunaEditorLayer::getProjectInfo() const
{
    return m_project_session.projectInfo();
}

bool LunaEditorLayer::hasProjectLoaded() const
{
    return m_project_session.hasProjectLoaded();
}

void LunaEditorLayer::refreshProjectScriptPlugins()
{
    m_project_session.refreshScriptPlugins(m_editor_shell.get());
}

const std::vector<ScriptPluginCandidate>& LunaEditorLayer::getDiscoveredScriptPlugins() const
{
    static const std::vector<ScriptPluginCandidate> kEmptyCandidates;
    return m_editor_shell ? m_editor_shell->scriptPlugins().getDiscoveredScriptPlugins() : kEmptyCandidates;
}

const std::string& LunaEditorLayer::getScriptPluginStatus() const
{
    static const std::string kEmptyStatus;
    return m_editor_shell ? m_editor_shell->scriptPlugins().getScriptPluginStatus() : kEmptyStatus;
}

const ScriptPluginCandidate* LunaEditorLayer::getSelectedScriptPluginCandidate() const
{
    return m_editor_shell ? m_editor_shell->scriptPlugins().getSelectedScriptPluginCandidate() : nullptr;
}

bool LunaEditorLayer::selectScriptPlugin(const ScriptPluginCandidate* candidate)
{
    if (m_editor_shell) {
        return m_editor_shell->scriptPlugins().selectScriptPlugin(candidate);
    }
    return false;
}

void LunaEditorLayer::resetEditorState()
{
    m_lifecycle.resetEditorDocumentState(m_runtime_viewport,
                                         m_viewports,
                                         m_authoring,
                                         m_show_pick_debug_visualization,
                                         m_viewports.activeSceneViewport(false),
                                         m_application != nullptr ? &m_application->getRenderer() : nullptr,
                                         m_show_editor_grid);
}

void LunaEditorLayer::setRuntimeViewportEnabled(bool enabled)
{
    (void) m_runtime_session.setRuntimeViewportEnabled(enabled,
                                                       m_runtime_viewport,
                                                       m_authoring,
                                                       m_project_session,
                                                       m_viewports,
                                                       m_gizmo,
                                                       m_lifecycle,
                                                       m_editor_camera,
                                                       m_application != nullptr ? &m_application->getRenderer()
                                                                                : nullptr,
                                                       m_show_editor_grid);
}

Scene& LunaEditorLayer::activeRenderScene()
{
    return m_runtime_viewport.activeRenderScene(m_authoring.scene());
}

SceneViewportInstance& LunaEditorLayer::activeSceneViewportInstance() noexcept
{
    return m_viewports.activeSceneViewport(m_runtime_viewport.isRuntimeViewportEnabled());
}

const SceneViewportInstance& LunaEditorLayer::activeSceneViewportInstance() const noexcept
{
    return m_viewports.activeSceneViewport(m_runtime_viewport.isRuntimeViewportEnabled());
}

bool LunaEditorLayer::syncPreviewSceneViewport(editor::ViewportId viewport_id,
                                               Renderer& renderer,
                                               SceneViewportInstance& viewport)
{
    return m_preview_scene_viewports.syncPreview(viewport_id, renderer, viewport);
}

void LunaEditorLayer::createScene()
{
    m_lifecycle.resetAuthoringViewportState(m_runtime_viewport,
                                            m_viewports,
                                            m_show_pick_debug_visualization,
                                            m_viewports.activeSceneViewport(false),
                                            m_application != nullptr ? &m_application->getRenderer() : nullptr,
                                            m_show_editor_grid);

    const auto bootstrap = m_authoring.createScene();
    if (bootstrap.directional_light) {
        setSelectedEntity(bootstrap.directional_light);
    } else if (bootstrap.camera) {
        setSelectedEntity(bootstrap.camera);
    }
    LUNA_EDITOR_INFO("Created a new scene with a primary camera and directional light");
}

bool LunaEditorLayer::syncProjectAssets()
{
    return m_project_session.refreshAssets(m_editor_shell.get());
}

bool LunaEditorLayer::openProject(const std::filesystem::path& project_file_path)
{
    if (!m_project_session.loadProject(project_file_path)) {
        return false;
    }

    resetEditorState();

    m_project_session.reloadProjectAssets(m_editor_shell.get());
    m_project_session.refreshScriptPlugins(m_editor_shell.get());

    createScene();

    if (const auto start_scene_path = m_project_session.configuredStartScenePath()) {
        if (std::filesystem::exists(*start_scene_path)) {
            if (!openScene(*start_scene_path, false)) {
                createScene();
                m_authoring.setSceneFilePath(*start_scene_path);
            }
        } else {
            m_authoring.setSceneFilePath(*start_scene_path);
            LUNA_EDITOR_WARN("Configured StartScene '{}' does not exist. Saving will create it at that location.",
                             start_scene_path->string());
        }
    } else {
        m_authoring.updateAssetLabel();
        LUNA_EDITOR_INFO("Project '{}' does not define a StartScene. Using an empty scene.",
                         project_file_path.string());
    }

    LUNA_EDITOR_INFO("Loaded project '{}' with {} scene entities",
                     project_file_path.string(),
                     m_authoring.scene().entityManager().entityCount());
    return true;
}

bool LunaEditorLayer::openScene()
{
    m_lifecycle.resetRuntimeViewportState(m_runtime_viewport, m_viewports);
    return m_scene_files.openSceneDialog(m_authoring, m_project_session, true);
}

bool LunaEditorLayer::openScene(const std::filesystem::path& scene_file_path, bool update_project_start_scene)
{
    m_lifecycle.resetRuntimeViewportState(m_runtime_viewport, m_viewports);
    return m_scene_files.openScene(scene_file_path, m_authoring, m_project_session, update_project_start_scene);
}

bool LunaEditorLayer::saveScene()
{
    return m_scene_files.saveScene(m_authoring, m_project_session);
}

bool LunaEditorLayer::saveSceneAs()
{
    return m_scene_files.saveSceneAsDialog(m_authoring, m_project_session);
}

bool LunaEditorLayer::saveSceneAs(const std::filesystem::path& scene_file_path)
{
    return m_scene_files.saveSceneAs(scene_file_path, m_authoring, m_project_session);
}

bool LunaEditorLayer::canUndo() const noexcept
{
    return !m_runtime_viewport.isRuntimeViewportEnabled() && m_authoring.canUndo();
}

bool LunaEditorLayer::canRedo() const noexcept
{
    return !m_runtime_viewport.isRuntimeViewportEnabled() && m_authoring.canRedo();
}

bool LunaEditorLayer::hasOpenEditorTransaction() const noexcept
{
    return m_authoring.hasOpenTransaction();
}

bool LunaEditorLayer::beginEditorTransaction(std::string name)
{
    if (m_runtime_viewport.isRuntimeViewportEnabled()) {
        return false;
    }

    return m_authoring.beginTransaction(std::move(name));
}

bool LunaEditorLayer::commitEditorTransaction()
{
    return m_authoring.commitTransaction();
}

bool LunaEditorLayer::rollbackEditorTransaction()
{
    return m_authoring.rollbackTransaction();
}

bool LunaEditorLayer::undoEditorCommand()
{
    return undo();
}

bool LunaEditorLayer::redoEditorCommand()
{
    return redo();
}

bool LunaEditorLayer::undo()
{
    if (m_runtime_viewport.isRuntimeViewportEnabled() || !m_authoring.undo()) {
        return false;
    }

    return true;
}

bool LunaEditorLayer::redo()
{
    if (m_runtime_viewport.isRuntimeViewportEnabled() || !m_authoring.redo()) {
        return false;
    }

    return true;
}

} // namespace luna

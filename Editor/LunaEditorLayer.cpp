#include "Asset/AssetDatabase.h"
#include "Asset/AssetManager.h"
#include "Asset/BuiltinAssets.h"
#include "Asset/Editor/ImporterManager.h"
#include "Asset/Model.h"
#include "Core/Log.h"
#include "Events/KeyEvent.h"
#include "Events/MouseEvent.h"
#include "Imgui/ImGuiContext.h"
#include "LunaEditorApp.h"
#include "LunaEditorLayer.h"
#include "Platform/Common/FileDialogs.h"
#include "Project/BuiltinMaterialOverrides.h"
#include "Project/ProjectInfo.h"
#include "Project/ProjectManager.h"
#include "Renderer/Mesh.h"
#include "Scene/Components.h"
#include "Scene/SceneSerializer.h"

#include <algorithm>
#include <filesystem>
#include <glm/gtc/type_ptr.hpp>
#include <ImGuizmo.h>
#include <imgui.h>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace {

constexpr const char* kProjectFileFilter = "Luna Project (*.lunaproj)\0*.lunaproj\0";
constexpr const char* kSceneFileFilter = "Luna Scene (*.lunascene)\0*.lunascene\0";
constexpr const char* kPickDebugToggleLabel = "Show Picking Debug";

const char* backendTypeToString(luna::RHI::BackendType type)
{
    switch (type) {
        case luna::RHI::BackendType::Auto:
            return "Auto";
        case luna::RHI::BackendType::Vulkan:
            return "Vulkan";
        case luna::RHI::BackendType::DirectX12:
            return "DirectX12";
        case luna::RHI::BackendType::DirectX11:
            return "DirectX11";
        case luna::RHI::BackendType::Metal:
            return "Metal";
        case luna::RHI::BackendType::OpenGL:
            return "OpenGL";
        case luna::RHI::BackendType::OpenGLES:
            return "OpenGLES";
        case luna::RHI::BackendType::WebGPU:
            return "WebGPU";
        default:
            return "Unknown";
    }
}

const char* gizmoOperationToString(luna::GizmoOperation operation)
{
    switch (operation) {
        case luna::GizmoOperation::Translate:
            return "Translate";
        case luna::GizmoOperation::Rotate:
            return "Rotate";
        case luna::GizmoOperation::Scale:
            return "Scale";
    }
    return "Unknown";
}

ImGuizmo::OPERATION toImGuizmoOperation(luna::GizmoOperation operation)
{
    switch (operation) {
        case luna::GizmoOperation::Translate:
            return ImGuizmo::TRANSLATE;
        case luna::GizmoOperation::Rotate:
            return ImGuizmo::ROTATE;
        case luna::GizmoOperation::Scale:
            return ImGuizmo::SCALE;
    }
    return ImGuizmo::TRANSLATE;
}

ImGuizmo::MODE toImGuizmoMode(luna::GizmoMode mode)
{
    return mode == luna::GizmoMode::World ? ImGuizmo::WORLD : ImGuizmo::LOCAL;
}

void logEditorAssetSyncStats(const luna::ImporterManager::ImportStats& stats)
{
    LUNA_EDITOR_INFO(
        "Project asset sync: discovered={}, imported_missing={}, loaded_existing={}, rebuilt={}, unsupported={}, "
        "failed={}, missing_after_sync={}, generated_models={}, generated_model_meta={}, generated_materials={}, "
        "generated_material_meta={}, generated_texture_meta={}, generated_model_failures={}",
        stats.discoveredAssets,
        stats.importedMissingAssets,
        stats.loadedExistingMetadata,
        stats.rebuiltMetadata,
        stats.unsupportedFilesSkipped,
        stats.failedAssets,
        stats.missingMetadataAfterSync,
        stats.generatedModelFiles,
        stats.generatedModelMetadata,
        stats.generatedMaterialFiles,
        stats.generatedMaterialMetadata,
        stats.generatedTextureMetadata,
        stats.failedGeneratedModelAssets);
}

std::filesystem::path projectDialogDefaultPath()
{
    if (const auto project_root = luna::ProjectManager::instance().getProjectRootPath()) {
        return *project_root;
    }

    return std::filesystem::current_path();
}

std::optional<std::filesystem::path> makeScenePathRelativeToProject(const std::filesystem::path& scene_file_path)
{
    const auto project_root = luna::ProjectManager::instance().getProjectRootPath();
    if (!project_root || scene_file_path.empty()) {
        return std::nullopt;
    }

    std::error_code ec;
    std::filesystem::path relative_path = std::filesystem::relative(scene_file_path, *project_root, ec);
    if (ec) {
        return std::nullopt;
    }

    relative_path = relative_path.lexically_normal();
    if (relative_path.empty() || relative_path.is_absolute()) {
        return std::nullopt;
    }

    const std::string relative_string = relative_path.generic_string();
    if (relative_string == "." || relative_string.starts_with("..")) {
        return std::nullopt;
    }

    return relative_path;
}

} // namespace

namespace luna {

LunaEditorLayer::LunaEditorLayer(LunaEditorApplication& application)
    : Layer("LunaEditorLayer"),
      m_application(&application),
      m_scene_hierarchy_panel(*this),
      m_inspector_panel(*this),
      m_content_browser_panel(*this)
{}

void LunaEditorLayer::onAttach()
{
    if (m_application == nullptr) {
        return;
    }

    m_scene.setAssetLoadBehavior(Scene::AssetLoadBehavior::NonBlocking);
    createScene();

    if (m_application->getImGuiLayer() != nullptr) {
        m_application->getRenderer().setSceneOutputMode(Renderer::SceneOutputMode::OffscreenTexture);
        syncPickDebugVisualizationState();
    } else {
        m_application->getRenderer().setSceneOutputMode(Renderer::SceneOutputMode::Swapchain);
        LUNA_EDITOR_INFO("ImGui overlay disabled for backend '{}'", backendTypeToString(m_application->getBackend()));
    }

    if (auto* imgui_layer = m_application->getImGuiLayer(); imgui_layer != nullptr) {
        imgui_layer->setMenuBarCallback([this]() {
            onImGuiMenuBar();
        });
    }
}

void LunaEditorLayer::onDetach()
{
    if (m_application == nullptr) {
        return;
    }

    m_editor_camera.releaseMouseCapture();
    m_editor_camera.setInputEnabled(false);
    m_application->getRenderer().setRenderGraphProfilingEnabled(false);
    m_application->getRenderer().setRenderDebugViewMode(RenderDebugViewMode::None);
    m_application->getRenderer().setScenePickDebugVisualizationEnabled(false);

    if (auto* imgui_layer = m_application->getImGuiLayer(); imgui_layer != nullptr) {
        imgui_layer->setMenuBarCallback({});
    }
}

void LunaEditorLayer::onUpdate(Timestep dt)
{
    AssetManager::get().updateAsyncLoads();
    consumePendingScenePick();

    const bool allow_editor_camera = !m_runtime_viewport_enabled &&
                                     (m_viewport_focused || m_viewport_hovered || m_editor_camera.isMouseCaptured()) &&
                                     !ImGuizmo::IsUsing();
    m_editor_camera.setInputEnabled(allow_editor_camera);
    if (!m_runtime_viewport_enabled) {
        m_editor_camera.onUpdate(dt);
        m_scene.onUpdateEditor(m_editor_camera.getCamera());
    } else {
        m_editor_camera.releaseMouseCapture();
        m_scene.onUpdateRuntime();
    }
}

void LunaEditorLayer::onEvent(Event& event)
{
    if (event.m_handled) {
        return;
    }

    if ((m_viewport_focused || m_viewport_hovered || m_editor_camera.isMouseCaptured()) && !ImGuizmo::IsUsing()) {
        m_editor_camera.onEvent(event);
    }
}

void LunaEditorLayer::onImGuiRender()
{
    if (m_application == nullptr) {
        return;
    }

    ImGuizmo::BeginFrame();

    auto& application = *m_application;
    auto& renderer = application.getRenderer();
    const float delta_seconds = Application::get().getTimestep().getSeconds();
    const float fps = 1.0f / (std::max) (delta_seconds, 0.0001f);

    ImGui::SetNextWindowSize(ImVec2(420.0f, 0.0f), ImGuiCond_FirstUseEver);
    ImGui::Begin("Scene");
    ImGui::Text("Backend: Luna RHI / %s", backendTypeToString(application.getBackend()));
    ImGui::Text("Frame: %.2f ms  |  %.1f FPS", delta_seconds * 1000.0f, fps);
    ImGui::Separator();
    ImGui::Text("Scene File: %s", m_asset_label.c_str());
    ImGui::Text("Entities: %zu", m_scene.entityManager().entityCount());
    ImGui::Separator();

    const auto viewport_extent = renderer.getSceneOutputSize();
    ImGui::Text("Viewport: %u x %u", viewport_extent.width, viewport_extent.height);
    ImGui::Text("Viewport Mode: %s", m_runtime_viewport_enabled ? "Runtime" : "Editor");
    const glm::vec3 camera_position = m_editor_camera.getCamera().getPosition();
    ImGui::Text("Editor Camera: %.2f, %.2f, %.2f", camera_position.x, camera_position.y, camera_position.z);
    ImGui::Text("Gizmo: %s / %s", gizmoOperationToString(m_gizmo_operation), m_gizmo_mode == GizmoMode::World ? "World" : "Local");
    ImGui::TextUnformatted("Gizmo shortcuts: W Translate, E Rotate, R Scale, Q Local/World.");
    if (ImGui::Checkbox(kPickDebugToggleLabel, &m_show_pick_debug_visualization)) {
        syncPickDebugVisualizationState();
    }
    ImGui::TextUnformatted("Highlights pickable pixels and shows the requested pick marker.");
    ImGui::TextUnformatted(
        "Scene rendering now targets a persistent offscreen texture and is presented in the Viewport panel.");
    ImGui::End();

    m_scene_hierarchy_panel.onImGuiRender();
    m_inspector_panel.onImGuiRender();
    m_asset_loading_panel.onImGuiRender();
    m_builtin_materials_panel.onImGuiRender(m_show_builtin_materials_panel);
    m_content_browser_panel.onImGuiRender();
    m_render_debug_panel.onImGuiRender(m_show_render_debug_panel, renderer);
    if (m_show_render_features_panel) {
        m_render_features_panel.onImGuiRender(m_show_render_features_panel,
                                              renderer.getDefaultRenderFeatureInfos(),
                                              [&renderer](std::string_view feature_name) {
                                                  return renderer.getDefaultRenderFeatureParameters(feature_name);
                                              },
                                              [&renderer](std::string_view feature_name, bool enabled) {
                                                  return renderer.setDefaultRenderFeatureEnabled(feature_name, enabled);
                                              },
                                              [&renderer](std::string_view feature_name,
                                                          std::string_view parameter_name,
                                                          const render_flow::RenderFeatureParameterValue& value) {
                                                  return renderer.setDefaultRenderFeatureParameter(
                                                      feature_name, parameter_name, value);
                                              });
    }
    m_render_profiler_panel.onImGuiRender(m_show_render_profiler_panel,
                                          renderer.getLastRenderGraphProfile(),
                                          backendTypeToString(application.getBackend()),
                                          renderer.isRenderGraphProfilingEnabled(),
                                          [&renderer](bool enabled) {
                                              renderer.setRenderGraphProfilingEnabled(enabled);
                                          });
    m_backend_capabilities_panel.onImGuiRender(m_show_backend_capabilities_panel, renderer);
    drawViewport();
}

void LunaEditorLayer::onImGuiMenuBar()
{
    const bool project_loaded = hasProjectLoaded();

    if (ImGui::BeginMenu("Project")) {
        if (ImGui::MenuItem("Open Project")) {
            const std::filesystem::path project_file_path =
                FileDialogs::openFile(kProjectFileFilter, projectDialogDefaultPath().string());
            if (!project_file_path.empty()) {
                openProject(project_file_path);
            }
        }

        if (ImGui::MenuItem("Create New Project")) {
            const std::filesystem::path project_root_path =
                FileDialogs::selectDirectory(projectDialogDefaultPath().string());
            if (!project_root_path.empty()) {
                ProjectInfo project_info{.Name = "New Project",
                                         .Version = "0.1.0",
                                         .Author = "Junjie Yang",
                                         .Description = "A simple Luna project.",
                                         .StartScene = "./Assets/Scenes/Main.lunascene",
                                         .AssetsPath = "./Assets/"};

                if (ProjectManager::instance().createProject(project_root_path, project_info)) {
                    std::error_code ec;
                    if (!project_info.AssetsPath.empty()) {
                        std::filesystem::create_directories(
                            (project_root_path / project_info.AssetsPath).lexically_normal(), ec);
                    }

                    ec.clear();
                    if (!project_info.StartScene.empty()) {
                        const auto scene_directory =
                            (project_root_path / project_info.StartScene).lexically_normal().parent_path();
                        if (!scene_directory.empty()) {
                            std::filesystem::create_directories(scene_directory, ec);
                        }
                    }

                    openProject(project_root_path / (project_info.Name + ".lunaproj"));
                }
            }
        }

        if (ImGui::MenuItem("Sync Assets", nullptr, false, project_loaded)) {
            syncProjectAssets();
        }
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Scene", project_loaded)) {
        if (ImGui::MenuItem("Create Scene")) {
            createScene();
        }

        if (ImGui::MenuItem("Open Scene")) {
            openScene();
        }

        if (ImGui::MenuItem("Save Scene")) {
            saveScene();
        }
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Viewport")) {
        ImGui::MenuItem("Runtime Viewport", nullptr, &m_runtime_viewport_enabled);
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Window")) {
        ImGui::MenuItem("Builtin Materials", nullptr, &m_show_builtin_materials_panel);
        ImGui::MenuItem("Render Debug", nullptr, &m_show_render_debug_panel);
        ImGui::MenuItem("Render Features", nullptr, &m_show_render_features_panel);
        ImGui::MenuItem("Render Profiler", nullptr, &m_show_render_profiler_panel);
        ImGui::MenuItem("Backend Capabilities", nullptr, &m_show_backend_capabilities_panel);
        ImGui::EndMenu();
    }
}

void LunaEditorLayer::drawViewport()
{
    if (m_application == nullptr) {
        return;
    }

    auto& renderer = m_application->getRenderer();

    ImGui::SetNextWindowSize(ImVec2(960.0f, 640.0f), ImGuiCond_FirstUseEver);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::Begin("Viewport");
    m_viewport_focused = ImGui::IsWindowFocused();
    m_viewport_hovered = ImGui::IsWindowHovered();
    updateGizmoShortcuts();

    const ImVec2 available = ImGui::GetContentRegionAvail();
    const float dpi_scale = ImGui::GetWindowViewport() != nullptr ? ImGui::GetWindowViewport()->DpiScale : 1.0f;
    const uint32_t viewport_width = static_cast<uint32_t>((std::max) (available.x * dpi_scale, 0.0f));
    const uint32_t viewport_height = static_cast<uint32_t>((std::max) (available.y * dpi_scale, 0.0f));
    renderer.setSceneOutputSize(viewport_width, viewport_height);
    m_editor_camera.setViewportSize(static_cast<float>(viewport_width), static_cast<float>(viewport_height));

    const auto& scene_texture = renderer.getSceneOutputTexture();
    const ImTextureID texture_id = ImGuiRhiContext::GetTextureId(scene_texture);
    if (texture_id != 0 && available.x > 0.0f && available.y > 0.0f) {
        const bool flip_uv_y = renderer.getCapabilities().conventions.imgui_render_target_requires_uv_y_flip;
        const ImVec2 uv0(0.0f, flip_uv_y ? 1.0f : 0.0f);
        const ImVec2 uv1(1.0f, flip_uv_y ? 0.0f : 1.0f);

        ImGui::Image(texture_id, available, uv0, uv1);
        const ImVec2 viewport_min = ImGui::GetItemRectMin();
        const ImVec2 viewport_size = ImGui::GetItemRectSize();
        const bool gizmo_active = !m_runtime_viewport_enabled && drawViewportGizmo(viewport_min, viewport_size);
        if (!gizmo_active) {
            if (!m_runtime_viewport_enabled) {
                requestViewportPick(
                    ImGui::GetItemRectMin(),
                    ImGui::GetItemRectMax(),
                    uv0,
                    uv1,
                    scene_texture ? luna::RHI::Extent2D{scene_texture->GetWidth(), scene_texture->GetHeight()}
                                  : luna::RHI::Extent2D{0, 0});
            }
        }
    } else if (available.x > 0.0f && available.y > 0.0f) {
        ImGui::SetCursorPos(ImVec2(16.0f, 16.0f));
        ImGui::TextUnformatted("Viewport texture will appear after the first rendered frame.");
    }

    ImGui::End();
    ImGui::PopStyleVar();
}

void LunaEditorLayer::updateGizmoShortcuts()
{
    if (!m_viewport_focused || m_editor_camera.isMouseCaptured() || ImGui::GetIO().WantTextInput || !m_selected_entity ||
        !m_selected_entity.isValid()) {
        return;
    }

    if (ImGui::IsKeyPressed(ImGuiKey_W, false)) {
        m_gizmo_operation = GizmoOperation::Translate;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_E, false)) {
        m_gizmo_operation = GizmoOperation::Rotate;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_R, false)) {
        m_gizmo_operation = GizmoOperation::Scale;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_Q, false)) {
        m_gizmo_mode = m_gizmo_mode == GizmoMode::Local ? GizmoMode::World : GizmoMode::Local;
    }
}

bool LunaEditorLayer::drawViewportGizmo(const ImVec2& viewport_min, const ImVec2& viewport_size)
{
    Entity selected_entity = getSelectedEntity();
    if (!selected_entity || !selected_entity.isValid() || !selected_entity.hasComponent<TransformComponent>()) {
        return false;
    }

    if (viewport_size.x <= 0.0f || viewport_size.y <= 0.0f) {
        return false;
    }

    const auto& camera = m_editor_camera.getCamera();
    const float aspect_ratio = viewport_size.x / viewport_size.y;
    glm::mat4 view = camera.getViewMatrix();
    glm::mat4 projection = camera.getProjectionMatrix(aspect_ratio);
    projection[1][1] *= -1.0f;
    glm::mat4 transform = m_scene.entityManager().getWorldSpaceTransformMatrix(selected_entity);

    ImGuizmo::SetOrthographic(camera.getProjectionType() == Camera::ProjectionType::Orthographic);
    ImGuizmo::SetDrawlist();
    ImGuizmo::SetRect(viewport_min.x, viewport_min.y, viewport_size.x, viewport_size.y);
    ImGuizmo::PushID(static_cast<int>(static_cast<uint64_t>(selected_entity.getUUID()) & 0x7fffffff));

    const ImGuizmo::MODE mode = m_gizmo_operation == GizmoOperation::Scale ? ImGuizmo::LOCAL : toImGuizmoMode(m_gizmo_mode);
    ImGuizmo::Manipulate(glm::value_ptr(view),
                         glm::value_ptr(projection),
                         toImGuizmoOperation(m_gizmo_operation),
                         mode,
                         glm::value_ptr(transform));

    if (ImGuizmo::IsUsing()) {
        m_scene.entityManager().setWorldSpaceTransform(selected_entity, transform);
    }

    const bool gizmo_active = ImGuizmo::IsOver() || ImGuizmo::IsUsing();
    ImGuizmo::PopID();
    return gizmo_active;
}

void LunaEditorLayer::consumePendingScenePick()
{
    if (m_application == nullptr) {
        return;
    }

    const std::optional<uint32_t> picked_id = m_application->getRenderer().consumeScenePickResult();
    if (!picked_id.has_value()) {
        return;
    }

    if (*picked_id == 0) {
        setSelectedEntity({});
        return;
    }

    auto& entity_manager = m_scene.entityManager();
    const entt::entity entity_handle = static_cast<entt::entity>(*picked_id - 1u);
    if (!entity_manager.registry().valid(entity_handle)) {
        setSelectedEntity({});
        return;
    }

    setSelectedEntity(Entity(entity_handle, &entity_manager));
}

void LunaEditorLayer::syncPickDebugVisualizationState() const
{
    if (m_application == nullptr) {
        return;
    }

    m_application->getRenderer().setScenePickDebugVisualizationEnabled(m_show_pick_debug_visualization);
}

void LunaEditorLayer::requestViewportPick(const ImVec2& image_min,
                                          const ImVec2& image_max,
                                          const ImVec2& uv0,
                                          const ImVec2& uv1,
                                          const luna::RHI::Extent2D& texture_extent) const
{
    if (m_application == nullptr || texture_extent.width == 0 || texture_extent.height == 0 ||
        m_editor_camera.isMouseCaptured() || ImGuizmo::IsOver() || ImGuizmo::IsUsing() ||
        !ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        return;
    }

    const ImVec2 mouse_position = ImGui::GetMousePos();
    const float image_width = image_max.x - image_min.x;
    const float image_height = image_max.y - image_min.y;
    if (image_width <= 0.0f || image_height <= 0.0f) {
        return;
    }

    if (mouse_position.x < image_min.x || mouse_position.x >= image_max.x || mouse_position.y < image_min.y ||
        mouse_position.y >= image_max.y) {
        return;
    }

    const float local_x = std::clamp((mouse_position.x - image_min.x) / image_width, 0.0f, 0.999999f);
    const float local_y = std::clamp((mouse_position.y - image_min.y) / image_height, 0.0f, 0.999999f);

    const float texture_u = std::clamp(uv0.x + (uv1.x - uv0.x) * local_x, 0.0f, 0.999999f);
    const float texture_v = std::clamp(uv0.y + (uv1.y - uv0.y) * local_y, 0.0f, 0.999999f);

    const uint32_t pixel_x = static_cast<uint32_t>(texture_u * static_cast<float>(texture_extent.width));
    const uint32_t color_pixel_y = (std::min) (
        static_cast<uint32_t>(texture_v * static_cast<float>(texture_extent.height)), texture_extent.height - 1);

    // The final scene color is produced by a fullscreen lighting pass before ImGui samples it.
    // Some backends need the displayed color-space Y converted back to the raw pick attachment Y.
    const bool pick_y_matches_display_y =
        m_application->getRenderer().getCapabilities().conventions.scene_pick_y_matches_display_y;
    const uint32_t pick_pixel_y =
        pick_y_matches_display_y ? color_pixel_y : (texture_extent.height - 1) - color_pixel_y;

    m_application->getRenderer().requestScenePick((std::min) (pixel_x, texture_extent.width - 1), pick_pixel_y);
}

const std::string& LunaEditorLayer::getAssetLabel() const
{
    return m_asset_label;
}

Scene& LunaEditorLayer::getScene()
{
    return m_scene;
}

Entity LunaEditorLayer::getSelectedEntity() const
{
    return m_selected_entity;
}

void LunaEditorLayer::setSelectedEntity(Entity entity)
{
    m_selected_entity = entity;
}

bool LunaEditorLayer::openSceneFile(const std::filesystem::path& scene_file_path)
{
    return openScene(scene_file_path, true);
}

Entity LunaEditorLayer::createEntityFromModelAsset(AssetHandle model_handle, Entity parent)
{
    if (!model_handle.isValid() || !AssetDatabase::exists(model_handle)) {
        return {};
    }

    const auto& metadata = AssetDatabase::getAssetMetadata(model_handle);
    if (metadata.Type != AssetType::Model) {
        return {};
    }

    const auto model = AssetManager::get().loadAssetAs<Model>(model_handle);
    if (!model || !model->isValid()) {
        return {};
    }

    const std::string root_name =
        !model->getName().empty()
            ? model->getName()
            : (!metadata.Name.empty() ? metadata.Name
                                      : (!metadata.FilePath.empty() ? metadata.FilePath.stem().string() : "Model"));

    auto& entity_manager = m_scene.entityManager();
    Entity root = parent ? entity_manager.createChildEntity(parent, root_name) : entity_manager.createEntity(root_name);
    if (!root) {
        return {};
    }

    const auto& nodes = model->getNodes();
    std::vector<Entity> node_entities(nodes.size());
    for (size_t node_index = 0; node_index < nodes.size(); ++node_index) {
        const ModelNode& model_node = nodes[node_index];
        const std::string node_name =
            model_node.Name.empty() ? root_name + "_Node_" + std::to_string(node_index) : model_node.Name;

        Entity node_entity = entity_manager.createChildEntity(root, node_name);
        if (!node_entity) {
            continue;
        }

        auto& transform = node_entity.transform();
        transform.translation = model_node.Translation;
        transform.rotation = model_node.Rotation;
        transform.scale = model_node.Scale;

        if (model_node.MeshHandle.isValid()) {
            applyMeshAssetToEntity(node_entity, model_node.MeshHandle);
            if (node_entity.hasComponent<MeshComponent>()) {
                auto& mesh_component = node_entity.getComponent<MeshComponent>();
                for (uint32_t material_index = 0; material_index < model_node.SubmeshMaterials.size();
                     ++material_index) {
                    const AssetHandle material_handle = model_node.SubmeshMaterials[material_index];
                    if (material_handle.isValid()) {
                        mesh_component.setSubmeshMaterial(material_index, material_handle);
                    }
                }
            }
        }

        node_entities[node_index] = node_entity;
    }

    for (size_t node_index = 0; node_index < nodes.size(); ++node_index) {
        Entity node_entity = node_entities[node_index];
        if (!node_entity) {
            continue;
        }

        const int32_t parent_index = nodes[node_index].Parent;
        if (parent_index < 0 || static_cast<size_t>(parent_index) >= node_entities.size()) {
            continue;
        }

        Entity parent_entity = node_entities[static_cast<size_t>(parent_index)];
        if (parent_entity) {
            entity_manager.setParent(node_entity, parent_entity, false);
        }
    }

    setSelectedEntity(root);
    return root;
}

Entity LunaEditorLayer::createEntityFromMeshAsset(AssetHandle mesh_handle, Entity parent)
{
    if (!mesh_handle.isValid() || !AssetDatabase::exists(mesh_handle)) {
        return {};
    }

    const auto& metadata = AssetDatabase::getAssetMetadata(mesh_handle);
    if (metadata.Type != AssetType::Mesh) {
        return {};
    }

    const std::string entity_name =
        !metadata.Name.empty() ? metadata.Name
                               : (!metadata.FilePath.empty() ? metadata.FilePath.stem().string() : "Mesh Entity");

    Entity entity = parent ? m_scene.entityManager().createChildEntity(parent, entity_name)
                           : m_scene.entityManager().createEntity(entity_name);
    if (!entity) {
        return {};
    }

    applyMeshAssetToEntity(entity, mesh_handle);
    setSelectedEntity(entity);
    return entity;
}

Entity LunaEditorLayer::createPrimitiveEntity(AssetHandle mesh_handle, Entity parent)
{
    if (!BuiltinAssets::isBuiltinMesh(mesh_handle)) {
        return {};
    }

    return createEntityFromMeshAsset(mesh_handle, parent);
}

Entity LunaEditorLayer::createCameraEntity(Entity parent)
{
    auto& entity_manager = m_scene.entityManager();
    Entity entity = parent ? entity_manager.createChildEntity(parent, "Camera") : entity_manager.createEntity("Camera");
    if (!entity) {
        return {};
    }

    entity.addComponent<CameraComponent>();
    auto& transform = entity.transform();
    transform.translation = {0.0f, 1.0f, 6.0f};
    transform.rotation = {0.0f, 0.0f, 0.0f};
    setSelectedEntity(entity);
    return entity;
}

Entity LunaEditorLayer::createDirectionalLightEntity(Entity parent)
{
    auto& entity_manager = m_scene.entityManager();
    Entity entity = parent ? entity_manager.createChildEntity(parent, "Directional Light")
                           : entity_manager.createEntity("Directional Light");
    if (!entity) {
        return {};
    }

    auto& light = entity.addComponent<LightComponent>();
    light.type = LightComponent::Type::Directional;
    light.enabled = true;
    light.color = {1.0f, 0.98f, 0.95f};
    light.intensity = 4.0f;

    auto& transform = entity.transform();
    transform.rotation = glm::radians(glm::vec3{-45.0f, 35.0f, 0.0f});
    setSelectedEntity(entity);
    return entity;
}

Entity LunaEditorLayer::createPointLightEntity(Entity parent)
{
    auto& entity_manager = m_scene.entityManager();
    Entity entity = parent ? entity_manager.createChildEntity(parent, "Point Light")
                           : entity_manager.createEntity("Point Light");
    if (!entity) {
        return {};
    }

    auto& light = entity.addComponent<LightComponent>();
    light.type = LightComponent::Type::Point;
    light.enabled = true;
    light.color = {1.0f, 1.0f, 1.0f};
    light.intensity = 20.0f;
    light.range = 10.0f;

    auto& transform = entity.transform();
    transform.translation = {0.0f, 2.0f, 0.0f};
    setSelectedEntity(entity);
    return entity;
}

Entity LunaEditorLayer::createSpotLightEntity(Entity parent)
{
    auto& entity_manager = m_scene.entityManager();
    Entity entity = parent ? entity_manager.createChildEntity(parent, "Spot Light")
                           : entity_manager.createEntity("Spot Light");
    if (!entity) {
        return {};
    }

    auto& light = entity.addComponent<LightComponent>();
    light.type = LightComponent::Type::Spot;
    light.enabled = true;
    light.color = {1.0f, 0.96f, 0.86f};
    light.intensity = 40.0f;
    light.range = 15.0f;
    light.innerConeAngleRadians = glm::radians(20.0f);
    light.outerConeAngleRadians = glm::radians(35.0f);

    auto& transform = entity.transform();
    transform.translation = {0.0f, 3.0f, 3.0f};
    transform.rotation = glm::radians(glm::vec3{-35.0f, 0.0f, 0.0f});
    setSelectedEntity(entity);
    return entity;
}

void LunaEditorLayer::applyMeshAssetToEntity(Entity entity, AssetHandle mesh_handle)
{
    if (!entity || !mesh_handle.isValid() || !AssetDatabase::exists(mesh_handle)) {
        return;
    }

    const auto& metadata = AssetDatabase::getAssetMetadata(mesh_handle);
    if (metadata.Type != AssetType::Mesh) {
        return;
    }

    if (!entity.hasComponent<MeshComponent>()) {
        entity.addComponent<MeshComponent>();
    }

    auto& mesh_component = entity.getComponent<MeshComponent>();
    const bool changed_mesh = mesh_component.meshHandle != mesh_handle;
    mesh_component.meshHandle = mesh_handle;
    if (changed_mesh) {
        mesh_component.clearAllSubmeshMaterials();
    }

    const auto mesh = AssetManager::get().requestAssetAs<Mesh>(mesh_handle);
    if (mesh && mesh->isValid()) {
        mesh_component.resizeSubmeshMaterials(mesh->getSubMeshes().size());
        for (uint32_t submesh_index = 0; submesh_index < mesh_component.getSubmeshMaterialCount(); ++submesh_index) {
            if (!mesh_component.getSubmeshMaterial(submesh_index).isValid()) {
                mesh_component.setSubmeshMaterial(submesh_index, BuiltinMaterials::DefaultLit);
            }
        }
    }
}

void LunaEditorLayer::openBuiltinMaterialsPanel(AssetHandle material_handle)
{
    if (material_handle.isValid()) {
        m_builtin_materials_panel.focusMaterial(material_handle);
    }
    m_show_builtin_materials_panel = true;
}

void LunaEditorLayer::resetEditorState()
{
    m_scene.entityManager().clear();
    m_scene.setName("Untitled");
    m_selected_entity = {};
    m_scene_file_path.clear();
    m_asset_label = "No scene loaded";
    m_show_pick_debug_visualization = false;
    syncPickDebugVisualizationState();
}

void LunaEditorLayer::createScene()
{
    resetEditorState();
    createCameraEntity();
    createDirectionalLightEntity();
    updateSceneLabel();
    LUNA_EDITOR_INFO("Created a new scene with a primary camera and directional light");
}

bool LunaEditorLayer::syncProjectAssets()
{
    if (!hasProjectLoaded()) {
        LUNA_EDITOR_WARN("Cannot sync assets because no project is currently loaded");
        return false;
    }

    const ImporterManager::ImportStats stats = ImporterManager::syncProjectAssets();
    logEditorAssetSyncStats(stats);
    m_content_browser_panel.requestRefresh();
    return stats.failedAssets == 0 && stats.missingMetadataAfterSync == 0;
}

bool LunaEditorLayer::openProject(const std::filesystem::path& project_file_path)
{
    if (project_file_path.empty()) {
        return false;
    }

    if (!ProjectManager::instance().loadProject(project_file_path)) {
        LUNA_EDITOR_WARN("Failed to load project '{}'", project_file_path.string());
        return false;
    }

    resetEditorState();

    AssetManager::get().clear();
    AssetDatabase::clear();
    syncProjectAssets();
    AssetManager::get().init();
    BuiltinMaterialOverrides::load();

    createScene();

    const auto project_root = ProjectManager::instance().getProjectRootPath();
    const auto project_info = ProjectManager::instance().getProjectInfo();
    if (project_root && project_info && !project_info->StartScene.empty()) {
        const std::filesystem::path start_scene_path =
            SceneSerializer::normalizeScenePath((*project_root / project_info->StartScene).lexically_normal());
        if (std::filesystem::exists(start_scene_path)) {
            if (!openScene(start_scene_path, false)) {
                createScene();
                m_scene_file_path = start_scene_path;
                updateSceneLabel();
            }
        } else {
            m_scene_file_path = start_scene_path;
            updateSceneLabel();
            LUNA_EDITOR_WARN("Configured StartScene '{}' does not exist. Saving will create it at that location.",
                             start_scene_path.string());
        }
    } else {
        updateSceneLabel();
        LUNA_EDITOR_INFO("Project '{}' does not define a StartScene. Using an empty scene.",
                         project_file_path.string());
    }

    LUNA_EDITOR_INFO("Loaded project '{}' with {} scene entities",
                     project_file_path.string(),
                     m_scene.entityManager().entityCount());
    m_content_browser_panel.requestRefresh();
    return true;
}

bool LunaEditorLayer::openScene()
{
    const std::filesystem::path scene_file_path =
        FileDialogs::openFile(kSceneFileFilter, sceneDialogDefaultPath().string());
    if (scene_file_path.empty()) {
        return false;
    }

    return openScene(scene_file_path, true);
}

bool LunaEditorLayer::openScene(const std::filesystem::path& scene_file_path, bool update_project_start_scene)
{
    const std::filesystem::path normalized_scene_path = SceneSerializer::normalizeScenePath(scene_file_path);
    if (normalized_scene_path.empty()) {
        return false;
    }

    if (!SceneSerializer::deserialize(m_scene, normalized_scene_path)) {
        LUNA_EDITOR_WARN("Failed to open scene '{}'", normalized_scene_path.string());
        return false;
    }

    m_scene_file_path = normalized_scene_path;
    m_selected_entity = {};
    updateSceneLabel();

    if (update_project_start_scene) {
        syncProjectStartScene(normalized_scene_path);
    }

    LUNA_EDITOR_INFO(
        "Opened scene '{}' with {} entities", normalized_scene_path.string(), m_scene.entityManager().entityCount());
    return true;
}

bool LunaEditorLayer::saveScene()
{
    if (m_scene_file_path.empty()) {
        return saveSceneAs();
    }

    return saveSceneAs(m_scene_file_path);
}

bool LunaEditorLayer::saveSceneAs()
{
    const std::filesystem::path scene_file_path =
        FileDialogs::saveFile(kSceneFileFilter, sceneDialogDefaultPath().string());
    if (scene_file_path.empty()) {
        return false;
    }

    return saveSceneAs(scene_file_path);
}

bool LunaEditorLayer::saveSceneAs(const std::filesystem::path& scene_file_path)
{
    const std::filesystem::path normalized_scene_path = SceneSerializer::normalizeScenePath(scene_file_path);
    if (normalized_scene_path.empty()) {
        return false;
    }

    if (m_scene.getName().empty() || m_scene.getName() == "Untitled") {
        m_scene.setName(normalized_scene_path.stem().string());
    }

    if (!SceneSerializer::serialize(m_scene, normalized_scene_path)) {
        LUNA_EDITOR_WARN("Failed to save scene '{}'", normalized_scene_path.string());
        return false;
    }

    m_scene_file_path = normalized_scene_path;
    updateSceneLabel();
    syncProjectStartScene(normalized_scene_path);
    m_content_browser_panel.requestRefresh();

    LUNA_EDITOR_INFO("Saved scene '{}' to '{}'", m_scene.getName(), normalized_scene_path.string());
    return true;
}

std::filesystem::path LunaEditorLayer::sceneDialogDefaultPath() const
{
    if (!m_scene_file_path.empty()) {
        const std::filesystem::path parent_path = m_scene_file_path.parent_path();
        if (!parent_path.empty() && std::filesystem::exists(parent_path)) {
            return parent_path;
        }
    }

    const auto project_root = ProjectManager::instance().getProjectRootPath();
    const auto project_info = ProjectManager::instance().getProjectInfo();
    if (project_root && project_info) {
        const std::filesystem::path scenes_directory =
            (*project_root / project_info->AssetsPath / "Scenes").lexically_normal();
        if (std::filesystem::exists(scenes_directory)) {
            return scenes_directory;
        }

        const std::filesystem::path assets_directory = (*project_root / project_info->AssetsPath).lexically_normal();
        if (std::filesystem::exists(assets_directory)) {
            return assets_directory;
        }

        return *project_root;
    }

    return projectDialogDefaultPath();
}

void LunaEditorLayer::updateSceneLabel()
{
    if (!m_scene_file_path.empty()) {
        if (const auto relative_path = makeScenePathRelativeToProject(m_scene_file_path)) {
            m_asset_label = relative_path->generic_string();
            return;
        }

        m_asset_label = m_scene_file_path.lexically_normal().string();
        return;
    }

    const std::string scene_name = m_scene.getName().empty() ? "Untitled" : m_scene.getName();
    m_asset_label = scene_name + SceneSerializer::FileExtension + std::string(" (unsaved)");
}

void LunaEditorLayer::syncProjectStartScene(const std::filesystem::path& scene_file_path)
{
    const auto relative_scene_path = makeScenePathRelativeToProject(scene_file_path);
    if (!relative_scene_path) {
        LUNA_EDITOR_WARN("Scene '{}' is outside the current project root. StartScene was not updated.",
                         scene_file_path.string());
        return;
    }

    const auto project_info = ProjectManager::instance().getProjectInfo();
    if (!project_info) {
        return;
    }

    if (project_info->StartScene.lexically_normal() == relative_scene_path->lexically_normal()) {
        return;
    }

    ProjectInfo updated_project_info = *project_info;
    updated_project_info.StartScene = *relative_scene_path;
    ProjectManager::instance().setProjectInfo(updated_project_info);

    if (ProjectManager::instance().saveProject()) {
        LUNA_EDITOR_INFO("Updated project StartScene to '{}'", relative_scene_path->generic_string());
    } else {
        LUNA_EDITOR_WARN("Failed to persist updated StartScene '{}'", relative_scene_path->generic_string());
    }
}

bool LunaEditorLayer::hasProjectLoaded() const
{
    return ProjectManager::instance().getProjectRootPath().has_value() &&
           ProjectManager::instance().getProjectInfo().has_value();
}

} // namespace luna

#include "Authoring/EditorAuthoringController.h"

#include "Project/ProjectManager.h"
#include "Scene/SceneSerializer.h"

#include <system_error>
#include <utility>
#include <vector>

namespace luna {

EditorAuthoringController::EditorAuthoringController()
{
    m_runtime.scene().setAssetLoadBehavior(Scene::AssetLoadBehavior::NonBlocking);
    m_document_context.bindScene(&m_runtime.scene());
    m_document_context.setRunning(false);
}

EditorDocumentContext& EditorAuthoringController::documentContext() noexcept
{
    return m_document_context;
}

const EditorDocumentContext& EditorAuthoringController::documentContext() const noexcept
{
    return m_document_context;
}

Scene& EditorAuthoringController::scene() noexcept
{
    return m_runtime.scene();
}

const Scene& EditorAuthoringController::scene() const noexcept
{
    return m_runtime.scene();
}

Scene& EditorAuthoringController::documentScene() noexcept
{
    Scene* scene = m_document_context.scene();
    return scene != nullptr ? *scene : m_runtime.scene();
}

const std::string& EditorAuthoringController::assetLabel() const noexcept
{
    return m_asset_label;
}

void EditorAuthoringController::reset()
{
    m_runtime.resetScene();
    m_document_context.bindScene(&m_runtime.scene());
    m_document_context.setRunning(false);
    m_asset_label = "No scene loaded";
}

authoring::SceneBootstrapResult EditorAuthoringController::createScene()
{
    m_runtime.clearSelection();
    const authoring::SceneBootstrapResult result = m_runtime.createScene();
    processEvents();
    return result;
}

bool EditorAuthoringController::openScene(const std::filesystem::path& scene_file_path)
{
    if (!m_runtime.openScene(scene_file_path)) {
        return false;
    }

    processEvents();
    return true;
}

bool EditorAuthoringController::saveSceneAs(const std::filesystem::path& scene_file_path)
{
    if (!m_runtime.saveSceneAs(scene_file_path)) {
        return false;
    }

    processEvents();
    return true;
}

void EditorAuthoringController::setSceneFilePath(std::filesystem::path scene_file_path)
{
    m_runtime.setSceneFilePath(std::move(scene_file_path));
    updateAssetLabel();
}

const std::filesystem::path& EditorAuthoringController::sceneFilePath() const noexcept
{
    return m_runtime.sceneFilePath();
}

UUID EditorAuthoringController::selectedEntityId() const noexcept
{
    return m_runtime.selectedEntityId();
}

Entity EditorAuthoringController::selectedEntity(Scene& inspection_scene) const
{
    return m_runtime.selectedEntity(inspection_scene);
}

void EditorAuthoringController::setSelectedEntity(Entity entity) noexcept
{
    m_runtime.setSelectedEntity(entity);
}

void EditorAuthoringController::setSelectedEntityId(UUID entity_id) noexcept
{
    m_runtime.setSelectedEntityId(entity_id);
}

void EditorAuthoringController::clearSelection() noexcept
{
    m_runtime.clearSelection();
}

void EditorAuthoringController::markSceneDirty()
{
    m_runtime.markSceneDirty();
    processEvents();
}

bool EditorAuthoringController::isSceneDirty() const noexcept
{
    return m_runtime.isSceneDirty();
}

bool EditorAuthoringController::hasOpenTransaction() const noexcept
{
    return m_runtime.hasOpenTransaction();
}

bool EditorAuthoringController::beginTransaction(std::string name)
{
    return m_runtime.beginTransaction(std::move(name));
}

bool EditorAuthoringController::commitTransaction()
{
    if (!m_runtime.hasOpenTransaction()) {
        return false;
    }

    const bool committed = m_runtime.commitTransaction();
    processEvents();
    return committed;
}

bool EditorAuthoringController::rollbackTransaction()
{
    if (!m_runtime.hasOpenTransaction()) {
        return false;
    }

    const bool rolled_back = m_runtime.rollbackTransaction();
    if (rolled_back) {
        processEvents();
    }
    return rolled_back;
}

bool EditorAuthoringController::undo()
{
    if (!m_runtime.undo()) {
        return false;
    }

    processEvents();
    return true;
}

bool EditorAuthoringController::redo()
{
    if (!m_runtime.redo()) {
        return false;
    }

    processEvents();
    return true;
}

bool EditorAuthoringController::canUndo() const noexcept
{
    return m_runtime.canUndo();
}

bool EditorAuthoringController::canRedo() const noexcept
{
    return m_runtime.canRedo();
}

Entity EditorAuthoringController::createEntity(const std::string& name, Entity parent)
{
    Entity entity = m_runtime.createEntity(name, parent);
    if (entity) {
        processEvents();
    }
    return entity;
}

Entity EditorAuthoringController::createEntityFromModelAsset(AssetHandle model_handle, Entity parent)
{
    Entity entity = m_runtime.createEntityFromModelAsset(model_handle, parent);
    if (entity) {
        processEvents();
    }
    return entity;
}

Entity EditorAuthoringController::createEntityFromMeshAsset(AssetHandle mesh_handle, Entity parent)
{
    Entity entity = m_runtime.createEntityFromMeshAsset(mesh_handle, parent);
    if (entity) {
        processEvents();
    }
    return entity;
}

Entity EditorAuthoringController::createPrimitiveEntity(AssetHandle mesh_handle, Entity parent)
{
    Entity entity = m_runtime.createPrimitiveEntity(mesh_handle, parent);
    if (entity) {
        processEvents();
    }
    return entity;
}

Entity EditorAuthoringController::createCameraEntity(Entity parent)
{
    Entity entity = m_runtime.createCameraEntity(parent);
    if (entity) {
        processEvents();
    }
    return entity;
}

Entity EditorAuthoringController::createDirectionalLightEntity(Entity parent)
{
    Entity entity = m_runtime.createDirectionalLightEntity(parent);
    if (entity) {
        processEvents();
    }
    return entity;
}

Entity EditorAuthoringController::createPointLightEntity(Entity parent)
{
    Entity entity = m_runtime.createPointLightEntity(parent);
    if (entity) {
        processEvents();
    }
    return entity;
}

Entity EditorAuthoringController::createSpotLightEntity(Entity parent)
{
    Entity entity = m_runtime.createSpotLightEntity(parent);
    if (entity) {
        processEvents();
    }
    return entity;
}

bool EditorAuthoringController::destroyEntity(Entity entity)
{
    const bool changed = m_runtime.destroyEntity(entity);
    if (changed) {
        processEvents();
    }
    return changed;
}

bool EditorAuthoringController::reparentEntity(Entity entity, Entity parent, bool preserve_world_transform)
{
    const bool changed = m_runtime.reparentEntity(entity, parent, preserve_world_transform);
    if (changed) {
        processEvents();
    }
    return changed;
}

bool EditorAuthoringController::addComponent(Entity entity, authoring::AuthoringComponentKind component_kind)
{
    const bool changed = m_runtime.addComponent(entity, component_kind);
    if (changed) {
        processEvents();
    }
    return changed;
}

bool EditorAuthoringController::removeComponent(Entity entity, authoring::AuthoringComponentKind component_kind)
{
    const bool changed = m_runtime.removeComponent(entity, component_kind);
    if (changed) {
        processEvents();
    }
    return changed;
}

bool EditorAuthoringController::applyMeshAssetToEntity(Entity entity, AssetHandle mesh_handle)
{
    const bool changed = m_runtime.applyMeshAssetToEntity(entity, mesh_handle);
    if (changed) {
        processEvents();
    }
    return changed;
}

bool EditorAuthoringController::setEntityName(Entity entity, std::string name)
{
    const bool changed = m_runtime.setEntityName(entity, std::move(name));
    if (changed) {
        processEvents();
    }
    return changed;
}

bool EditorAuthoringController::setEntityTransform(Entity entity, const TransformComponent& transform)
{
    const bool defer_events = m_runtime.hasOpenTransaction();
    const bool changed = m_runtime.setEntityTransform(entity, transform);
    if (changed && !defer_events) {
        processEvents();
    }
    return changed;
}

bool EditorAuthoringController::setCameraComponent(Entity entity, const CameraComponent& camera_component)
{
    const bool changed = m_runtime.setCameraComponent(entity, camera_component);
    if (changed) {
        processEvents();
    }
    return changed;
}

bool EditorAuthoringController::setLightComponent(Entity entity, const LightComponent& light_component)
{
    const bool changed = m_runtime.setLightComponent(entity, light_component);
    if (changed) {
        processEvents();
    }
    return changed;
}

bool EditorAuthoringController::setMeshComponent(Entity entity, const MeshComponent& mesh_component)
{
    const bool changed = m_runtime.setMeshComponent(entity, mesh_component);
    if (changed) {
        processEvents();
    }
    return changed;
}

bool EditorAuthoringController::setScriptComponent(Entity entity, const ScriptComponent& script_component)
{
    const bool changed = m_runtime.setScriptComponent(entity, script_component);
    if (changed) {
        processEvents();
    }
    return changed;
}

bool EditorAuthoringController::setSceneEnvironmentSettings(const SceneEnvironmentSettings& settings)
{
    const bool changed = m_runtime.setSceneEnvironmentSettings(settings);
    if (changed) {
        processEvents();
    }
    return changed;
}

bool EditorAuthoringController::setSceneShadowSettings(const SceneShadowSettings& settings)
{
    const bool changed = m_runtime.setSceneShadowSettings(settings);
    if (changed) {
        processEvents();
    }
    return changed;
}

void EditorAuthoringController::processEvents()
{
    const std::vector<authoring::AuthoringEvent> events = m_runtime.consumeAuthoringEvents();
    if (events.empty()) {
        return;
    }

    bool update_scene_label = false;
    bool validate_selection = false;

    for (const auto& event : events) {
        switch (event.type) {
            case authoring::AuthoringEventType::SceneReset:
            case authoring::AuthoringEventType::SceneCreated:
            case authoring::AuthoringEventType::SceneLoaded:
            case authoring::AuthoringEventType::HistoryChanged:
                update_scene_label = true;
                validate_selection = true;
                break;
            case authoring::AuthoringEventType::SceneSaved:
            case authoring::AuthoringEventType::SceneDirtyChanged:
            case authoring::AuthoringEventType::EntityCreated:
            case authoring::AuthoringEventType::EntityModified:
            case authoring::AuthoringEventType::EntityDestroyed:
            case authoring::AuthoringEventType::EntityReparented:
            case authoring::AuthoringEventType::ComponentAdded:
            case authoring::AuthoringEventType::ComponentRemoved:
                update_scene_label = true;
                validate_selection = true;
                break;
            case authoring::AuthoringEventType::SceneSettingsChanged:
                update_scene_label = true;
                break;
        }
    }

    if (validate_selection) {
        m_runtime.validateSelection();
    }

    if (update_scene_label) {
        updateAssetLabel();
    }
}

void EditorAuthoringController::updateAssetLabel()
{
    m_asset_label = makeAssetLabel(m_runtime);
}

std::filesystem::path EditorAuthoringController::relativeScenePathToProject(
    const std::filesystem::path& scene_file_path)
{
    const auto project_root = ProjectManager::instance().getProjectRootPath();
    if (!project_root || scene_file_path.empty()) {
        return {};
    }

    std::error_code ec;
    std::filesystem::path relative_path = std::filesystem::relative(scene_file_path, *project_root, ec);
    if (ec) {
        return {};
    }

    relative_path = relative_path.lexically_normal();
    if (relative_path.empty() || relative_path.is_absolute()) {
        return {};
    }

    const std::string relative_string = relative_path.generic_string();
    if (relative_string == "." || relative_string.starts_with("..")) {
        return {};
    }

    return relative_path;
}

std::string EditorAuthoringController::makeAssetLabel(const EditorRuntime& runtime)
{
    const char* dirty_suffix = runtime.isSceneDirty() ? " *" : "";
    if (!runtime.sceneFilePath().empty()) {
        const std::filesystem::path relative_path = relativeScenePathToProject(runtime.sceneFilePath());
        if (!relative_path.empty()) {
            return relative_path.generic_string() + dirty_suffix;
        }

        return runtime.sceneFilePath().lexically_normal().string() + dirty_suffix;
    }

    const std::string scene_name = runtime.scene().getName().empty() ? "Untitled" : runtime.scene().getName();
    return scene_name + SceneSerializer::FileExtension + std::string(" (unsaved)");
}

} // namespace luna

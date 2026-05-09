#include "EditorRuntime.h"

#include <utility>

namespace luna {

EditorRuntime::EditorRuntime()
    : m_scene(std::make_unique<Scene>()),
      m_authoring_host(m_authoring_session)
{
    m_authoring_session.bindScene(*m_scene);
}

Scene& EditorRuntime::scene() noexcept
{
    return *m_scene;
}

const Scene& EditorRuntime::scene() const noexcept
{
    return *m_scene;
}

authoring::AuthoringSession& EditorRuntime::authoringSession() noexcept
{
    return m_authoring_session;
}

const authoring::AuthoringSession& EditorRuntime::authoringSession() const noexcept
{
    return m_authoring_session;
}

UUID EditorRuntime::selectedEntityId() const noexcept
{
    return m_selected_entity_id;
}

Entity EditorRuntime::selectedEntity()
{
    return selectedEntity(scene());
}

Entity EditorRuntime::selectedEntity(Scene& inspection_scene) const
{
    if (!m_selected_entity_id.isValid()) {
        return {};
    }

    return inspection_scene.entityManager().findEntityByUUID(m_selected_entity_id);
}

void EditorRuntime::setSelectedEntity(Entity entity) noexcept
{
    m_selected_entity_id = entity ? entity.getUUID() : UUID(0);
}

void EditorRuntime::setSelectedEntityId(UUID entity_id) noexcept
{
    m_selected_entity_id = entity_id;
}

void EditorRuntime::clearSelection() noexcept
{
    m_selected_entity_id = UUID(0);
}

void EditorRuntime::validateSelection()
{
    if (m_selected_entity_id.isValid() && !scene().entityManager().containsEntity(m_selected_entity_id)) {
        clearSelection();
    }
}

bool EditorRuntime::isSceneDirty() const noexcept
{
    return m_authoring_session.isSceneDirty();
}

void EditorRuntime::markSceneDirty()
{
    m_authoring_session.markSceneDirty();
}

void EditorRuntime::clearSceneDirty()
{
    m_authoring_session.clearSceneDirty();
}

void EditorRuntime::resetScene()
{
    m_authoring_session.resetScene();
    clearSelection();
}

authoring::SceneBootstrapResult EditorRuntime::createScene()
{
    clearSelection();
    return m_authoring_session.createScene();
}

void EditorRuntime::setSceneFilePath(std::filesystem::path scene_file_path)
{
    m_authoring_session.setSceneFilePath(std::move(scene_file_path));
}

const std::filesystem::path& EditorRuntime::sceneFilePath() const noexcept
{
    return m_authoring_session.sceneFilePath();
}

bool EditorRuntime::openScene(const std::filesystem::path& scene_file_path)
{
    const bool opened = m_authoring_session.openScene(scene_file_path);
    if (opened) {
        clearSelection();
    }
    return opened;
}

bool EditorRuntime::saveScene()
{
    return m_authoring_session.saveScene();
}

bool EditorRuntime::saveSceneAs(const std::filesystem::path& scene_file_path)
{
    return m_authoring_session.saveSceneAs(scene_file_path);
}

bool EditorRuntime::beginTransaction(std::string name)
{
    return m_authoring_session.beginTransaction(std::move(name));
}

bool EditorRuntime::commitTransaction()
{
    return m_authoring_session.commitTransaction();
}

bool EditorRuntime::rollbackTransaction()
{
    return m_authoring_session.rollbackTransaction();
}

bool EditorRuntime::hasOpenTransaction() const noexcept
{
    return m_authoring_session.hasOpenTransaction();
}

bool EditorRuntime::undo()
{
    const bool changed = m_authoring_host.undo();
    if (changed) {
        validateSelection();
    }
    return changed;
}

bool EditorRuntime::redo()
{
    const bool changed = m_authoring_host.redo();
    if (changed) {
        validateSelection();
    }
    return changed;
}

bool EditorRuntime::canUndo() const noexcept
{
    return m_authoring_host.canUndo();
}

bool EditorRuntime::canRedo() const noexcept
{
    return m_authoring_host.canRedo();
}

Entity EditorRuntime::createEntity(const std::string& name, Entity parent)
{
    Entity entity = m_authoring_session.createEntity(name, parent);
    if (entity) {
        setSelectedEntity(entity);
    }
    return entity;
}

Entity EditorRuntime::createEntityFromModelAsset(AssetHandle model_handle, Entity parent)
{
    Entity entity = m_authoring_session.createEntityFromModelAsset(model_handle, parent);
    if (entity) {
        setSelectedEntity(entity);
    }
    return entity;
}

Entity EditorRuntime::createEntityFromMeshAsset(AssetHandle mesh_handle, Entity parent)
{
    Entity entity = m_authoring_session.createEntityFromMeshAsset(mesh_handle, parent);
    if (entity) {
        setSelectedEntity(entity);
    }
    return entity;
}

Entity EditorRuntime::createPrimitiveEntity(AssetHandle mesh_handle, Entity parent)
{
    Entity entity = m_authoring_session.createPrimitiveEntity(mesh_handle, parent);
    if (entity) {
        setSelectedEntity(entity);
    }
    return entity;
}

Entity EditorRuntime::createCameraEntity(Entity parent)
{
    Entity entity = m_authoring_session.createCameraEntity(parent);
    if (entity) {
        setSelectedEntity(entity);
    }
    return entity;
}

Entity EditorRuntime::createDirectionalLightEntity(Entity parent)
{
    Entity entity = m_authoring_session.createDirectionalLightEntity(parent);
    if (entity) {
        setSelectedEntity(entity);
    }
    return entity;
}

Entity EditorRuntime::createPointLightEntity(Entity parent)
{
    Entity entity = m_authoring_session.createPointLightEntity(parent);
    if (entity) {
        setSelectedEntity(entity);
    }
    return entity;
}

Entity EditorRuntime::createSpotLightEntity(Entity parent)
{
    Entity entity = m_authoring_session.createSpotLightEntity(parent);
    if (entity) {
        setSelectedEntity(entity);
    }
    return entity;
}

bool EditorRuntime::destroyEntity(Entity entity)
{
    const bool changed = m_authoring_session.destroyEntity(entity);
    if (changed) {
        validateSelection();
    }
    return changed;
}

bool EditorRuntime::reparentEntity(Entity entity, Entity parent, bool preserve_world_transform)
{
    return m_authoring_session.reparentEntity(entity, parent, preserve_world_transform);
}

bool EditorRuntime::addComponent(Entity entity, authoring::AuthoringComponentKind component_kind)
{
    return m_authoring_session.addComponent(entity, component_kind);
}

bool EditorRuntime::removeComponent(Entity entity, authoring::AuthoringComponentKind component_kind)
{
    return m_authoring_session.removeComponent(entity, component_kind);
}

bool EditorRuntime::applyMeshAssetToEntity(Entity entity, AssetHandle mesh_handle)
{
    return m_authoring_session.applyMeshAssetToEntity(entity, mesh_handle);
}

bool EditorRuntime::setEntityName(Entity entity, std::string name)
{
    return m_authoring_session.setEntityName(entity, std::move(name));
}

bool EditorRuntime::setEntityTransform(Entity entity, const TransformComponent& transform)
{
    return m_authoring_session.setEntityTransform(entity, transform);
}

bool EditorRuntime::setCameraComponent(Entity entity, const CameraComponent& camera_component)
{
    return m_authoring_session.setCameraComponent(entity, camera_component);
}

bool EditorRuntime::setLightComponent(Entity entity, const LightComponent& light_component)
{
    return m_authoring_session.setLightComponent(entity, light_component);
}

bool EditorRuntime::setMeshComponent(Entity entity, const MeshComponent& mesh_component)
{
    return m_authoring_session.setMeshComponent(entity, mesh_component);
}

bool EditorRuntime::setScriptComponent(Entity entity, const ScriptComponent& script_component)
{
    return m_authoring_session.setScriptComponent(entity, script_component);
}

bool EditorRuntime::setSceneEnvironmentSettings(const SceneEnvironmentSettings& settings)
{
    return m_authoring_session.setSceneEnvironmentSettings(settings);
}

bool EditorRuntime::setSceneShadowSettings(const SceneShadowSettings& settings)
{
    return m_authoring_session.setSceneShadowSettings(settings);
}

bool EditorRuntime::executeAuthoringPlan(const authoring::AuthoringPlan& plan, authoring::AuthoringReport& report)
{
    const bool ok = m_authoring_host.executePlan(plan, report);
    validateSelection();
    return ok;
}

authoring::AuthoringSceneSnapshot EditorRuntime::captureSceneSnapshot() const
{
    return m_authoring_host.captureSceneSnapshot();
}

std::vector<authoring::AuthoringEvent> EditorRuntime::consumeAuthoringEvents()
{
    return m_authoring_host.consumeEvents();
}

} // namespace luna

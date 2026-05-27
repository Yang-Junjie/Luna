#include "Asset/AssetDatabase.h"
#include "Asset/AssetManager.h"
#include "Asset/BuiltinAssets.h"
#include "Asset/Model.h"
#include "AuthoringSession.h"
#include "Core/Log.h"
#include "Renderer/Mesh.h"
#include "Scene/Components.h"
#include "Scene/SceneSerializer.h"

#include <cstddef>
#include <cstdint>

#include <algorithm>
#include <glm/trigonometric.hpp>
#include <optional>
#include <string>
#include <utility>

namespace {

void configureCameraEntity(luna::Entity entity)
{
    entity.addComponent<luna::CameraComponent>();
    auto& transform = entity.transform();
    transform.translation = {0.0f, 1.0f, 6.0f};
    transform.rotation = {0.0f, 0.0f, 0.0f};
}

void configureDirectionalLightEntity(luna::Entity entity)
{
    auto& light = entity.addComponent<luna::LightComponent>();
    light.type = luna::LightComponent::Type::Directional;
    light.enabled = true;
    light.color = {1.0f, 0.98f, 0.95f};
    light.intensity = 4.0f;

    auto& transform = entity.transform();
    transform.rotation = glm::radians(glm::vec3{-45.0f, 35.0f, 0.0f});
}

void configurePointLightEntity(luna::Entity entity)
{
    auto& light = entity.addComponent<luna::LightComponent>();
    light.type = luna::LightComponent::Type::Point;
    light.enabled = true;
    light.color = {1.0f, 1.0f, 1.0f};
    light.intensity = 20.0f;
    light.range = 10.0f;

    auto& transform = entity.transform();
    transform.translation = {0.0f, 2.0f, 0.0f};
}

void configureSpotLightEntity(luna::Entity entity)
{
    auto& light = entity.addComponent<luna::LightComponent>();
    light.type = luna::LightComponent::Type::Spot;
    light.enabled = true;
    light.color = {1.0f, 0.96f, 0.86f};
    light.intensity = 40.0f;
    light.range = 15.0f;
    light.innerConeAngleRadians = glm::radians(20.0f);
    light.outerConeAngleRadians = glm::radians(35.0f);

    auto& transform = entity.transform();
    transform.translation = {0.0f, 3.0f, 3.0f};
    transform.rotation = glm::radians(glm::vec3{-35.0f, 0.0f, 0.0f});
}

bool sameVec3(const glm::vec3& lhs, const glm::vec3& rhs)
{
    return lhs.x == rhs.x && lhs.y == rhs.y && lhs.z == rhs.z;
}

bool sameEnvironmentSettings(const luna::SceneEnvironmentSettings& lhs, const luna::SceneEnvironmentSettings& rhs)
{
    return lhs.backgroundMode == rhs.backgroundMode && sameVec3(lhs.backgroundColor, rhs.backgroundColor) &&
           lhs.enabled == rhs.enabled && lhs.iblEnabled == rhs.iblEnabled &&
           lhs.environmentMapHandle == rhs.environmentMapHandle && lhs.intensity == rhs.intensity &&
           lhs.skyIntensity == rhs.skyIntensity && lhs.diffuseIntensity == rhs.diffuseIntensity &&
           lhs.specularIntensity == rhs.specularIntensity &&
           sameVec3(lhs.proceduralSunDirection, rhs.proceduralSunDirection) &&
           lhs.proceduralSunIntensity == rhs.proceduralSunIntensity &&
           lhs.proceduralSunAngularRadius == rhs.proceduralSunAngularRadius &&
           sameVec3(lhs.proceduralSkyColorZenith, rhs.proceduralSkyColorZenith) &&
           sameVec3(lhs.proceduralSkyColorHorizon, rhs.proceduralSkyColorHorizon) &&
           sameVec3(lhs.proceduralGroundColor, rhs.proceduralGroundColor) &&
           lhs.proceduralSkyExposure == rhs.proceduralSkyExposure;
}

bool sameShadowSettings(const luna::SceneShadowSettings& lhs, const luna::SceneShadowSettings& rhs)
{
    return lhs.mode == rhs.mode && lhs.pcfShadowDistance == rhs.pcfShadowDistance && lhs.pcfMapSize == rhs.pcfMapSize &&
           lhs.csmCascadeSize == rhs.csmCascadeSize;
}

bool isBoundSceneEntity(luna::Scene& scene, luna::Entity entity)
{
    return entity && entity.getEntityManager() == &scene.entityManager();
}

bool sameTransformComponent(const luna::TransformComponent& lhs, const luna::TransformComponent& rhs)
{
    return sameVec3(lhs.translation, rhs.translation) && sameVec3(lhs.rotation, rhs.rotation) &&
           sameVec3(lhs.scale, rhs.scale);
}

bool sameCameraComponent(const luna::CameraComponent& lhs, const luna::CameraComponent& rhs)
{
    return lhs.primary == rhs.primary && lhs.fixedAspectRatio == rhs.fixedAspectRatio &&
           lhs.projectionType == rhs.projectionType &&
           lhs.perspectiveVerticalFovRadians == rhs.perspectiveVerticalFovRadians &&
           lhs.perspectiveNear == rhs.perspectiveNear && lhs.perspectiveFar == rhs.perspectiveFar &&
           lhs.orthographicSize == rhs.orthographicSize && lhs.orthographicNear == rhs.orthographicNear &&
           lhs.orthographicFar == rhs.orthographicFar;
}

bool sameLightComponent(const luna::LightComponent& lhs, const luna::LightComponent& rhs)
{
    return lhs.type == rhs.type && lhs.enabled == rhs.enabled && sameVec3(lhs.color, rhs.color) &&
           lhs.intensity == rhs.intensity && lhs.range == rhs.range &&
           lhs.innerConeAngleRadians == rhs.innerConeAngleRadians &&
           lhs.outerConeAngleRadians == rhs.outerConeAngleRadians;
}

bool sameMeshComponent(const luna::MeshComponent& lhs, const luna::MeshComponent& rhs)
{
    return lhs.meshHandle == rhs.meshHandle && lhs.firstSubmesh == rhs.firstSubmesh &&
           lhs.submeshCount == rhs.submeshCount && lhs.submeshMaterials == rhs.submeshMaterials;
}

bool sameScriptProperty(const luna::ScriptProperty& lhs, const luna::ScriptProperty& rhs)
{
    return lhs.name == rhs.name && lhs.type == rhs.type && lhs.boolValue == rhs.boolValue &&
           lhs.intValue == rhs.intValue && lhs.floatValue == rhs.floatValue && lhs.stringValue == rhs.stringValue &&
           sameVec3(lhs.vec3Value, rhs.vec3Value) && lhs.entityValue == rhs.entityValue &&
           lhs.assetValue == rhs.assetValue && lhs.metadata.displayName == rhs.metadata.displayName &&
           lhs.metadata.description == rhs.metadata.description && lhs.metadata.category == rhs.metadata.category &&
           lhs.metadata.hasMinValue == rhs.metadata.hasMinValue &&
           lhs.metadata.hasMaxValue == rhs.metadata.hasMaxValue &&
           lhs.metadata.hasStepValue == rhs.metadata.hasStepValue && lhs.metadata.minValue == rhs.metadata.minValue &&
           lhs.metadata.maxValue == rhs.metadata.maxValue && lhs.metadata.stepValue == rhs.metadata.stepValue &&
           lhs.metadata.assetType == rhs.metadata.assetType && lhs.metadata.entityFilter == rhs.metadata.entityFilter &&
           lhs.metadata.options.size() == rhs.metadata.options.size() &&
           std::equal(lhs.metadata.options.begin(),
                      lhs.metadata.options.end(),
                      rhs.metadata.options.begin(),
                      [](const luna::ScriptPropertyOption& left, const luna::ScriptPropertyOption& right) {
                          return left.label == right.label && left.intValue == right.intValue &&
                                 left.stringValue == right.stringValue;
                      });
}

bool sameScriptEntry(const luna::ScriptEntry& lhs, const luna::ScriptEntry& rhs)
{
    if (lhs.id != rhs.id || lhs.enabled != rhs.enabled || lhs.scriptAsset != rhs.scriptAsset ||
        lhs.typeName != rhs.typeName || lhs.executionOrder != rhs.executionOrder ||
        lhs.properties.size() != rhs.properties.size()) {
        return false;
    }

    for (size_t index = 0; index < lhs.properties.size(); ++index) {
        if (!sameScriptProperty(lhs.properties[index], rhs.properties[index])) {
            return false;
        }
    }

    return true;
}

bool sameScriptComponent(const luna::ScriptComponent& lhs, const luna::ScriptComponent& rhs)
{
    if (lhs.enabled != rhs.enabled || lhs.scripts.size() != rhs.scripts.size()) {
        return false;
    }

    for (size_t index = 0; index < lhs.scripts.size(); ++index) {
        if (!sameScriptEntry(lhs.scripts[index], rhs.scripts[index])) {
            return false;
        }
    }

    return true;
}

const char* componentKindName(luna::authoring::AuthoringComponentKind component_kind)
{
    switch (component_kind) {
        case luna::authoring::AuthoringComponentKind::Camera:
            return "Camera";
        case luna::authoring::AuthoringComponentKind::Light:
            return "Light";
        case luna::authoring::AuthoringComponentKind::Mesh:
            return "Mesh";
        case luna::authoring::AuthoringComponentKind::Script:
            return "Script";
    }

    return "Component";
}

} // namespace

namespace luna::authoring {

AuthoringSession::AuthoringSession(Scene& scene)
{
    bindScene(scene);
}

void AuthoringSession::bindScene(Scene& scene)
{
    m_scene = &scene;
    m_scene_file_path.clear();
    m_scene_dirty = false;
    m_events.clear();
    m_history.clear();
    m_implicit_history_suppression_depth = 0;
}

bool AuthoringSession::hasScene() const noexcept
{
    return m_scene != nullptr;
}

bool AuthoringSession::hasBoundScene() const noexcept
{
    return m_scene != nullptr;
}

Scene& AuthoringSession::scene()
{
    return *m_scene;
}

const Scene& AuthoringSession::scene() const
{
    return *m_scene;
}

void AuthoringSession::setSceneFilePath(std::filesystem::path scene_file_path)
{
    m_scene_file_path = std::move(scene_file_path);
}

const std::filesystem::path& AuthoringSession::sceneFilePath() const noexcept
{
    return m_scene_file_path;
}

bool AuthoringSession::isSceneDirty() const noexcept
{
    return m_scene_dirty;
}

void AuthoringSession::markSceneDirty()
{
    if (!hasBoundScene() || m_scene_dirty) {
        return;
    }

    m_scene_dirty = true;
    queueEvent(AuthoringEvent{
        .type = AuthoringEventType::SceneDirtyChanged,
        .message = "Scene marked dirty",
    });
}

void AuthoringSession::clearSceneDirty()
{
    if (!hasBoundScene() || !m_scene_dirty) {
        return;
    }

    m_scene_dirty = false;
    queueEvent(AuthoringEvent{
        .type = AuthoringEventType::SceneDirtyChanged,
        .message = "Scene marked clean",
    });
}

std::vector<AuthoringEvent> AuthoringSession::consumeEvents()
{
    std::vector<AuthoringEvent> events = std::move(m_events);
    m_events.clear();
    return events;
}

bool AuthoringSession::beginTransaction(std::string name)
{
    if (!hasBoundScene()) {
        return false;
    }

    return m_history.beginTransaction(std::move(name), scene(), m_scene_file_path, m_scene_dirty);
}

bool AuthoringSession::commitTransaction()
{
    if (!hasBoundScene()) {
        return false;
    }

    return m_history.commitTransaction(scene(), m_scene_file_path, m_scene_dirty);
}

bool AuthoringSession::rollbackTransaction()
{
    std::optional<AuthoringSceneState> state = m_history.rollbackTransaction();
    if (!state.has_value()) {
        return false;
    }

    restoreHistoryState(std::move(*state), "Transaction rolled back");
    return true;
}

bool AuthoringSession::hasOpenTransaction() const noexcept
{
    return m_history.hasOpenTransaction();
}

bool AuthoringSession::undo()
{
    std::optional<AuthoringSceneState> state = m_history.undo();
    if (!state.has_value()) {
        return false;
    }

    restoreHistoryState(std::move(*state), "Undo");
    return true;
}

bool AuthoringSession::redo()
{
    std::optional<AuthoringSceneState> state = m_history.redo();
    if (!state.has_value()) {
        return false;
    }

    restoreHistoryState(std::move(*state), "Redo");
    return true;
}

bool AuthoringSession::canUndo() const noexcept
{
    return m_history.canUndo();
}

bool AuthoringSession::canRedo() const noexcept
{
    return m_history.canRedo();
}

size_t AuthoringSession::undoDepth() const noexcept
{
    return m_history.undoDepth();
}

size_t AuthoringSession::redoDepth() const noexcept
{
    return m_history.redoDepth();
}

void AuthoringSession::clearHistory()
{
    m_history.clear();
}

void AuthoringSession::resetScene()
{
    if (!hasBoundScene()) {
        LUNA_CORE_WARN("Cannot reset scene because no scene is bound");
        return;
    }

    const bool keep_history = hasOpenTransaction();
    const bool was_dirty = m_scene_dirty;
    suppressImplicitHistory();
    scene().entityManager().clear();
    scene().setName("Untitled");
    scene().environmentSettings() = {};
    scene().shadowSettings() = {};
    m_scene_file_path.clear();
    m_scene_dirty = false;
    resumeImplicitHistory();
    if (was_dirty) {
        queueEvent(AuthoringEvent{
            .type = AuthoringEventType::SceneDirtyChanged,
            .message = "Scene marked clean",
        });
    }
    queueEvent(AuthoringEvent{
        .type = AuthoringEventType::SceneReset,
        .message = "Scene reset",
    });
    if (!keep_history) {
        clearHistory();
    }
}

SceneBootstrapResult AuthoringSession::createScene()
{
    SceneBootstrapResult result{};
    if (!hasBoundScene()) {
        LUNA_CORE_WARN("Cannot create a scene because no scene is bound");
        return result;
    }

    resetScene();
    suppressImplicitHistory();
    result.camera = createCameraEntity();
    result.directional_light = createDirectionalLightEntity();
    resumeImplicitHistory();
    clearSceneDirty();
    queueEvent(AuthoringEvent{
        .type = AuthoringEventType::SceneCreated,
        .message = "Scene created with a bootstrap camera and directional light",
    });
    return result;
}

bool AuthoringSession::openScene(const std::filesystem::path& scene_file_path)
{
    if (!hasBoundScene()) {
        LUNA_CORE_WARN("Cannot open scene because no scene is bound");
        return false;
    }

    const std::filesystem::path normalized_scene_path = SceneSerializer::normalizeScenePath(scene_file_path);
    if (normalized_scene_path.empty()) {
        LUNA_CORE_WARN("Cannot open scene because the target path is empty");
        return false;
    }

    if (!SceneSerializer::deserialize(scene(), normalized_scene_path)) {
        return false;
    }

    m_scene_file_path = normalized_scene_path;
    clearSceneDirty();
    queueEvent(AuthoringEvent{
        .type = AuthoringEventType::SceneLoaded,
        .path = normalized_scene_path,
        .message = "Scene loaded",
    });
    if (!hasOpenTransaction()) {
        clearHistory();
    }
    return true;
}

bool AuthoringSession::saveScene()
{
    if (m_scene_file_path.empty()) {
        LUNA_CORE_WARN("Cannot save scene because no scene file path is set");
        return false;
    }

    return saveSceneAs(m_scene_file_path);
}

bool AuthoringSession::saveSceneAs(const std::filesystem::path& scene_file_path)
{
    if (!hasBoundScene()) {
        LUNA_CORE_WARN("Cannot save scene because no scene is bound");
        return false;
    }

    const std::filesystem::path normalized_scene_path = SceneSerializer::normalizeScenePath(scene_file_path);
    if (normalized_scene_path.empty()) {
        LUNA_CORE_WARN("Cannot save scene because the target path is empty");
        return false;
    }

    if (scene().getName().empty() || scene().getName() == "Untitled") {
        scene().setName(normalized_scene_path.stem().string());
    }

    if (!SceneSerializer::serialize(scene(), normalized_scene_path)) {
        return false;
    }

    m_scene_file_path = normalized_scene_path;
    clearSceneDirty();
    m_history.markCurrentStateSaved(scene(), m_scene_file_path);
    queueEvent(AuthoringEvent{
        .type = AuthoringEventType::SceneSaved,
        .path = normalized_scene_path,
        .message = "Scene saved",
    });
    return true;
}

Entity AuthoringSession::createEntity(const std::string& name, Entity parent)
{
    if (!hasBoundScene()) {
        LUNA_CORE_WARN("Cannot create entity because no scene is bound");
        return {};
    }

    const bool implicit_transaction = beginImplicitTransaction("Create Entity");
    Entity entity =
        parent ? scene().entityManager().createChildEntity(parent, name) : scene().entityManager().createEntity(name);
    if (!entity) {
        (void) finishImplicitTransaction(implicit_transaction, false);
        return {};
    }

    markSceneDirty();
    queueEvent(AuthoringEvent{
        .type = AuthoringEventType::EntityCreated,
        .entity_id = entity.getUUID(),
        .message = entity.getName(),
    });
    (void) finishImplicitTransaction(implicit_transaction, true);
    return entity;
}

Entity AuthoringSession::createCameraEntity(Entity parent)
{
    const bool implicit_transaction = beginImplicitTransaction("Create Camera");
    Entity entity = createEntity("Camera", parent);
    if (!entity) {
        (void) finishImplicitTransaction(implicit_transaction, false);
        return {};
    }

    configureCameraEntity(entity);
    (void) finishImplicitTransaction(implicit_transaction, true);
    return entity;
}

Entity AuthoringSession::createDirectionalLightEntity(Entity parent)
{
    const bool implicit_transaction = beginImplicitTransaction("Create Directional Light");
    Entity entity = createEntity("Directional Light", parent);
    if (!entity) {
        (void) finishImplicitTransaction(implicit_transaction, false);
        return {};
    }

    configureDirectionalLightEntity(entity);
    (void) finishImplicitTransaction(implicit_transaction, true);
    return entity;
}

Entity AuthoringSession::createPointLightEntity(Entity parent)
{
    const bool implicit_transaction = beginImplicitTransaction("Create Point Light");
    Entity entity = createEntity("Point Light", parent);
    if (!entity) {
        (void) finishImplicitTransaction(implicit_transaction, false);
        return {};
    }

    configurePointLightEntity(entity);
    (void) finishImplicitTransaction(implicit_transaction, true);
    return entity;
}

Entity AuthoringSession::createSpotLightEntity(Entity parent)
{
    const bool implicit_transaction = beginImplicitTransaction("Create Spot Light");
    Entity entity = createEntity("Spot Light", parent);
    if (!entity) {
        (void) finishImplicitTransaction(implicit_transaction, false);
        return {};
    }

    configureSpotLightEntity(entity);
    (void) finishImplicitTransaction(implicit_transaction, true);
    return entity;
}

bool AuthoringSession::destroyEntity(Entity entity)
{
    if (!hasBoundScene()) {
        LUNA_CORE_WARN("Cannot destroy entity because no scene is bound");
        return false;
    }

    if (!isBoundSceneEntity(scene(), entity)) {
        return false;
    }

    const bool implicit_transaction = beginImplicitTransaction("Destroy Entity");
    const UUID entity_id = entity.getUUID();
    const std::string entity_name = entity.getName();
    scene().entityManager().destroyEntity(entity);
    markSceneDirty();
    queueEvent(AuthoringEvent{
        .type = AuthoringEventType::EntityDestroyed,
        .entity_id = entity_id,
        .message = entity_name,
    });
    (void) finishImplicitTransaction(implicit_transaction, true);
    return true;
}

bool AuthoringSession::reparentEntity(Entity entity, Entity parent, bool preserve_world_transform)
{
    if (!hasBoundScene()) {
        LUNA_CORE_WARN("Cannot reparent entity because no scene is bound");
        return false;
    }

    if (!isBoundSceneEntity(scene(), entity) || (parent && !isBoundSceneEntity(scene(), parent))) {
        return false;
    }

    const UUID previous_parent_id = entity.getParentUUID();
    const UUID next_parent_id = parent ? parent.getUUID() : UUID(0);
    if (previous_parent_id == next_parent_id) {
        return false;
    }

    const bool implicit_transaction = beginImplicitTransaction("Reparent Entity");
    if (!scene().entityManager().setParent(entity, parent, preserve_world_transform)) {
        (void) finishImplicitTransaction(implicit_transaction, false);
        return false;
    }

    markSceneDirty();
    queueEvent(AuthoringEvent{
        .type = AuthoringEventType::EntityReparented,
        .entity_id = entity.getUUID(),
        .message = entity.getName(),
    });
    (void) finishImplicitTransaction(implicit_transaction, true);
    return true;
}

bool AuthoringSession::addComponent(Entity entity, AuthoringComponentKind component_kind)
{
    if (!hasBoundScene()) {
        LUNA_CORE_WARN("Cannot add component because no scene is bound");
        return false;
    }

    if (!isBoundSceneEntity(scene(), entity)) {
        return false;
    }

    const bool implicit_transaction = beginImplicitTransaction("Add Component");
    bool added = false;
    switch (component_kind) {
        case AuthoringComponentKind::Camera:
            if (!entity.hasComponent<CameraComponent>()) {
                entity.addComponent<CameraComponent>();
                added = true;
            }
            break;
        case AuthoringComponentKind::Light:
            if (!entity.hasComponent<LightComponent>()) {
                entity.addComponent<LightComponent>();
                added = true;
            }
            break;
        case AuthoringComponentKind::Mesh:
            if (!entity.hasComponent<MeshComponent>()) {
                entity.addComponent<MeshComponent>();
                added = true;
            }
            break;
        case AuthoringComponentKind::Script:
            if (!entity.hasComponent<ScriptComponent>()) {
                entity.addComponent<ScriptComponent>();
                added = true;
            }
            break;
    }

    if (!added) {
        (void) finishImplicitTransaction(implicit_transaction, false);
        return false;
    }

    markSceneDirty();
    queueEvent(AuthoringEvent{
        .type = AuthoringEventType::ComponentAdded,
        .entity_id = entity.getUUID(),
        .message = componentKindName(component_kind),
    });
    (void) finishImplicitTransaction(implicit_transaction, true);
    return true;
}

bool AuthoringSession::removeComponent(Entity entity, AuthoringComponentKind component_kind)
{
    if (!hasBoundScene()) {
        LUNA_CORE_WARN("Cannot remove component because no scene is bound");
        return false;
    }

    if (!isBoundSceneEntity(scene(), entity)) {
        return false;
    }

    const bool implicit_transaction = beginImplicitTransaction("Remove Component");
    bool removed = false;
    switch (component_kind) {
        case AuthoringComponentKind::Camera:
            if (entity.hasComponent<CameraComponent>()) {
                entity.removeComponent<CameraComponent>();
                removed = true;
            }
            break;
        case AuthoringComponentKind::Light:
            if (entity.hasComponent<LightComponent>()) {
                entity.removeComponent<LightComponent>();
                removed = true;
            }
            break;
        case AuthoringComponentKind::Mesh:
            if (entity.hasComponent<MeshComponent>()) {
                entity.removeComponent<MeshComponent>();
                removed = true;
            }
            break;
        case AuthoringComponentKind::Script:
            if (entity.hasComponent<ScriptComponent>()) {
                entity.removeComponent<ScriptComponent>();
                removed = true;
            }
            break;
    }

    if (!removed) {
        (void) finishImplicitTransaction(implicit_transaction, false);
        return false;
    }

    markSceneDirty();
    queueEvent(AuthoringEvent{
        .type = AuthoringEventType::ComponentRemoved,
        .entity_id = entity.getUUID(),
        .message = componentKindName(component_kind),
    });
    (void) finishImplicitTransaction(implicit_transaction, true);
    return true;
}

Entity AuthoringSession::createEntityFromModelAsset(AssetHandle model_handle, Entity parent)
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

    const bool implicit_transaction = beginImplicitTransaction("Create Model Entity");
    Entity root = createEntity(root_name, parent);
    if (!root) {
        (void) finishImplicitTransaction(implicit_transaction, false);
        return {};
    }

    const auto& nodes = model->getNodes();
    std::vector<Entity> node_entities(nodes.size());
    for (size_t node_index = 0; node_index < nodes.size(); ++node_index) {
        const ModelNode& model_node = nodes[node_index];
        const std::string node_name =
            model_node.Name.empty() ? root_name + "_Node_" + std::to_string(node_index) : model_node.Name;

        Entity node_entity = createEntity(node_name, root);
        if (!node_entity) {
            continue;
        }

        auto& transform = node_entity.transform();
        transform.translation = model_node.Translation;
        transform.rotation = model_node.Rotation;
        transform.scale = model_node.Scale;

        if (model_node.MeshHandle.isValid()) {
            (void) applyMeshAssetToEntity(node_entity, model_node.MeshHandle);
            if (node_entity.hasComponent<MeshComponent>()) {
                auto& mesh_component = node_entity.getComponent<MeshComponent>();
                mesh_component.setSubmeshRange(model_node.FirstSubmesh, model_node.SubmeshCount);
                mesh_component.clearAllSubmeshMaterials();
                mesh_component.resizeSubmeshMaterials(model_node.SubmeshMaterials.size());
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
            (void) reparentEntity(node_entity, parent_entity, false);
        }
    }

    (void) finishImplicitTransaction(implicit_transaction, true);
    return root;
}

Entity AuthoringSession::createEntityFromMeshAsset(AssetHandle mesh_handle, Entity parent)
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

    const bool implicit_transaction = beginImplicitTransaction("Create Mesh Entity");
    Entity entity = createEntity(entity_name, parent);
    if (!entity) {
        (void) finishImplicitTransaction(implicit_transaction, false);
        return {};
    }

    (void) applyMeshAssetToEntity(entity, mesh_handle);
    (void) finishImplicitTransaction(implicit_transaction, true);
    return entity;
}

Entity AuthoringSession::createPrimitiveEntity(AssetHandle mesh_handle, Entity parent)
{
    if (!BuiltinAssets::isBuiltinMesh(mesh_handle)) {
        return {};
    }

    const bool implicit_transaction = beginImplicitTransaction("Create Primitive");
    Entity entity = createEntityFromMeshAsset(mesh_handle, parent);
    (void) finishImplicitTransaction(implicit_transaction, static_cast<bool>(entity));
    return entity;
}

bool AuthoringSession::applyMeshAssetToEntity(Entity entity, AssetHandle mesh_handle)
{
    if (!entity || !mesh_handle.isValid() || !AssetDatabase::exists(mesh_handle)) {
        return false;
    }

    const auto& metadata = AssetDatabase::getAssetMetadata(mesh_handle);
    if (metadata.Type != AssetType::Mesh) {
        return false;
    }

    const bool implicit_transaction = beginImplicitTransaction("Apply Mesh Asset");
    bool changed = false;
    if (!entity.hasComponent<MeshComponent>()) {
        entity.addComponent<MeshComponent>();
        changed = true;
    }

    auto& mesh_component = entity.getComponent<MeshComponent>();
    const bool changed_mesh = mesh_component.meshHandle != mesh_handle;
    mesh_component.meshHandle = mesh_handle;
    if (changed_mesh) {
        mesh_component.resetSubmeshRange();
        mesh_component.clearAllSubmeshMaterials();
        changed = true;
    }

    const auto mesh = AssetManager::get().requestAssetAs<Mesh>(mesh_handle);
    if (mesh && mesh->isValid()) {
        const size_t previous_slot_count = mesh_component.getSubmeshMaterialCount();
        const size_t active_submesh_count = mesh_component.resolveSubmeshCount(mesh->getSubMeshes().size());
        mesh_component.resizeSubmeshMaterials(active_submesh_count);
        changed |= mesh_component.getSubmeshMaterialCount() != previous_slot_count;
        for (uint32_t submesh_index = 0; submesh_index < mesh_component.getSubmeshMaterialCount(); ++submesh_index) {
            if (!mesh_component.getSubmeshMaterial(submesh_index).isValid()) {
                mesh_component.setSubmeshMaterial(submesh_index, BuiltinMaterials::DefaultLit);
                changed = true;
            }
        }
    }

    if (changed) {
        markSceneDirty();
        queueEvent(AuthoringEvent{
            .type = AuthoringEventType::EntityModified,
            .entity_id = entity.getUUID(),
            .message = entity.getName(),
        });
    }

    (void) finishImplicitTransaction(implicit_transaction, changed);
    return changed;
}

bool AuthoringSession::setEntityName(Entity entity, std::string name)
{
    if (!hasBoundScene()) {
        LUNA_CORE_WARN("Cannot rename entity because no scene is bound");
        return false;
    }

    if (!isBoundSceneEntity(scene(), entity) || !entity.hasComponent<TagComponent>()) {
        return false;
    }

    auto& tag = entity.getComponent<TagComponent>().tag;
    if (tag == name) {
        return false;
    }

    const bool implicit_transaction = beginImplicitTransaction("Rename Entity");
    tag = std::move(name);
    markSceneDirty();
    queueEntityModified(entity, tag);
    (void) finishImplicitTransaction(implicit_transaction, true);
    return true;
}

bool AuthoringSession::setEntityTransform(Entity entity, const TransformComponent& transform)
{
    if (!hasBoundScene()) {
        LUNA_CORE_WARN("Cannot set entity transform because no scene is bound");
        return false;
    }

    if (!isBoundSceneEntity(scene(), entity) || !entity.hasComponent<TransformComponent>()) {
        return false;
    }

    if (sameTransformComponent(entity.getComponent<TransformComponent>(), transform)) {
        return false;
    }

    const bool implicit_transaction = beginImplicitTransaction("Set Entity Transform");
    entity.getComponent<TransformComponent>() = transform;
    markSceneDirty();
    queueEntityModified(entity);
    (void) finishImplicitTransaction(implicit_transaction, true);
    return true;
}

bool AuthoringSession::setCameraComponent(Entity entity, const CameraComponent& camera_component)
{
    if (!hasBoundScene()) {
        LUNA_CORE_WARN("Cannot set camera component because no scene is bound");
        return false;
    }

    if (!isBoundSceneEntity(scene(), entity)) {
        return false;
    }

    if (!entity.hasComponent<CameraComponent>()) {
        const bool implicit_transaction = beginImplicitTransaction("Set Camera Component");
        entity.addComponent<CameraComponent>();
        queueEvent(AuthoringEvent{
            .type = AuthoringEventType::ComponentAdded,
            .entity_id = entity.getUUID(),
            .message = componentKindName(AuthoringComponentKind::Camera),
        });
        entity.getComponent<CameraComponent>() = camera_component;
        markSceneDirty();
        queueEntityModified(entity);
        (void) finishImplicitTransaction(implicit_transaction, true);
        return true;
    } else if (sameCameraComponent(entity.getComponent<CameraComponent>(), camera_component)) {
        return false;
    }

    const bool implicit_transaction = beginImplicitTransaction("Set Camera Component");
    entity.getComponent<CameraComponent>() = camera_component;
    markSceneDirty();
    queueEntityModified(entity);
    (void) finishImplicitTransaction(implicit_transaction, true);
    return true;
}

bool AuthoringSession::setLightComponent(Entity entity, const LightComponent& light_component)
{
    if (!hasBoundScene()) {
        LUNA_CORE_WARN("Cannot set light component because no scene is bound");
        return false;
    }

    if (!isBoundSceneEntity(scene(), entity)) {
        return false;
    }

    if (!entity.hasComponent<LightComponent>()) {
        const bool implicit_transaction = beginImplicitTransaction("Set Light Component");
        entity.addComponent<LightComponent>();
        queueEvent(AuthoringEvent{
            .type = AuthoringEventType::ComponentAdded,
            .entity_id = entity.getUUID(),
            .message = componentKindName(AuthoringComponentKind::Light),
        });
        entity.getComponent<LightComponent>() = light_component;
        markSceneDirty();
        queueEntityModified(entity);
        (void) finishImplicitTransaction(implicit_transaction, true);
        return true;
    } else if (sameLightComponent(entity.getComponent<LightComponent>(), light_component)) {
        return false;
    }

    const bool implicit_transaction = beginImplicitTransaction("Set Light Component");
    entity.getComponent<LightComponent>() = light_component;
    markSceneDirty();
    queueEntityModified(entity);
    (void) finishImplicitTransaction(implicit_transaction, true);
    return true;
}

bool AuthoringSession::setMeshComponent(Entity entity, const MeshComponent& mesh_component)
{
    if (!hasBoundScene()) {
        LUNA_CORE_WARN("Cannot set mesh component because no scene is bound");
        return false;
    }

    if (!isBoundSceneEntity(scene(), entity)) {
        return false;
    }

    if (!entity.hasComponent<MeshComponent>()) {
        const bool implicit_transaction = beginImplicitTransaction("Set Mesh Component");
        entity.addComponent<MeshComponent>();
        queueEvent(AuthoringEvent{
            .type = AuthoringEventType::ComponentAdded,
            .entity_id = entity.getUUID(),
            .message = componentKindName(AuthoringComponentKind::Mesh),
        });
        entity.getComponent<MeshComponent>() = mesh_component;
        markSceneDirty();
        queueEntityModified(entity);
        (void) finishImplicitTransaction(implicit_transaction, true);
        return true;
    } else if (sameMeshComponent(entity.getComponent<MeshComponent>(), mesh_component)) {
        return false;
    }

    const bool implicit_transaction = beginImplicitTransaction("Set Mesh Component");
    entity.getComponent<MeshComponent>() = mesh_component;
    markSceneDirty();
    queueEntityModified(entity);
    (void) finishImplicitTransaction(implicit_transaction, true);
    return true;
}

bool AuthoringSession::setScriptComponent(Entity entity, const ScriptComponent& script_component)
{
    if (!hasBoundScene()) {
        LUNA_CORE_WARN("Cannot set script component because no scene is bound");
        return false;
    }

    if (!isBoundSceneEntity(scene(), entity)) {
        return false;
    }

    if (!entity.hasComponent<ScriptComponent>()) {
        const bool implicit_transaction = beginImplicitTransaction("Set Script Component");
        entity.addComponent<ScriptComponent>();
        queueEvent(AuthoringEvent{
            .type = AuthoringEventType::ComponentAdded,
            .entity_id = entity.getUUID(),
            .message = componentKindName(AuthoringComponentKind::Script),
        });
        entity.getComponent<ScriptComponent>() = script_component;
        markSceneDirty();
        queueEntityModified(entity);
        (void) finishImplicitTransaction(implicit_transaction, true);
        return true;
    } else if (sameScriptComponent(entity.getComponent<ScriptComponent>(), script_component)) {
        return false;
    }

    const bool implicit_transaction = beginImplicitTransaction("Set Script Component");
    entity.getComponent<ScriptComponent>() = script_component;
    markSceneDirty();
    queueEntityModified(entity);
    (void) finishImplicitTransaction(implicit_transaction, true);
    return true;
}

bool AuthoringSession::setSceneEnvironmentSettings(const SceneEnvironmentSettings& settings)
{
    if (!hasBoundScene()) {
        LUNA_CORE_WARN("Cannot set scene environment because no scene is bound");
        return false;
    }

    if (sameEnvironmentSettings(scene().environmentSettings(), settings)) {
        return false;
    }

    const bool implicit_transaction = beginImplicitTransaction("Set Scene Environment");
    scene().environmentSettings() = settings;
    markSceneDirty();
    queueEvent(AuthoringEvent{
        .type = AuthoringEventType::SceneSettingsChanged,
        .message = "Scene environment settings changed",
    });
    (void) finishImplicitTransaction(implicit_transaction, true);
    return true;
}

bool AuthoringSession::setSceneShadowSettings(const SceneShadowSettings& settings)
{
    if (!hasBoundScene()) {
        LUNA_CORE_WARN("Cannot set scene shadows because no scene is bound");
        return false;
    }

    if (sameShadowSettings(scene().shadowSettings(), settings)) {
        return false;
    }

    const bool implicit_transaction = beginImplicitTransaction("Set Scene Shadows");
    scene().shadowSettings() = settings;
    markSceneDirty();
    queueEvent(AuthoringEvent{
        .type = AuthoringEventType::SceneSettingsChanged,
        .message = "Scene shadow settings changed",
    });
    (void) finishImplicitTransaction(implicit_transaction, true);
    return true;
}

void AuthoringSession::queueEvent(AuthoringEvent event)
{
    m_events.push_back(std::move(event));
}

void AuthoringSession::queueEntityModified(Entity entity, std::string message)
{
    if (!entity) {
        return;
    }

    if (message.empty()) {
        message = entity.getName();
    }

    queueEvent(AuthoringEvent{
        .type = AuthoringEventType::EntityModified,
        .entity_id = entity.getUUID(),
        .message = std::move(message),
    });
}

bool AuthoringSession::beginImplicitTransaction(std::string name)
{
    if (m_implicit_history_suppression_depth > 0 || m_history.hasOpenTransaction()) {
        return false;
    }

    return beginTransaction(std::move(name));
}

bool AuthoringSession::finishImplicitTransaction(bool implicit_transaction, bool changed)
{
    if (!implicit_transaction) {
        return changed;
    }

    if (changed) {
        (void) commitTransaction();
    } else {
        (void) rollbackTransaction();
    }
    return changed;
}

void AuthoringSession::suppressImplicitHistory()
{
    ++m_implicit_history_suppression_depth;
}

void AuthoringSession::resumeImplicitHistory()
{
    if (m_implicit_history_suppression_depth > 0) {
        --m_implicit_history_suppression_depth;
    }
}

void AuthoringSession::restoreHistoryState(AuthoringSceneState state, std::string message)
{
    if (!hasBoundScene() || state.scene == nullptr) {
        return;
    }

    const bool dirty_changed = m_scene_dirty != state.scene_dirty;
    scene().copyFrom(*state.scene);
    m_scene_file_path = std::move(state.scene_file_path);
    m_scene_dirty = state.scene_dirty;

    queueEvent(AuthoringEvent{
        .type = AuthoringEventType::HistoryChanged,
        .path = m_scene_file_path,
        .message = std::move(message),
    });

    if (dirty_changed) {
        queueEvent(AuthoringEvent{
            .type = AuthoringEventType::SceneDirtyChanged,
            .message = m_scene_dirty ? "Scene marked dirty" : "Scene marked clean",
        });
    }
}

} // namespace luna::authoring

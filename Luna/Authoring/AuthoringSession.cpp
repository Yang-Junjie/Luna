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

#include <glm/trigonometric.hpp>
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

void AuthoringSession::resetScene()
{
    if (!hasBoundScene()) {
        LUNA_CORE_WARN("Cannot reset scene because no scene is bound");
        return;
    }

    const bool was_dirty = m_scene_dirty;
    scene().entityManager().clear();
    scene().setName("Untitled");
    scene().environmentSettings() = {};
    scene().shadowSettings() = {};
    m_scene_file_path.clear();
    m_scene_dirty = false;
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
}

SceneBootstrapResult AuthoringSession::createScene()
{
    SceneBootstrapResult result{};
    if (!hasBoundScene()) {
        LUNA_CORE_WARN("Cannot create a scene because no scene is bound");
        return result;
    }

    resetScene();
    result.camera = createCameraEntity();
    result.directional_light = createDirectionalLightEntity();
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

    Entity entity =
        parent ? scene().entityManager().createChildEntity(parent, name) : scene().entityManager().createEntity(name);
    if (!entity) {
        return {};
    }

    markSceneDirty();
    queueEvent(AuthoringEvent{
        .type = AuthoringEventType::EntityCreated,
        .entity_id = entity.getUUID(),
        .message = entity.getName(),
    });
    return entity;
}

Entity AuthoringSession::createCameraEntity(Entity parent)
{
    Entity entity = createEntity("Camera", parent);
    if (!entity) {
        return {};
    }

    configureCameraEntity(entity);
    return entity;
}

Entity AuthoringSession::createDirectionalLightEntity(Entity parent)
{
    Entity entity = createEntity("Directional Light", parent);
    if (!entity) {
        return {};
    }

    configureDirectionalLightEntity(entity);
    return entity;
}

Entity AuthoringSession::createPointLightEntity(Entity parent)
{
    Entity entity = createEntity("Point Light", parent);
    if (!entity) {
        return {};
    }

    configurePointLightEntity(entity);
    return entity;
}

Entity AuthoringSession::createSpotLightEntity(Entity parent)
{
    Entity entity = createEntity("Spot Light", parent);
    if (!entity) {
        return {};
    }

    configureSpotLightEntity(entity);
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

    const UUID entity_id = entity.getUUID();
    const std::string entity_name = entity.getName();
    scene().entityManager().destroyEntity(entity);
    markSceneDirty();
    queueEvent(AuthoringEvent{
        .type = AuthoringEventType::EntityDestroyed,
        .entity_id = entity_id,
        .message = entity_name,
    });
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

    if (!scene().entityManager().setParent(entity, parent, preserve_world_transform)) {
        return false;
    }

    markSceneDirty();
    queueEvent(AuthoringEvent{
        .type = AuthoringEventType::EntityReparented,
        .entity_id = entity.getUUID(),
        .message = entity.getName(),
    });
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

    Entity root = createEntity(root_name, parent);
    if (!root) {
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

    Entity entity = createEntity(entity_name, parent);
    if (!entity) {
        return {};
    }

    (void) applyMeshAssetToEntity(entity, mesh_handle);
    return entity;
}

Entity AuthoringSession::createPrimitiveEntity(AssetHandle mesh_handle, Entity parent)
{
    if (!BuiltinAssets::isBuiltinMesh(mesh_handle)) {
        return {};
    }

    return createEntityFromMeshAsset(mesh_handle, parent);
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

    bool changed = false;
    if (!entity.hasComponent<MeshComponent>()) {
        entity.addComponent<MeshComponent>();
        changed = true;
    }

    auto& mesh_component = entity.getComponent<MeshComponent>();
    const bool changed_mesh = mesh_component.meshHandle != mesh_handle;
    mesh_component.meshHandle = mesh_handle;
    if (changed_mesh) {
        mesh_component.clearAllSubmeshMaterials();
        changed = true;
    }

    const auto mesh = AssetManager::get().requestAssetAs<Mesh>(mesh_handle);
    if (mesh && mesh->isValid()) {
        const size_t previous_slot_count = mesh_component.getSubmeshMaterialCount();
        mesh_component.resizeSubmeshMaterials(mesh->getSubMeshes().size());
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

    return changed;
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

    scene().environmentSettings() = settings;
    markSceneDirty();
    queueEvent(AuthoringEvent{
        .type = AuthoringEventType::SceneSettingsChanged,
        .message = "Scene environment settings changed",
    });
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

    scene().shadowSettings() = settings;
    markSceneDirty();
    queueEvent(AuthoringEvent{
        .type = AuthoringEventType::SceneSettingsChanged,
        .message = "Scene shadow settings changed",
    });
    return true;
}

void AuthoringSession::queueEvent(AuthoringEvent event)
{
    m_events.push_back(std::move(event));
}

} // namespace luna::authoring

#include "Core/Application.h"
#include "Core/Log.h"
#include "Entity.h"
#include "Renderer/RenderWorld/RenderWorldExtractor.h"
#include "Scene.h"

#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/matrix_decompose.hpp>

#include <memory>
#include <vector>
#include <utility>

namespace {

bool hasValidParent(const luna::EntityManager& entity_manager,
                    const entt::registry& registry,
                    entt::entity entity_handle)
{
    if (!registry.all_of<luna::RelationshipComponent>(entity_handle)) {
        return false;
    }

    const luna::UUID parent_id = registry.get<luna::RelationshipComponent>(entity_handle).parentHandle;
    return parent_id.isValid() && entity_manager.findEntityHandleByUUID(parent_id).has_value();
}

bool decomposeCameraTransform(const glm::mat4& transform, glm::vec3& out_translation, glm::quat& out_orientation)
{
    glm::vec3 scale{};
    glm::vec3 skew{};
    glm::vec4 perspective{};
    glm::quat orientation{};
    glm::vec3 translation{};

    if (!glm::decompose(transform, scale, orientation, translation, skew, perspective)) {
        return false;
    }

    out_translation = translation;
    out_orientation = glm::normalize(orientation);
    return true;
}

} // namespace

namespace luna {

Scene::Scene()
    : m_entity_manager(this)
{}

std::unique_ptr<Scene> Scene::clone() const
{
    auto cloned_scene = std::make_unique<Scene>();
    cloned_scene->copyFrom(*this);
    return cloned_scene;
}

void Scene::copyFrom(const Scene& source)
{
    if (this == &source) {
        return;
    }

    m_name = source.m_name;
    m_asset_load_behavior = source.m_asset_load_behavior;
    m_environment_settings = source.m_environment_settings;
    m_shadow_settings = source.m_shadow_settings;
    m_entity_manager.clear();

    auto& target_entity_manager = entityManager();
    const auto& registry = source.m_entity_manager.registry();

    std::vector<UUID> serialized_entity_ids;
    serialized_entity_ids.reserve(source.m_entity_manager.entityCount());

    auto view = registry.view<const IDComponent>();
    for (const auto entity_handle : view) {
        const auto& id_component = registry.get<const IDComponent>(entity_handle);
        serialized_entity_ids.push_back(id_component.id);

        std::string tag = "Entity";
        if (registry.all_of<TagComponent>(entity_handle)) {
            tag = registry.get<const TagComponent>(entity_handle).tag;
        }

        Entity cloned_entity = target_entity_manager.createEntityWithUUID(id_component.id, tag);
        if (!cloned_entity) {
            continue;
        }

        if (registry.all_of<TransformComponent>(entity_handle)) {
            cloned_entity.getComponent<TransformComponent>() = registry.get<const TransformComponent>(entity_handle);
        }

        if (registry.all_of<CameraComponent>(entity_handle)) {
            cloned_entity.addComponent<CameraComponent>(registry.get<const CameraComponent>(entity_handle));
        }

        if (registry.all_of<LightComponent>(entity_handle)) {
            cloned_entity.addComponent<LightComponent>(registry.get<const LightComponent>(entity_handle));
        }

        if (registry.all_of<MeshComponent>(entity_handle)) {
            cloned_entity.addComponent<MeshComponent>(registry.get<const MeshComponent>(entity_handle));
        }

        if (registry.all_of<ScriptComponent>(entity_handle)) {
            cloned_entity.addComponent<ScriptComponent>(registry.get<const ScriptComponent>(entity_handle));
        }
    }

    for (const UUID entity_id : serialized_entity_ids) {
        const auto source_entity_handle = source.m_entity_manager.findEntityHandleByUUID(entity_id);
        Entity cloned_entity = target_entity_manager.findEntityByUUID(entity_id);
        if (!source_entity_handle.has_value() || !cloned_entity) {
            continue;
        }

        UUID parent_id(0);
        if (registry.all_of<RelationshipComponent>(source_entity_handle.value())) {
            parent_id = registry.get<const RelationshipComponent>(source_entity_handle.value()).parentHandle;
        }
        if (!parent_id.isValid()) {
            continue;
        }

        Entity cloned_parent = target_entity_manager.findEntityByUUID(parent_id);
        if (cloned_parent) {
            target_entity_manager.setParent(cloned_entity, cloned_parent, false);
        }
    }
}

void Scene::renderFromRuntimeCamera()
{
    Camera runtime_camera;
    if (!findPrimaryRuntimeCamera(runtime_camera)) {
        return;
    }

    submitScene(runtime_camera);
}

void Scene::renderFromEditorCamera(const Camera& camera)
{
    submitScene(camera);
}

void Scene::submitScene(const Camera& camera)
{
    auto& renderer = Application::get().getRenderer();
    if (!renderer.isInitialized()) {
        return;
    }

    RenderWorldExtractor{}.extract(*this, camera, renderer.getRenderWorld());
}

bool Scene::findPrimaryRuntimeCamera(Camera& camera) const
{
    const auto& registry = m_entity_manager.registry();
    auto view = registry.view<TransformComponent, CameraComponent>();
    for (const auto entity_handle : view) {
        const auto& camera_component = view.get<CameraComponent>(entity_handle);
        if (!camera_component.primary) {
            continue;
        }

        const TransformComponent& local_transform = view.get<TransformComponent>(entity_handle);
        const bool has_parent = hasValidParent(m_entity_manager, registry, entity_handle);

        camera = camera_component.createCamera();
        if (has_parent) {
            glm::vec3 world_translation{};
            glm::quat world_orientation{};
            if (decomposeCameraTransform(
                    m_entity_manager.getWorldSpaceTransformMatrix(entity_handle), world_translation, world_orientation)) {
                camera.setPosition(world_translation);
                camera.setOrientation(world_orientation);
                return true;
            }
        }

        camera.setPosition(local_transform.translation);
        camera.setOrientationEuler(local_transform.rotation);
        return true;
    }

    LUNA_CORE_WARN("Scene '{}' has no primary camera; runtime scene will not render", m_name);
    return false;
}

void Scene::setAssetLoadBehavior(AssetLoadBehavior behavior)
{
    m_asset_load_behavior = behavior;
}

Scene::AssetLoadBehavior Scene::getAssetLoadBehavior() const
{
    return m_asset_load_behavior;
}

void Scene::setName(std::string name)
{
    m_name = name.empty() ? "Untitled" : std::move(name);
}

const std::string& Scene::getName() const
{
    return m_name;
}

void Scene::setIblEnabled(bool enabled)
{
    m_environment_settings.iblEnabled = enabled;
}

bool Scene::isIblEnabled() const
{
    return m_environment_settings.iblEnabled;
}

SceneEnvironmentSettings& Scene::environmentSettings()
{
    return m_environment_settings;
}

const SceneEnvironmentSettings& Scene::environmentSettings() const
{
    return m_environment_settings;
}

SceneShadowSettings& Scene::shadowSettings()
{
    return m_shadow_settings;
}

const SceneShadowSettings& Scene::shadowSettings() const
{
    return m_shadow_settings;
}

EntityManager& Scene::entityManager()
{
    return m_entity_manager;
}

const EntityManager& Scene::entityManager() const
{
    return m_entity_manager;
}

} // namespace luna

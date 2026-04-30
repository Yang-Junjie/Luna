#include "Core/Application.h"
#include "Core/Log.h"
#include "Entity.h"
#include "Renderer/RenderWorld/RenderWorldExtractor.h"
#include "Scene.h"

#include <memory>
#include <vector>
#include <utility>

namespace luna {

Scene::Scene()
    : m_entity_manager(this)
{}

std::unique_ptr<Scene> Scene::clone() const
{
    auto cloned_scene = std::make_unique<Scene>();
    cloned_scene->m_name = m_name;
    cloned_scene->m_asset_load_behavior = m_asset_load_behavior;
    cloned_scene->m_environment_settings = m_environment_settings;

    auto& cloned_entity_manager = cloned_scene->entityManager();
    const auto& registry = m_entity_manager.registry();

    std::vector<UUID> serialized_entity_ids;
    serialized_entity_ids.reserve(m_entity_manager.entityCount());

    auto view = registry.view<const IDComponent>();
    for (const auto entity_handle : view) {
        const auto& id_component = registry.get<const IDComponent>(entity_handle);
        serialized_entity_ids.push_back(id_component.id);

        std::string tag = "Entity";
        if (registry.all_of<TagComponent>(entity_handle)) {
            tag = registry.get<const TagComponent>(entity_handle).tag;
        }

        Entity cloned_entity = cloned_entity_manager.createEntityWithUUID(id_component.id, tag);
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
    }

    for (const UUID entity_id : serialized_entity_ids) {
        Entity source_entity = m_entity_manager.findEntityByUUID(entity_id);
        Entity cloned_entity = cloned_entity_manager.findEntityByUUID(entity_id);
        if (!source_entity || !cloned_entity) {
            continue;
        }

        const UUID parent_id = source_entity.getParentUUID();
        if (!parent_id.isValid()) {
            continue;
        }

        Entity cloned_parent = cloned_entity_manager.findEntityByUUID(parent_id);
        if (cloned_parent) {
            cloned_entity_manager.setParent(cloned_entity, cloned_parent, false);
        }
    }

    return cloned_scene;
}

void Scene::onUpdateRuntime()
{
    Camera runtime_camera;
    if (!findPrimaryRuntimeCamera(runtime_camera)) {
        return;
    }

    submitScene(runtime_camera);
}

void Scene::onUpdateEditor(const Camera& camera)
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

        Entity camera_entity(entity_handle, const_cast<EntityManager*>(&m_entity_manager));
        const TransformComponent world_transform = m_entity_manager.getWorldSpaceTransform(camera_entity);
        camera = camera_component.createCamera();
        camera.setPosition(world_transform.translation);
        camera.setOrientationEuler(world_transform.rotation);
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

SceneEnvironmentSettings& Scene::environmentSettings()
{
    return m_environment_settings;
}

const SceneEnvironmentSettings& Scene::environmentSettings() const
{
    return m_environment_settings;
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

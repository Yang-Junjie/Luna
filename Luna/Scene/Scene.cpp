#include "Core/Application.h"
#include "Core/Log.h"
#include "Entity.h"
#include "Renderer/RenderWorld/RenderWorldExtractor.h"
#include "Scene.h"

#include <utility>

namespace luna {

Scene::Scene()
    : m_entity_manager(this)
{}

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

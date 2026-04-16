#include "Core/Application.h"
#include "Entity.h"
#include "Renderer/SceneRenderer.h"
#include "Scene.h"

namespace luna {

Entity Scene::createEntity(const std::string& name)
{
    return createEntityWithUUID(UUID{}, name);
}

Entity Scene::createEntityWithUUID(UUID uuid, const std::string& name)
{
    const entt::entity entity_handle = m_registry.create();
    Entity entity(entity_handle, this);

    entity.addComponent<IDComponent>(uuid);
    entity.addComponent<TagComponent>(name.empty() ? "Entity" : name);
    entity.addComponent<TransformComponent>();

    return entity;
}

void Scene::destroyEntity(Entity entity)
{
    if (entity.m_scene != this || entity.m_entity_handle == entt::null) {
        return;
    }

    m_registry.destroy(entity.m_entity_handle);
}

void Scene::onUpdateRuntime()
{
    auto& renderer = Application::get().getRenderer();
    if (!renderer.isInitialized()) {
        return;
    }

    auto& scene_renderer = renderer.getSceneRenderer();
    scene_renderer.beginScene(renderer.getMainCamera());

    auto view = m_registry.view<TransformComponent, StaticMeshComponent>();
    for (const auto entity_handle : view) {
        const auto& transform_component = view.get<TransformComponent>(entity_handle);
        const auto& static_mesh_component = view.get<StaticMeshComponent>(entity_handle);

        if (!static_mesh_component.visible || static_mesh_component.mesh == nullptr) {
            continue;
        }

        scene_renderer.submitStaticMesh(
            transform_component.getTransform(), static_mesh_component.mesh, static_mesh_component.material);
    }
}

} // namespace luna

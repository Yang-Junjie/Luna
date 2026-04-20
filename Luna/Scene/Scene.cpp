#include "Core/Application.h"
#include "Asset/AssetManager.h"
#include "Entity.h"
#include "Renderer/Material.h"
#include "Renderer/Mesh.h"
#include "Renderer/SceneRenderer.h"
#include "Scene.h"

#include <utility>

namespace luna {

namespace {

std::vector<std::shared_ptr<Material>>
    resolveSubmeshMaterials(const MeshComponent& mesh_component, const Mesh& mesh, AssetManager& asset_manager)
{
    const auto& sub_meshes = mesh.getSubMeshes();
    std::vector<std::shared_ptr<Material>> submesh_materials(sub_meshes.size());

    for (size_t submesh_index = 0; submesh_index < sub_meshes.size(); ++submesh_index) {
        const AssetHandle material_handle = mesh_component.getSubmeshMaterial(static_cast<uint32_t>(submesh_index));
        if (!material_handle.isValid()) {
            continue;
        }

        submesh_materials[submesh_index] = asset_manager.loadAssetAs<Material>(material_handle);
    }

    return submesh_materials;
}

} // namespace

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

void Scene::clear()
{
    m_registry.clear();
}

void Scene::onUpdateRuntime()
{
    auto& renderer = Application::get().getRenderer();
    if (!renderer.isInitialized()) {
        return;
    }

    auto& scene_renderer = renderer.getSceneRenderer();
    auto& asset_manager = AssetManager::get();
    scene_renderer.beginScene(renderer.getMainCamera());

    auto view = m_registry.view<TransformComponent, MeshComponent>();
    for (const auto entity_handle : view) {
        const auto& transform_component = view.get<TransformComponent>(entity_handle);
        const auto& mesh_component = view.get<MeshComponent>(entity_handle);

        if (!mesh_component.meshHandle.isValid()) {
            continue;
        }

        const auto mesh = asset_manager.loadAssetAs<Mesh>(mesh_component.meshHandle);
        if (!mesh || !mesh->isValid()) {
            continue;
        }

        scene_renderer.submitStaticMesh(transform_component.getTransform(),
                                        mesh,
                                        resolveSubmeshMaterials(mesh_component, *mesh, asset_manager));
    }
}

void Scene::setName(std::string name)
{
    m_name = name.empty() ? "Untitled" : std::move(name);
}

const std::string& Scene::getName() const
{
    return m_name;
}

size_t Scene::entityCount() const
{
    size_t count = 0;
    for ([[maybe_unused]] const auto entity_handle : m_registry.view<IDComponent>()) {
        ++count;
    }

    return count;
}

entt::registry& Scene::registry()
{
    return m_registry;
}

const entt::registry& Scene::registry() const
{
    return m_registry;
}

} // namespace luna

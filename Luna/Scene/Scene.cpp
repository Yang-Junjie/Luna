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

Scene::Scene()
    : m_entity_manager(this)
{}

void Scene::onUpdateRuntime()
{
    auto& renderer = Application::get().getRenderer();
    if (!renderer.isInitialized()) {
        return;
    }

    auto& scene_renderer = renderer.getSceneRenderer();
    auto& asset_manager = AssetManager::get();
    auto& entity_manager = m_entity_manager;
    scene_renderer.beginScene(renderer.getMainCamera());

    auto& registry = entity_manager.registry();
    auto view = registry.view<TransformComponent, MeshComponent>();
    for (const auto entity_handle : view) {
        Entity entity(entity_handle, &entity_manager);
        const auto& mesh_component = view.get<MeshComponent>(entity_handle);

        if (!mesh_component.meshHandle.isValid()) {
            continue;
        }

        const auto mesh = asset_manager.loadAssetAs<Mesh>(mesh_component.meshHandle);
        if (!mesh || !mesh->isValid()) {
            continue;
        }

        scene_renderer.submitStaticMesh(entity_manager.getWorldSpaceTransformMatrix(entity),
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

EntityManager& Scene::entityManager()
{
    return m_entity_manager;
}

const EntityManager& Scene::entityManager() const
{
    return m_entity_manager;
}

} // namespace luna

#include "Core/Application.h"
#include "Asset/AssetManager.h"
#include "Core/Log.h"
#include "Entity.h"
#include "Renderer/Material.h"
#include "Renderer/Mesh.h"
#include "Renderer/SceneRenderer/SceneRenderer.h"
#include "Scene.h"

#include <utility>

namespace luna {

namespace {

std::vector<std::shared_ptr<Material>>
    resolveSubmeshMaterials(const MeshComponent& mesh_component,
                            const Mesh& mesh,
                            AssetManager& asset_manager,
                            Scene::AssetLoadBehavior asset_load_behavior)
{
    const auto& sub_meshes = mesh.getSubMeshes();
    std::vector<std::shared_ptr<Material>> submesh_materials(sub_meshes.size());

    for (size_t submesh_index = 0; submesh_index < sub_meshes.size(); ++submesh_index) {
        const AssetHandle material_handle = mesh_component.getSubmeshMaterial(static_cast<uint32_t>(submesh_index));
        if (!material_handle.isValid()) {
            continue;
        }

        submesh_materials[submesh_index] = asset_load_behavior == Scene::AssetLoadBehavior::NonBlocking
                                               ? asset_manager.requestAssetAs<Material>(material_handle)
                                               : asset_manager.loadAssetAs<Material>(material_handle);
    }

    return submesh_materials;
}

uint32_t encodePickingId(Entity entity)
{
    return entity ? (static_cast<uint32_t>(entity) + 1u) : 0u;
}

} // namespace

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

    auto& scene_renderer = renderer.getSceneRenderer();
    auto& asset_manager = AssetManager::get();
    auto& entity_manager = m_entity_manager;
    scene_renderer.beginScene(camera);
    auto& registry = entity_manager.registry();
    auto view = registry.view<TransformComponent, MeshComponent>();
    for (const auto entity_handle : view) {
        Entity entity(entity_handle, &entity_manager);
        const auto& mesh_component = view.get<MeshComponent>(entity_handle);

        if (!mesh_component.meshHandle.isValid()) {
            continue;
        }

        const auto mesh = m_asset_load_behavior == AssetLoadBehavior::NonBlocking
                              ? asset_manager.requestAssetAs<Mesh>(mesh_component.meshHandle)
                              : asset_manager.loadAssetAs<Mesh>(mesh_component.meshHandle);
        if (!mesh || !mesh->isValid()) {
            continue;
        }

        scene_renderer.submitStaticMesh(entity_manager.getWorldSpaceTransformMatrix(entity),
                                        mesh,
                                        resolveSubmeshMaterials(
                                            mesh_component, *mesh, asset_manager, m_asset_load_behavior),
                                        encodePickingId(entity));
    }
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

EntityManager& Scene::entityManager()
{
    return m_entity_manager;
}

const EntityManager& Scene::entityManager() const
{
    return m_entity_manager;
}

} // namespace luna

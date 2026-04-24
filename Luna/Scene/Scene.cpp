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

glm::vec3 safeNormalize(const glm::vec3& value, const glm::vec3& fallback)
{
    const float length_squared = glm::dot(value, value);
    if (length_squared <= 0.000001f) {
        return fallback;
    }
    return glm::normalize(value);
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

    bool has_directional_light = false;
    auto light_view = registry.view<TransformComponent, LightComponent>();
    for (const auto entity_handle : light_view) {
        const auto& light_component = light_view.get<LightComponent>(entity_handle);
        if (!light_component.enabled) {
            continue;
        }

        Entity light_entity(entity_handle, &entity_manager);
        const TransformComponent world_transform = entity_manager.getWorldSpaceTransform(light_entity);
        const float intensity = (std::max)(light_component.intensity, 0.0f);
        const float range = (std::max)(light_component.range, 0.001f);

        switch (light_component.type) {
            case LightComponent::Type::Directional:
                if (!has_directional_light) {
                    scene_renderer.submitDirectionalLight(SceneRenderer::DirectionalLight{
                        .direction = safeNormalize(-world_transform.getForward(), glm::vec3(0.0f, 1.0f, 0.0f)),
                        .intensity = intensity,
                        .color = light_component.color,
                    });
                    has_directional_light = true;
                }
                break;
            case LightComponent::Type::Point:
                scene_renderer.submitPointLight(SceneRenderer::PointLight{
                    .position = world_transform.translation,
                    .intensity = intensity,
                    .color = light_component.color,
                    .range = range,
                });
                break;
            case LightComponent::Type::Spot:
                scene_renderer.submitSpotLight(SceneRenderer::SpotLight{
                    .position = world_transform.translation,
                    .intensity = intensity,
                    .direction = safeNormalize(world_transform.getForward(), glm::vec3(0.0f, -1.0f, 0.0f)),
                    .range = range,
                    .color = light_component.color,
                    .innerConeCos = glm::cos(light_component.innerConeAngleRadians),
                    .outerConeCos = glm::cos(light_component.outerConeAngleRadians),
                });
                break;
        }
    }

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

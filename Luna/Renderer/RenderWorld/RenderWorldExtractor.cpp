#include "Renderer/RenderWorld/RenderWorldExtractor.h"

#include "Asset/AssetManager.h"
#include "Renderer/Material.h"
#include "Renderer/Mesh.h"
#include "Renderer/RenderWorld/RenderWorld.h"
#include "Scene/Entity.h"

#include <algorithm>
#include <glm/geometric.hpp>
#include <glm/trigonometric.hpp>

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

RenderPhaseMask phasesForMaterial(const std::shared_ptr<Material>& material)
{
    RenderPhaseMask phases = renderPhaseBit(RenderPhase::DepthOnly) | renderPhaseBit(RenderPhase::ShadowCaster) |
                             renderPhaseBit(RenderPhase::Picking);

    if (material && material->isTransparent()) {
        phases |= renderPhaseBit(RenderPhase::Transparent);
    } else {
        phases |= renderPhaseBit(RenderPhase::GBuffer) | renderPhaseBit(RenderPhase::ForwardOpaque);
    }

    return phases;
}

} // namespace

void RenderWorldExtractor::extract(Scene& scene, const Camera& camera, RenderWorld& render_world) const
{
    auto& asset_manager = AssetManager::get();
    auto& entity_manager = scene.entityManager();
    const Scene::AssetLoadBehavior asset_load_behavior = scene.getAssetLoadBehavior();

    render_world.begin(camera);
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
                    render_world.addDirectionalLight(RenderDirectionalLight{
                        .direction = safeNormalize(-world_transform.getForward(), glm::vec3(0.0f, 1.0f, 0.0f)),
                        .intensity = intensity,
                        .color = light_component.color,
                    });
                    has_directional_light = true;
                }
                break;
            case LightComponent::Type::Point:
                render_world.addPointLight(RenderPointLight{
                    .position = world_transform.translation,
                    .intensity = intensity,
                    .color = light_component.color,
                    .range = range,
                });
                break;
            case LightComponent::Type::Spot:
                render_world.addSpotLight(RenderSpotLight{
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

    auto mesh_view = registry.view<TransformComponent, MeshComponent>();
    for (const auto entity_handle : mesh_view) {
        Entity entity(entity_handle, &entity_manager);
        const auto& mesh_component = mesh_view.get<MeshComponent>(entity_handle);

        if (!mesh_component.meshHandle.isValid()) {
            continue;
        }

        const auto mesh = asset_load_behavior == Scene::AssetLoadBehavior::NonBlocking
                              ? asset_manager.requestAssetAs<Mesh>(mesh_component.meshHandle)
                              : asset_manager.loadAssetAs<Mesh>(mesh_component.meshHandle);
        if (!mesh || !mesh->isValid()) {
            continue;
        }

        auto submesh_materials = resolveSubmeshMaterials(mesh_component, *mesh, asset_manager, asset_load_behavior);
        render_world.addMeshInstance(RenderMeshInstance{
            .transform = entity_manager.getWorldSpaceTransformMatrix(entity),
            .mesh = mesh,
            .submesh_materials = submesh_materials,
            .picking_id = encodePickingId(entity),
        });

        const auto& sub_meshes = mesh->getSubMeshes();
        for (uint32_t submesh_index = 0; submesh_index < sub_meshes.size(); ++submesh_index) {
            std::shared_ptr<Material> material;
            if (submesh_index < submesh_materials.size()) {
                material = submesh_materials[submesh_index];
            }

            render_world.addDrawPacket(RenderDrawPacket{
                .transform = entity_manager.getWorldSpaceTransformMatrix(entity),
                .mesh = mesh,
                .material = material,
                .submesh_index = submesh_index,
                .picking_id = encodePickingId(entity),
                .phases = phasesForMaterial(material),
            });
        }
    }
}

} // namespace luna





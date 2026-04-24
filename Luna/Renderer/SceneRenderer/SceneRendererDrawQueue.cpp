#include "Core/Log.h"
#include "Renderer/Material.h"
#include "Renderer/Mesh.h"
#include "Renderer/RendererUtilities.h"
#include "Renderer/SceneRenderer/SceneRendererDrawQueue.h"
#include "Renderer/SceneRenderer/SceneRendererSupport.h"

#include <algorithm>

namespace luna::scene_renderer {

void DrawQueue::beginScene(const Camera& camera)
{
    m_camera = camera;
    LUNA_RENDERER_FRAME_TRACE("Beginning scene draw submission");
    clear();
}

void DrawQueue::clear() noexcept
{
    if (!m_opaque_draw_commands.empty() || !m_transparent_draw_commands.empty()) {
        LUNA_RENDERER_FRAME_TRACE("Clearing draw queue: opaque={} transparent={}",
                                  m_opaque_draw_commands.size(),
                                  m_transparent_draw_commands.size());
    }
    m_opaque_draw_commands.clear();
    m_transparent_draw_commands.clear();
    m_directional_light.reset();
    m_point_lights.clear();
    m_spot_lights.clear();
}

void DrawQueue::submitDirectionalLight(const DirectionalLightSubmission& light)
{
    m_directional_light = light;
    LUNA_RENDERER_FRAME_DEBUG("Submitted directional light: direction=({}, {}, {}) color=({}, {}, {}) intensity={}",
                              light.direction.x,
                              light.direction.y,
                              light.direction.z,
                              light.color.r,
                              light.color.g,
                              light.color.b,
                              light.intensity);
}

void DrawQueue::submitPointLight(const PointLightSubmission& light)
{
    m_point_lights.push_back(light);
    LUNA_RENDERER_FRAME_DEBUG("Submitted point light: position=({}, {}, {}) color=({}, {}, {}) intensity={} range={}",
                              light.position.x,
                              light.position.y,
                              light.position.z,
                              light.color.r,
                              light.color.g,
                              light.color.b,
                              light.intensity,
                              light.range);
}

void DrawQueue::submitSpotLight(const SpotLightSubmission& light)
{
    m_spot_lights.push_back(light);
    LUNA_RENDERER_FRAME_DEBUG(
        "Submitted spot light: position=({}, {}, {}) direction=({}, {}, {}) color=({}, {}, {}) intensity={} range={}",
        light.position.x,
        light.position.y,
        light.position.z,
        light.direction.x,
        light.direction.y,
        light.direction.z,
        light.color.r,
        light.color.g,
        light.color.b,
        light.intensity,
        light.range);
}

void DrawQueue::submitStaticMesh(const glm::mat4& transform,
                                 std::shared_ptr<Mesh> mesh,
                                 std::shared_ptr<Material> material,
                                 uint32_t picking_id)
{
    if (!mesh || !mesh->isValid()) {
        LUNA_RENDERER_WARN("Ignoring static mesh submission because mesh is null or invalid");
        return;
    }

    const size_t sub_mesh_count = mesh->getSubMeshes().size();
    std::vector<std::shared_ptr<Material>> submesh_materials(sub_mesh_count, std::move(material));
    submitStaticMesh(transform, std::move(mesh), submesh_materials, picking_id);
}

void DrawQueue::submitStaticMesh(const glm::mat4& transform,
                                 std::shared_ptr<Mesh> mesh,
                                 const std::vector<std::shared_ptr<Material>>& submesh_materials,
                                 uint32_t picking_id)
{
    if (!mesh || !mesh->isValid()) {
        LUNA_RENDERER_WARN("Ignoring static mesh submission because mesh is null or invalid");
        return;
    }

    const auto& mesh_sub_meshes = mesh->getSubMeshes();
    const std::string mesh_name = mesh->getName().empty() ? "<unnamed>" : mesh->getName();
    size_t submitted_opaque_count = 0;
    size_t submitted_transparent_count = 0;
    size_t skipped_sub_mesh_count = 0;
    for (size_t sub_mesh_index = 0; sub_mesh_index < mesh_sub_meshes.size(); ++sub_mesh_index) {
        const auto& sub_mesh = mesh_sub_meshes[sub_mesh_index];
        if (sub_mesh.Vertices.empty() || sub_mesh.Indices.empty()) {
            ++skipped_sub_mesh_count;
            LUNA_RENDERER_FRAME_TRACE("Skipping empty submesh {} from mesh '{}'", sub_mesh_index, mesh_name);
            continue;
        }

        std::shared_ptr<Material> material;
        if (sub_mesh_index < submesh_materials.size()) {
            material = submesh_materials[sub_mesh_index];
        }

        DrawCommand draw_command{
            .transform = transform,
            .mesh = mesh,
            .material = std::move(material),
            .sub_mesh_index = static_cast<uint32_t>(sub_mesh_index),
            .picking_id = picking_id,
        };

        if (draw_command.material != nullptr && draw_command.material->isTransparent()) {
            m_transparent_draw_commands.push_back(std::move(draw_command));
            ++submitted_transparent_count;
            continue;
        }

        m_opaque_draw_commands.push_back(std::move(draw_command));
        ++submitted_opaque_count;
    }
    LUNA_RENDERER_FRAME_DEBUG("Submitted mesh '{}' to draw queue: opaque={} transparent={} skipped={} picking_id={}",
                              mesh_name,
                              submitted_opaque_count,
                              submitted_transparent_count,
                              skipped_sub_mesh_count,
                              picking_id);
}

void DrawQueue::sortTransparentBackToFront()
{
    using namespace scene_renderer_detail;

    LUNA_RENDERER_FRAME_TRACE("Sorting {} transparent draw command(s) back-to-front",
                              m_transparent_draw_commands.size());
    const glm::vec3 camera_position = resolveCameraPosition(m_camera);
    std::sort(m_transparent_draw_commands.begin(),
              m_transparent_draw_commands.end(),
              [camera_position](const DrawCommand& lhs, const DrawCommand& rhs) {
                  return transparentSortDistanceSq(lhs.transform, camera_position) >
                         transparentSortDistanceSq(rhs.transform, camera_position);
              });
}

} // namespace luna::scene_renderer

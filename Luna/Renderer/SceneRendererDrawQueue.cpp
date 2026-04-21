#include "Renderer/SceneRendererDrawQueue.h"

#include "Renderer/Mesh.h"
#include "Renderer/SceneRendererCommon.h"

#include <algorithm>

namespace luna::scene_renderer {

void DrawQueue::beginScene(const Camera& camera)
{
    m_camera = camera;
    clear();
}

void DrawQueue::clear() noexcept
{
    m_opaque_draw_commands.clear();
    m_transparent_draw_commands.clear();
}

void DrawQueue::submitStaticMesh(const glm::mat4& transform,
                                 std::shared_ptr<Mesh> mesh,
                                 std::shared_ptr<Material> material)
{
    if (!mesh || !mesh->isValid()) {
        return;
    }

    const size_t sub_mesh_count = mesh->getSubMeshes().size();
    std::vector<std::shared_ptr<Material>> submesh_materials(sub_mesh_count, std::move(material));
    submitStaticMesh(transform, std::move(mesh), submesh_materials);
}

void DrawQueue::submitStaticMesh(const glm::mat4& transform,
                                 std::shared_ptr<Mesh> mesh,
                                 const std::vector<std::shared_ptr<Material>>& submesh_materials)
{
    if (!mesh || !mesh->isValid()) {
        return;
    }

    const auto& mesh_sub_meshes = mesh->getSubMeshes();
    for (size_t sub_mesh_index = 0; sub_mesh_index < mesh_sub_meshes.size(); ++sub_mesh_index) {
        const auto& sub_mesh = mesh_sub_meshes[sub_mesh_index];
        if (sub_mesh.Vertices.empty() || sub_mesh.Indices.empty()) {
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
        };

        if (draw_command.material != nullptr && draw_command.material->isTransparent()) {
            m_transparent_draw_commands.push_back(std::move(draw_command));
            continue;
        }

        m_opaque_draw_commands.push_back(std::move(draw_command));
    }
}

void DrawQueue::sortTransparentBackToFront()
{
    using namespace scene_renderer_detail;

    const glm::vec3 camera_position = resolveCameraPosition(m_camera);
    std::sort(m_transparent_draw_commands.begin(),
              m_transparent_draw_commands.end(),
              [camera_position](const DrawCommand& lhs, const DrawCommand& rhs) {
                  return transparentSortDistanceSq(lhs.transform, camera_position) >
                         transparentSortDistanceSq(rhs.transform, camera_position);
              });
}

} // namespace luna::scene_renderer

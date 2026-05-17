#include "Core/Log.h"
#include "Renderer/Mesh.h"

#include <glm/common.hpp>
#include <glm/geometric.hpp>
#include <glm/mat3x3.hpp>
#include <limits>
#include <numeric>
#include <utility>

namespace luna {

namespace {

MeshBounds finalizeBounds(const glm::vec3& min, const glm::vec3& max)
{
    const glm::vec3 center = (min + max) * 0.5f;
    const glm::vec3 extents = (max - min) * 0.5f;
    return MeshBounds{
        .Min = min,
        .Max = max,
        .Center = center,
        .Extents = extents,
        .Radius = glm::length(extents),
        .Valid = true,
    };
}

MeshBounds computeSubMeshBounds(const SubMesh& sub_mesh)
{
    if (sub_mesh.Vertices.empty()) {
        return {};
    }

    glm::vec3 min_bounds(std::numeric_limits<float>::max());
    glm::vec3 max_bounds(std::numeric_limits<float>::lowest());
    for (const StaticMeshVertex& vertex : sub_mesh.Vertices) {
        min_bounds = glm::min(min_bounds, vertex.Position);
        max_bounds = glm::max(max_bounds, vertex.Position);
    }

    return finalizeBounds(min_bounds, max_bounds);
}

void includeBounds(MeshBounds& bounds, const MeshBounds& added_bounds)
{
    if (!added_bounds.isValid()) {
        return;
    }

    if (!bounds.isValid()) {
        bounds = added_bounds;
        return;
    }

    bounds = finalizeBounds(glm::min(bounds.Min, added_bounds.Min), glm::max(bounds.Max, added_bounds.Max));
}

MeshBounds computeMeshBounds(std::vector<SubMesh>& sub_meshes)
{
    MeshBounds mesh_bounds{};
    for (SubMesh& sub_mesh : sub_meshes) {
        sub_mesh.Bounds = computeSubMeshBounds(sub_mesh);
        includeBounds(mesh_bounds, sub_mesh.Bounds);
    }
    return mesh_bounds;
}

} // namespace

MeshBounds mergeMeshBounds(const MeshBounds& lhs, const MeshBounds& rhs)
{
    if (!lhs.isValid()) {
        return rhs;
    }

    if (!rhs.isValid()) {
        return lhs;
    }

    return finalizeBounds(glm::min(lhs.Min, rhs.Min), glm::max(lhs.Max, rhs.Max));
}

MeshBounds transformMeshBounds(const MeshBounds& bounds, const glm::mat4& transform)
{
    if (!bounds.isValid()) {
        return {};
    }

    const glm::vec3 center = glm::vec3(transform * glm::vec4(bounds.Center, 1.0f));
    const glm::mat3 linear_transform(transform);
    const glm::vec3 extents = glm::abs(linear_transform[0]) * bounds.Extents.x +
                              glm::abs(linear_transform[1]) * bounds.Extents.y +
                              glm::abs(linear_transform[2]) * bounds.Extents.z;

    return finalizeBounds(center - extents, center + extents);
}

Mesh::Mesh(std::string name, std::vector<SubMesh> subMeshes)
    : m_name(std::move(name)),
      m_subMeshes(std::move(subMeshes))
{
    m_bounds = computeMeshBounds(m_subMeshes);

    const size_t vertex_count =
        std::accumulate(m_subMeshes.begin(), m_subMeshes.end(), size_t{0}, [](size_t total, const SubMesh& sub_mesh) {
            return total + sub_mesh.Vertices.size();
        });
    const size_t index_count =
        std::accumulate(m_subMeshes.begin(), m_subMeshes.end(), size_t{0}, [](size_t total, const SubMesh& sub_mesh) {
            return total + sub_mesh.Indices.size();
        });
    LUNA_RENDERER_DEBUG("Created mesh '{}' with {} submesh(es), {} vertex/vertices, {} index/indices",
                        m_name.empty() ? "<unnamed>" : m_name,
                        m_subMeshes.size(),
                        vertex_count,
                        index_count);
}

std::shared_ptr<Mesh> Mesh::create(std::string name, std::vector<SubMesh> subMeshes)
{
    return std::make_shared<Mesh>(std::move(name), std::move(subMeshes));
}

const std::string& Mesh::getName() const
{
    return m_name;
}

const std::vector<SubMesh>& Mesh::getSubMeshes() const
{
    return m_subMeshes;
}

const MeshBounds& Mesh::getBounds() const
{
    return m_bounds;
}

bool Mesh::isValid() const
{
    if (m_subMeshes.empty()) {
        return false;
    }

    for (const auto& sm : m_subMeshes) {
        if (sm.Vertices.empty() || sm.Indices.empty()) {
            return false;
        }
    }
    return true;
}

} // namespace luna

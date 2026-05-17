#include "Renderer/Visibility/Frustum.h"

#include <glm/common.hpp>
#include <glm/geometric.hpp>
#include <glm/vec4.hpp>

namespace luna {
namespace {

glm::vec4 matrixRow(const glm::mat4& matrix, size_t row)
{
    return {matrix[0][row], matrix[1][row], matrix[2][row], matrix[3][row]};
}

Plane normalizePlane(const glm::vec4& plane)
{
    const glm::vec3 normal(plane);
    const float length = glm::length(normal);
    if (length <= 0.000001f) {
        return {};
    }

    return Plane{
        .Normal = normal / length,
        .Distance = plane.w / length,
    };
}

} // namespace

Frustum Frustum::fromViewProjection(const glm::mat4& view_projection)
{
    const glm::vec4 row_x = matrixRow(view_projection, 0);
    const glm::vec4 row_y = matrixRow(view_projection, 1);
    const glm::vec4 row_z = matrixRow(view_projection, 2);
    const glm::vec4 row_w = matrixRow(view_projection, 3);

    Frustum frustum;
    frustum.m_planes[static_cast<size_t>(PlaneIndex::Left)] = normalizePlane(row_w + row_x);
    frustum.m_planes[static_cast<size_t>(PlaneIndex::Right)] = normalizePlane(row_w - row_x);
    frustum.m_planes[static_cast<size_t>(PlaneIndex::Bottom)] = normalizePlane(row_w + row_y);
    frustum.m_planes[static_cast<size_t>(PlaneIndex::Top)] = normalizePlane(row_w - row_y);
    frustum.m_planes[static_cast<size_t>(PlaneIndex::Near)] = normalizePlane(row_z);
    frustum.m_planes[static_cast<size_t>(PlaneIndex::Far)] = normalizePlane(row_w - row_z);
    return frustum;
}

bool Frustum::intersects(const MeshBounds& bounds) const noexcept
{
    if (!bounds.isValid()) {
        return true;
    }

    for (const Plane& plane : m_planes) {
        const float projected_radius = glm::dot(glm::abs(plane.Normal), bounds.Extents);
        const float center_distance = glm::dot(plane.Normal, bounds.Center) + plane.Distance;
        if (center_distance + projected_radius < 0.0f) {
            return false;
        }
    }

    return true;
}

const std::array<Plane, static_cast<size_t>(Frustum::PlaneIndex::Count)>& Frustum::planes() const noexcept
{
    return m_planes;
}

} // namespace luna

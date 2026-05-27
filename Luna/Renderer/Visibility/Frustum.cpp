#include "Renderer/Camera.h"
#include "Renderer/Visibility/Frustum.h"

#include <cmath>

#include <algorithm>
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

FrustumCorners perspectiveFrustumCorners(const Camera& camera, float aspect_ratio)
{
    const Camera::PerspectiveSettings& settings = camera.getPerspectiveSettings();
    const float near_distance = settings.near_clip;
    const float far_distance = settings.far_clip;
    const float tan_half_fov = std::tan(settings.vertical_fov_radians * 0.5f);
    const float near_half_height = tan_half_fov * near_distance;
    const float near_half_width = near_half_height * aspect_ratio;
    const float far_half_height = tan_half_fov * far_distance;
    const float far_half_width = far_half_height * aspect_ratio;

    const glm::vec3 position = camera.getPosition();
    const glm::vec3 forward = camera.getForwardDirection();
    const glm::vec3 right = camera.getRightDirection();
    const glm::vec3 up = camera.getUpDirection();
    const glm::vec3 near_center = position + forward * near_distance;
    const glm::vec3 far_center = position + forward * far_distance;

    return {{
        near_center - right * near_half_width - up * near_half_height,
        near_center + right * near_half_width - up * near_half_height,
        near_center + right * near_half_width + up * near_half_height,
        near_center - right * near_half_width + up * near_half_height,
        far_center - right * far_half_width - up * far_half_height,
        far_center + right * far_half_width - up * far_half_height,
        far_center + right * far_half_width + up * far_half_height,
        far_center - right * far_half_width + up * far_half_height,
    }};
}

FrustumCorners orthographicFrustumCorners(const Camera& camera, float aspect_ratio)
{
    const Camera::OrthographicSettings& settings = camera.getOrthographicSettings();
    const float half_height = settings.vertical_size * 0.5f;
    const float half_width = half_height * aspect_ratio;

    const glm::vec3 position = camera.getPosition();
    const glm::vec3 forward = camera.getForwardDirection();
    const glm::vec3 right = camera.getRightDirection();
    const glm::vec3 up = camera.getUpDirection();
    const glm::vec3 near_center = position + forward * settings.near_clip;
    const glm::vec3 far_center = position + forward * settings.far_clip;

    return {{
        near_center - right * half_width - up * half_height,
        near_center + right * half_width - up * half_height,
        near_center + right * half_width + up * half_height,
        near_center - right * half_width + up * half_height,
        far_center - right * half_width - up * half_height,
        far_center + right * half_width - up * half_height,
        far_center + right * half_width + up * half_height,
        far_center - right * half_width + up * half_height,
    }};
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

FrustumCorners cameraFrustumCorners(const Camera& camera, float aspect_ratio)
{
    const float clamped_aspect_ratio = std::max(aspect_ratio, 0.001f);
    if (camera.getProjectionType() == Camera::ProjectionType::Orthographic) {
        return orthographicFrustumCorners(camera, clamped_aspect_ratio);
    }
    return perspectiveFrustumCorners(camera, clamped_aspect_ratio);
}

} // namespace luna

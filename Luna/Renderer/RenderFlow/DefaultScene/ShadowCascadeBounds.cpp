#include "Math/Math.h"
#include "Renderer/RenderFlow/DefaultScene/ShadowCascadeBounds.h"

#include <cmath>

#include <algorithm>
#include <array>
#include <glm/common.hpp>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/geometric.hpp>
#include <glm/gtx/norm.hpp>
#include <limits>

namespace luna::render_flow::default_scene_detail {
namespace {

constexpr float kCascadeBoundsPaddingScale = 1.08f;
constexpr float kLightSpaceBoundsEpsilon = 0.001f;

glm::mat4 adjustProjectionForConventions(glm::mat4 projection, const luna::RHI::RHIConventions& conventions)
{
    return conventions.requires_projection_y_flip ? luna::flipProjectionY(projection) : projection;
}

void includePoint(glm::vec3& min_bounds, glm::vec3& max_bounds, const glm::vec3& point)
{
    min_bounds = glm::min(min_bounds, point);
    max_bounds = glm::max(max_bounds, point);
}

std::array<glm::vec3, 8> meshBoundsCorners(const MeshBounds& bounds)
{
    return {{
        {bounds.Min.x, bounds.Min.y, bounds.Min.z},
        {bounds.Max.x, bounds.Min.y, bounds.Min.z},
        {bounds.Max.x, bounds.Max.y, bounds.Min.z},
        {bounds.Min.x, bounds.Max.y, bounds.Min.z},
        {bounds.Min.x, bounds.Min.y, bounds.Max.z},
        {bounds.Max.x, bounds.Min.y, bounds.Max.z},
        {bounds.Max.x, bounds.Max.y, bounds.Max.z},
        {bounds.Min.x, bounds.Max.y, bounds.Max.z},
    }};
}

} // namespace

ShadowCascadeLightBounds buildShadowCascadeReceiverBounds(const std::array<glm::vec3, 8>& corners,
                                                          const glm::vec3& light_direction,
                                                          uint32_t shadow_map_size)
{
    glm::vec3 center{0.0f};
    for (const glm::vec3& corner : corners) {
        center += corner;
    }
    center /= static_cast<float>(corners.size());

    glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);
    if (glm::length2(glm::cross(-light_direction, up)) <= 1.0e-6f) {
        up = glm::vec3(0.0f, 0.0f, 1.0f);
    }

    ShadowCascadeLightBounds bounds{};
    bounds.view = glm::lookAtRH(center + light_direction, center, up);
    bounds.min_bounds = glm::vec3(std::numeric_limits<float>::max());
    bounds.max_bounds = glm::vec3(std::numeric_limits<float>::lowest());
    for (const glm::vec3& corner : corners) {
        includePoint(bounds.min_bounds, bounds.max_bounds, glm::vec3(bounds.view * glm::vec4(corner, 1.0f)));
    }

    const glm::vec2 bounds_center = (glm::vec2(bounds.min_bounds) + glm::vec2(bounds.max_bounds)) * 0.5f;
    const glm::vec2 half_extent =
        (glm::vec2(bounds.max_bounds) - glm::vec2(bounds.min_bounds)) * (0.5f * kCascadeBoundsPaddingScale);
    bounds.min_bounds.x = bounds_center.x - half_extent.x;
    bounds.max_bounds.x = bounds_center.x + half_extent.x;
    bounds.min_bounds.y = bounds_center.y - half_extent.y;
    bounds.max_bounds.y = bounds_center.y + half_extent.y;

    const float cascade_width = bounds.max_bounds.x - bounds.min_bounds.x;
    const float cascade_height = bounds.max_bounds.y - bounds.min_bounds.y;
    const float texel_size_x = cascade_width / static_cast<float>(std::max(shadow_map_size, 1u));
    const float texel_size_y = cascade_height / static_cast<float>(std::max(shadow_map_size, 1u));
    if (texel_size_x > 0.0f && texel_size_y > 0.0f) {
        const glm::vec2 snapped_center{
            std::floor(bounds_center.x / texel_size_x) * texel_size_x,
            std::floor(bounds_center.y / texel_size_y) * texel_size_y,
        };
        bounds.min_bounds.x = snapped_center.x - half_extent.x;
        bounds.max_bounds.x = snapped_center.x + half_extent.x;
        bounds.min_bounds.y = snapped_center.y - half_extent.y;
        bounds.max_bounds.y = snapped_center.y + half_extent.y;
    }

    return bounds;
}

bool expandShadowCascadeDepthForCaster(ShadowCascadeLightBounds& bounds, const MeshBounds& caster_world_bounds)
{
    if (!caster_world_bounds.isValid()) {
        return true;
    }

    glm::vec3 caster_min(std::numeric_limits<float>::max());
    glm::vec3 caster_max(std::numeric_limits<float>::lowest());
    for (const glm::vec3& corner : meshBoundsCorners(caster_world_bounds)) {
        includePoint(caster_min, caster_max, glm::vec3(bounds.view * glm::vec4(corner, 1.0f)));
    }

    if (caster_max.x < bounds.min_bounds.x || caster_min.x > bounds.max_bounds.x ||
        caster_max.y < bounds.min_bounds.y || caster_min.y > bounds.max_bounds.y) {
        return false;
    }

    bounds.min_bounds.z = std::min(bounds.min_bounds.z, caster_min.z);
    bounds.max_bounds.z = std::max(bounds.max_bounds.z, caster_max.z);
    return true;
}

void padShadowCascadeDepth(ShadowCascadeLightBounds& bounds, float depth_padding)
{
    const float padding = std::max(depth_padding, kLightSpaceBoundsEpsilon);
    bounds.min_bounds.z -= padding;
    bounds.max_bounds.z += padding;
    if (bounds.max_bounds.z - bounds.min_bounds.z < kLightSpaceBoundsEpsilon) {
        const float center_z = (bounds.min_bounds.z + bounds.max_bounds.z) * 0.5f;
        bounds.min_bounds.z = center_z - kLightSpaceBoundsEpsilon * 0.5f;
        bounds.max_bounds.z = center_z + kLightSpaceBoundsEpsilon * 0.5f;
    }
}

glm::mat4 buildShadowCascadeViewProjection(const ShadowCascadeLightBounds& bounds,
                                           const luna::RHI::RHIConventions& conventions)
{
    const glm::mat4 projection = glm::orthoRH_ZO(bounds.min_bounds.x,
                                                 bounds.max_bounds.x,
                                                 bounds.min_bounds.y,
                                                 bounds.max_bounds.y,
                                                 -bounds.max_bounds.z,
                                                 -bounds.min_bounds.z);
    return adjustProjectionForConventions(projection, conventions) * bounds.view;
}

} // namespace luna::render_flow::default_scene_detail

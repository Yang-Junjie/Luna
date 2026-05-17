#pragma once

#include "Renderer/Mesh.h"

#include <cstdint>

#include <array>
#include <Capabilities.h>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

namespace luna::render_flow::default_scene_detail {

struct ShadowCascadeLightBounds {
    glm::mat4 view{1.0f};
    glm::vec3 min_bounds{0.0f};
    glm::vec3 max_bounds{0.0f};
};

[[nodiscard]] ShadowCascadeLightBounds buildShadowCascadeReceiverBounds(const std::array<glm::vec3, 8>& corners,
                                                                        const glm::vec3& light_direction,
                                                                        uint32_t shadow_map_size);

[[nodiscard]] bool expandShadowCascadeDepthForCaster(ShadowCascadeLightBounds& bounds,
                                                     const MeshBounds& caster_world_bounds);

void padShadowCascadeDepth(ShadowCascadeLightBounds& bounds, float depth_padding);

[[nodiscard]] glm::mat4 buildShadowCascadeViewProjection(const ShadowCascadeLightBounds& bounds,
                                                         const luna::RHI::RHIConventions& conventions);

} // namespace luna::render_flow::default_scene_detail

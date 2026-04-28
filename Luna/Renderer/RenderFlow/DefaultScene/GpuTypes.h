#pragma once

#include "Renderer/RenderFlow/DefaultScene/Constants.h"

#include <array>
#include <cstdint>
#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>

namespace luna::render_flow::default_scene_detail {

struct MeshPushConstants {
    glm::mat4 model{1.0f};
    uint32_t picking_id{0};
    uint32_t padding[3]{0, 0, 0};
};

struct SceneGpuParams {
    glm::mat4 view_projection{1.0f};
    glm::mat4 inverse_view_projection{1.0f};
    glm::mat4 view{1.0f};
    glm::mat4 projection{1.0f};
    glm::mat4 inverse_view{1.0f};
    glm::mat4 inverse_projection{1.0f};
    glm::mat4 jittered_view_projection{1.0f};
    glm::mat4 inverse_jittered_view_projection{1.0f};
    glm::mat4 previous_view_projection{1.0f};
    glm::mat4 previous_inverse_view_projection{1.0f};
    glm::mat4 previous_jittered_view_projection{1.0f};
    glm::mat4 previous_inverse_jittered_view_projection{1.0f};
    glm::vec4 camera_position_env_mip{0.0f, 0.0f, 0.0f, 0.0f};
    glm::vec4 viewport_size{0.0f, 0.0f, 0.0f, 0.0f};
    glm::vec4 previous_viewport_size{0.0f, 0.0f, 0.0f, 0.0f};
    glm::vec4 jitter_ndc{0.0f, 0.0f, 0.0f, 0.0f};
    glm::vec4 jitter_pixels{0.0f, 0.0f, 0.0f, 0.0f};
    glm::uvec4 frame_indices{0u, 0u, 0u, 0u};
    glm::uvec4 view_flags{0u, 0u, 0u, 0u};
    glm::vec4 light_direction_intensity{0.0f, 1.0f, 0.0f, 0.0f};
    glm::vec4 light_color_exposure{1.0f, 1.0f, 1.0f, 1.0f};
    glm::vec4 ibl_factors{1.0f, 1.0f, 1.0f, 0.0f};
    glm::vec4 debug_overlay_params{0.0f, 0.65f, 0.0f, 0.0f};
    glm::vec4 debug_pick_marker{0.0f, 0.0f, 0.0f, 1.0f};
    glm::mat4 shadow_view_projection{1.0f};
    glm::vec4 shadow_params{0.0f, 0.0015f, 0.0f, 1.0f / static_cast<float>(kShadowMapSize)};
    glm::vec4 light_counts{0.0f, 0.0f, 0.0f, 0.0f};
    std::array<glm::vec4, kMaxPointLights> point_light_position_intensity{};
    std::array<glm::vec4, kMaxPointLights> point_light_color_range{};
    std::array<glm::vec4, kMaxSpotLights> spot_light_position_intensity{};
    std::array<glm::vec4, kMaxSpotLights> spot_light_direction_range{};
    std::array<glm::vec4, kMaxSpotLights> spot_light_color_cones{};
    std::array<glm::vec4, kMaxSpotLights> spot_light_cone_params{};
    std::array<glm::vec4, 9> irradiance_sh{};
};

static_assert(sizeof(SceneGpuParams) % 16 == 0);

struct MaterialGpuParams {
    glm::vec4 base_color_factor{1.0f};
    glm::vec4 emissive_factor_normal_scale{0.0f, 0.0f, 0.0f, 1.0f};
    glm::vec4 material_factors{0.0f, 1.0f, 1.0f, kDefaultMaterialAlphaCutoff};
    glm::vec4 material_flags{0.0f};
};

struct ShadowRenderParams {
    glm::mat4 view_projection{1.0f};
    glm::vec4 params{0.0f, 0.0015f, 0.0f, 1.0f / static_cast<float>(kShadowMapSize)};
};

} // namespace luna::render_flow::default_scene_detail

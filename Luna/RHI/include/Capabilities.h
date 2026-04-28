#ifndef LUNA_RHI_CAPABILITIES_H
#define LUNA_RHI_CAPABILITIES_H

#include <Instance.h>

namespace luna::RHI {

struct RHIConventions {
    bool requires_projection_y_flip{false};
    bool imgui_clip_top_y_is_negative_one{false};
    bool imgui_render_target_requires_uv_y_flip{true};
    bool scene_pick_y_matches_display_y{false};
};

struct RHICapabilities {
    BackendType backend_type{BackendType::Auto};
    bool supports_default_render_flow{false};
    bool supports_imgui{false};
    bool supports_scene_pick_readback{false};
    bool supports_gpu_timestamp{false};
    bool gpu_timestamp_uses_disjoint_query{false};
    bool supports_graphics_pipeline{false};
    bool supports_compute_pipeline{false};
    bool supports_sampled_texture{false};
    bool supports_storage_texture{false};
    bool supports_color_attachment{false};
    bool supports_depth_attachment{false};
    bool supports_uniform_buffer{false};
    bool supports_storage_buffer{false};
    bool supports_sampler{false};
    RHIConventions conventions{};
};

[[nodiscard]] inline RHICapabilities makeCapabilitiesForBackend(BackendType backend_type) noexcept
{
    RHICapabilities capabilities{};
    capabilities.backend_type = backend_type;

    switch (backend_type) {
        case BackendType::Vulkan:
            capabilities.supports_default_render_flow = true;
            capabilities.supports_imgui = true;
            capabilities.supports_scene_pick_readback = true;
            capabilities.supports_gpu_timestamp = true;
            capabilities.gpu_timestamp_uses_disjoint_query = false;
            capabilities.supports_graphics_pipeline = true;
            capabilities.supports_compute_pipeline = true;
            capabilities.supports_sampled_texture = true;
            capabilities.supports_storage_texture = true;
            capabilities.supports_color_attachment = true;
            capabilities.supports_depth_attachment = true;
            capabilities.supports_uniform_buffer = true;
            capabilities.supports_storage_buffer = true;
            capabilities.supports_sampler = true;
            capabilities.conventions.requires_projection_y_flip = true;
            capabilities.conventions.imgui_clip_top_y_is_negative_one = true;
            capabilities.conventions.imgui_render_target_requires_uv_y_flip = false;
            capabilities.conventions.scene_pick_y_matches_display_y = false;
            break;
        case BackendType::DirectX12:
            capabilities.supports_default_render_flow = true;
            capabilities.supports_imgui = true;
            capabilities.supports_scene_pick_readback = true;
            capabilities.supports_gpu_timestamp = true;
            capabilities.gpu_timestamp_uses_disjoint_query = false;
            capabilities.supports_graphics_pipeline = true;
            capabilities.supports_compute_pipeline = true;
            capabilities.supports_sampled_texture = true;
            capabilities.supports_storage_texture = true;
            capabilities.supports_color_attachment = true;
            capabilities.supports_depth_attachment = true;
            capabilities.supports_uniform_buffer = true;
            capabilities.supports_storage_buffer = true;
            capabilities.supports_sampler = true;
            capabilities.conventions.requires_projection_y_flip = false;
            capabilities.conventions.imgui_clip_top_y_is_negative_one = true;
            capabilities.conventions.imgui_render_target_requires_uv_y_flip = true;
            capabilities.conventions.scene_pick_y_matches_display_y = false;
            break;
        case BackendType::DirectX11:
            capabilities.supports_default_render_flow = true;
            capabilities.supports_imgui = true;
            capabilities.supports_scene_pick_readback = true;
            capabilities.supports_gpu_timestamp = true;
            capabilities.gpu_timestamp_uses_disjoint_query = true;
            capabilities.supports_graphics_pipeline = true;
            capabilities.supports_compute_pipeline = true;
            capabilities.supports_sampled_texture = true;
            capabilities.supports_storage_texture = true;
            capabilities.supports_color_attachment = true;
            capabilities.supports_depth_attachment = true;
            capabilities.supports_uniform_buffer = true;
            capabilities.supports_storage_buffer = true;
            capabilities.supports_sampler = true;
            capabilities.conventions.requires_projection_y_flip = false;
            capabilities.conventions.imgui_clip_top_y_is_negative_one = false;
            capabilities.conventions.imgui_render_target_requires_uv_y_flip = true;
            capabilities.conventions.scene_pick_y_matches_display_y = true;
            break;
        case BackendType::OpenGL:
        case BackendType::OpenGLES:
            capabilities.supports_imgui = true;
            capabilities.supports_graphics_pipeline = true;
            capabilities.supports_sampled_texture = true;
            capabilities.supports_color_attachment = true;
            capabilities.supports_depth_attachment = true;
            capabilities.supports_uniform_buffer = true;
            capabilities.supports_sampler = true;
            capabilities.conventions.requires_projection_y_flip = false;
            capabilities.conventions.imgui_clip_top_y_is_negative_one = false;
            capabilities.conventions.imgui_render_target_requires_uv_y_flip = true;
            capabilities.conventions.scene_pick_y_matches_display_y = false;
            break;
        case BackendType::Metal:
        case BackendType::WebGPU:
        case BackendType::Auto:
        default:
            break;
    }

    return capabilities;
}

} // namespace luna::RHI

#endif

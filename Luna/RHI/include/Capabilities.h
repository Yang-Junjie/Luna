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
            capabilities.conventions.requires_projection_y_flip = false;
            capabilities.conventions.imgui_clip_top_y_is_negative_one = false;
            capabilities.conventions.imgui_render_target_requires_uv_y_flip = true;
            capabilities.conventions.scene_pick_y_matches_display_y = true;
            break;
        case BackendType::OpenGL:
        case BackendType::OpenGLES:
            capabilities.supports_imgui = true;
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

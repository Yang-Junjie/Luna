#pragma once

#include "Renderer/RenderGraphBuilder.h"
#include "Renderer/RenderViewState.h"

#include <Capabilities.h>
#include <Core.h>
#include <filesystem>
#include <glm/vec4.hpp>
#include <Instance.h>

namespace luna::RHI {
class Device;
class ShaderCompiler;
} // namespace luna::RHI

namespace luna {

enum class RenderDebugViewMode : uint8_t {
    None,
    Velocity,
    HistoryValidity,
    ShadowCascades,
};

struct SceneShaderPaths {
    std::filesystem::path geometry_vertex_path;
    std::filesystem::path geometry_fragment_path;
    std::filesystem::path shadow_vertex_path;
    std::filesystem::path shadow_fragment_path;
    std::filesystem::path lighting_vertex_path;
    std::filesystem::path lighting_fragment_path;
    std::filesystem::path transparent_composite_path;
    std::filesystem::path environment_ibl_path;
    std::filesystem::path procedural_sky_path;

    [[nodiscard]] bool isComplete() const
    {
        return !geometry_vertex_path.empty() && !geometry_fragment_path.empty() && !shadow_vertex_path.empty() &&
               !shadow_fragment_path.empty() && !lighting_vertex_path.empty() && !lighting_fragment_path.empty() &&
               !transparent_composite_path.empty() && !environment_ibl_path.empty() && !procedural_sky_path.empty();
    }
};

struct SceneRenderContext {
    luna::RHI::Ref<luna::RHI::Device> device;
    luna::RHI::Ref<luna::RHI::ShaderCompiler> compiler;
    luna::RHI::BackendType backend_type{luna::RHI::BackendType::Auto};
    luna::RHI::RHICapabilities capabilities{luna::RHI::makeCapabilitiesForBackend(luna::RHI::BackendType::Auto)};
    RenderGraphTextureHandle color_target;
    RenderGraphTextureHandle depth_target;
    RenderGraphTextureHandle pick_target;
    luna::RHI::Format color_format{luna::RHI::Format::UNDEFINED};
    glm::vec4 clear_color{0.10f, 0.10f, 0.12f, 1.0f};
    bool show_pick_debug_visualization{false};
    uint32_t debug_pick_pixel_x{0};
    uint32_t debug_pick_pixel_y{0};
    bool show_pick_debug_marker{false};
    RenderGraphTextureHandle debug_target;
    luna::RHI::Format debug_format{luna::RHI::Format::UNDEFINED};
    luna::RenderDebugViewMode debug_view_mode{luna::RenderDebugViewMode::None};
    float debug_velocity_scale{20.0f};
    bool temporal_jitter_enabled{false};
    uint32_t framebuffer_width{0};
    uint32_t framebuffer_height{0};

    [[nodiscard]] bool isValid() const
    {
        return device && color_target.isValid() && depth_target.isValid() && framebuffer_width > 0 &&
               framebuffer_height > 0;
    }
};

} // namespace luna

namespace luna::render_flow {

enum class RenderFeatureHistoryInvalidationFlags : uint32_t {
    None = 0,
    FirstFrame = 1 << 0,
    Resize = 1 << 1,
    SceneOutputChanged = 1 << 2,
    DeviceChanged = 1 << 3,
    BackendChanged = 1 << 4,
    CameraCut = 1 << 5,
};

inline RenderFeatureHistoryInvalidationFlags operator|(RenderFeatureHistoryInvalidationFlags lhs,
                                                       RenderFeatureHistoryInvalidationFlags rhs) noexcept
{
    return static_cast<RenderFeatureHistoryInvalidationFlags>(static_cast<uint32_t>(lhs) | static_cast<uint32_t>(rhs));
}

inline RenderFeatureHistoryInvalidationFlags& operator|=(RenderFeatureHistoryInvalidationFlags& lhs,
                                                         RenderFeatureHistoryInvalidationFlags rhs) noexcept
{
    lhs = lhs | rhs;
    return lhs;
}

inline bool operator&(RenderFeatureHistoryInvalidationFlags lhs, RenderFeatureHistoryInvalidationFlags rhs) noexcept
{
    return (static_cast<uint32_t>(lhs) & static_cast<uint32_t>(rhs)) != 0;
}

struct RenderFeatureFrameContext {
    luna::RHI::Ref<luna::RHI::Device> device;
    luna::RHI::BackendType backend_type{luna::RHI::BackendType::Auto};
    luna::RHI::RHICapabilities capabilities{luna::RHI::makeCapabilitiesForBackend(luna::RHI::BackendType::Auto)};
    uint64_t frame_index{0};
    uint32_t framebuffer_width{0};
    uint32_t framebuffer_height{0};
    luna::RenderViewFrameState view;
    RenderFeatureHistoryInvalidationFlags history_invalidation_flags{RenderFeatureHistoryInvalidationFlags::None};

    [[nodiscard]] bool historyInvalidated() const noexcept
    {
        return history_invalidation_flags != RenderFeatureHistoryInvalidationFlags::None;
    }

    [[nodiscard]] bool hasHistoryInvalidation(RenderFeatureHistoryInvalidationFlags reason) const noexcept
    {
        return history_invalidation_flags & reason;
    }
};

} // namespace luna::render_flow

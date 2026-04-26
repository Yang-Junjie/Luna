#pragma once

#include "Renderer/RenderGraphBuilder.h"

#include <Core.h>
#include <Instance.h>
#include <filesystem>
#include <glm/vec4.hpp>

namespace luna::RHI {
class Device;
class ShaderCompiler;
} // namespace luna::RHI

namespace luna {

struct SceneShaderPaths {
    std::filesystem::path geometry_vertex_path;
    std::filesystem::path geometry_fragment_path;
    std::filesystem::path shadow_vertex_path;
    std::filesystem::path shadow_fragment_path;
    std::filesystem::path lighting_vertex_path;
    std::filesystem::path lighting_fragment_path;

    [[nodiscard]] bool isComplete() const
    {
        return !geometry_vertex_path.empty() && !geometry_fragment_path.empty() && !shadow_vertex_path.empty() &&
               !shadow_fragment_path.empty() && !lighting_vertex_path.empty() && !lighting_fragment_path.empty();
    }
};

struct SceneRenderContext {
    luna::RHI::Ref<luna::RHI::Device> device;
    luna::RHI::Ref<luna::RHI::ShaderCompiler> compiler;
    luna::RHI::BackendType backend_type{luna::RHI::BackendType::Vulkan};
    RenderGraphTextureHandle color_target;
    RenderGraphTextureHandle depth_target;
    RenderGraphTextureHandle pick_target;
    luna::RHI::Format color_format{luna::RHI::Format::UNDEFINED};
    glm::vec4 clear_color{0.10f, 0.10f, 0.12f, 1.0f};
    bool show_pick_debug_visualization{false};
    uint32_t debug_pick_pixel_x{0};
    uint32_t debug_pick_pixel_y{0};
    bool show_pick_debug_marker{false};
    uint32_t framebuffer_width{0};
    uint32_t framebuffer_height{0};

    [[nodiscard]] bool isValid() const
    {
        return device && color_target.isValid() && depth_target.isValid() && framebuffer_width > 0 && framebuffer_height > 0;
    }
};

} // namespace luna





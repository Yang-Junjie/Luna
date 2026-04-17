#pragma once

#include "Imgui/ImGuiContext.h"

#include <Core.h>

namespace luna::rhi {

struct ImGuiRenderPass {
    static void Render(luna::RHI::CommandBufferEncoder& command_buffer,
                       const luna::RHI::Ref<luna::RHI::Texture>& color_target,
                       uint32_t framebuffer_width,
                       uint32_t framebuffer_height)
    {
        ImGuiVulkanContext::RenderFrame(command_buffer, color_target, framebuffer_width, framebuffer_height);
    }
};

} // namespace luna::rhi

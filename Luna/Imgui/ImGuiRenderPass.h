#pragma once

#include "Imgui/ImGuiContext.h"

#include <Core.h>

namespace luna {

struct ImGuiRenderPass {
    static void Render(luna::RHI::CommandBufferEncoder& command_buffer,
                       const luna::RHI::Ref<luna::RHI::Texture>& color_target,
                       uint32_t framebuffer_width,
                       uint32_t framebuffer_height,
                       uint32_t frame_index)
    {
        ImGuiRhiContext::RenderDrawData(
            command_buffer, color_target, framebuffer_width, framebuffer_height, frame_index);
    }
};

} // namespace luna

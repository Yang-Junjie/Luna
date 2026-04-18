#pragma once

#include "imgui.h"

#include <Core.h>

namespace luna::RHI {
class TextureView;
class CommandBufferEncoder;
class Sampler;
class Texture;
} // namespace luna::RHI

namespace luna {
class Renderer;
}

namespace luna::rhi {

class ImGuiRhiContext {
public:
    static bool Init(luna::Renderer& renderer);
    static void Destroy();
    static void StartFrame();
    static void RenderDrawData(luna::RHI::CommandBufferEncoder& command_buffer,
                               const luna::RHI::Ref<luna::RHI::Texture>& color_target,
                               uint32_t framebuffer_width,
                               uint32_t framebuffer_height,
                               uint32_t frame_index);
    static ImTextureID GetTextureId(const luna::RHI::Ref<luna::RHI::Texture>& texture);
    static ImTextureID GetTextureId(const luna::RHI::Ref<luna::RHI::TextureView>& view,
                                    const luna::RHI::Ref<luna::RHI::Sampler>& sampler = {});
    static void EndFrame();
    static void NotifyFrameResourcesChanged(uint32_t frames_in_flight);
};

} // namespace luna::rhi

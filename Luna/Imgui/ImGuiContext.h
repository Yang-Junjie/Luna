#pragma once

#include "imgui.h"

#include <Core.h>

namespace Cacao {
class CacaoTextureView;
class CommandBufferEncoder;
class Sampler;
class Texture;
} // namespace Cacao

namespace luna {
class VulkanRenderer;
}

namespace luna::rhi {

class ImGuiVulkanContext {
public:
    static bool Init(luna::VulkanRenderer& renderer);
    static void Destroy();
    static void StartFrame();
    static void RenderFrame(Cacao::CommandBufferEncoder& command_buffer,
                            const Cacao::Ref<Cacao::Texture>& color_target,
                            uint32_t framebuffer_width,
                            uint32_t framebuffer_height);
    static ImTextureID GetTextureId(const Cacao::Ref<Cacao::Texture>& texture);
    static ImTextureID GetTextureId(const Cacao::Ref<Cacao::CacaoTextureView>& view,
                                    const Cacao::Ref<Cacao::Sampler>& sampler = {});
    static void EndFrame();
    static void NotifySwapchainChanged(uint32_t image_count);
};

} // namespace luna::rhi

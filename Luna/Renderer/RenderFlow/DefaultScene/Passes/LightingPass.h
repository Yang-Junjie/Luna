#pragma once

#include "Renderer/RenderFlow/DefaultScene/SharedState.h"
#include "Renderer/RenderFlow/LightingExtensionInputs.h"
#include "Renderer/RenderFlow/RenderPass.h"

namespace luna::render_flow::default_scene {

class LightingPass final : public IRenderPass {
public:
    explicit LightingPass(PassSharedState& state);

    [[nodiscard]] const char* name() const noexcept override;
    [[nodiscard]] std::span<const RenderPassResourceUsage> resourceUsages() const noexcept override;
    void setup(RenderPassContext& context) override;

private:
    void execute(RenderGraphRasterPassContext& pass_context,
                 const SceneRenderContext& context,
                 GBufferTextures gbuffer,
                 RenderGraphTextureHandle shadow_map,
                 RenderGraphTextureHandle pick_texture,
                 LightingExtensionInputSet extension_inputs);
    void executeDebugView(RenderGraphRasterPassContext& pass_context,
                          GBufferTextures gbuffer,
                          RenderGraphTextureHandle pick_texture);

private:
    PassSharedState* m_state{nullptr};
};

} // namespace luna::render_flow::default_scene

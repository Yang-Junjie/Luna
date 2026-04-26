#pragma once

#include "Renderer/RenderFlow/DefaultScene/SharedState.h"
#include "Renderer/RenderFlow/RenderPass.h"

namespace luna::render_flow::default_scene {

class LightingPass final : public IRenderPass {
public:
    explicit LightingPass(PassSharedState& state);

    [[nodiscard]] const char* name() const noexcept override;
    void setup(RenderPassContext& context) override;

private:
    struct ExtensionInputTextures {
        RenderGraphTextureHandle ambient_occlusion;
        RenderGraphTextureHandle reflection;
        RenderGraphTextureHandle indirect_diffuse;
        RenderGraphTextureHandle indirect_specular;
    };

    void execute(RenderGraphRasterPassContext& pass_context,
                 const SceneRenderContext& context,
                 GBufferTextures gbuffer,
                 RenderGraphTextureHandle shadow_map,
                 RenderGraphTextureHandle pick_texture,
                 ExtensionInputTextures extension_inputs);

private:
    PassSharedState* m_state{nullptr};
};

} // namespace luna::render_flow::default_scene

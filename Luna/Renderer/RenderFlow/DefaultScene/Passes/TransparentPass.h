#pragma once

#include "Renderer/RenderFlow/DefaultScene/SharedState.h"
#include "Renderer/RenderFlow/RenderPass.h"

namespace luna::render_flow::default_scene {

class TransparentPass final : public IRenderPass {
public:
    explicit TransparentPass(PassSharedState& state);

    [[nodiscard]] const char* name() const noexcept override;
    [[nodiscard]] std::span<const RenderPassResourceUsage> resourceUsages() const noexcept override;
    void setup(RenderPassContext& context) override;

private:
    void execute(RenderGraphRasterPassContext& pass_context, const SceneRenderContext& context);
    void composite(RenderGraphRasterPassContext& pass_context, RenderGraphTextureHandle transparent_color);

private:
    PassSharedState* m_state{nullptr};
};

} // namespace luna::render_flow::default_scene

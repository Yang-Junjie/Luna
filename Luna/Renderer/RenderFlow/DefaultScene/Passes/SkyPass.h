#pragma once

#include "Renderer/RenderFlow/DefaultScene/SharedState.h"
#include "Renderer/RenderFlow/RenderPass.h"

namespace luna::render_flow::default_scene {

class SkyPass final : public IRenderPass {
public:
    explicit SkyPass(PassSharedState& state);

    [[nodiscard]] const char* name() const noexcept override;
    [[nodiscard]] std::span<const RenderPassResourceUsage> resourceUsages() const noexcept override;
    void setup(RenderPassContext& context) override;

private:
    void execute(RenderGraphRasterPassContext& pass_context,
                 RenderGraphTextureHandle base_color,
                 RenderGraphTextureHandle normal_metallic,
                 RenderGraphTextureHandle pick_texture);

private:
    PassSharedState* m_state{nullptr};
};

} // namespace luna::render_flow::default_scene

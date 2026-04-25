#pragma once

#include "Renderer/Material.h"
#include "Renderer/RenderFlow/RenderFlow.h"
#include "Renderer/RenderFlow/RenderFlowBuilder.h"
#include "Renderer/RenderFlow/RenderPass.h"
#include "Renderer/RenderFlow/DefaultScene/Passes.h"
#include "Renderer/RenderFlow/DefaultScene/DrawQueue.h"
#include "Renderer/RenderFlow/DefaultScene/ResourceManager.h"

#include <memory>
#include <vector>

namespace luna {

class DefaultRenderFlow final : public IRenderFlow {
public:
    DefaultRenderFlow();
    ~DefaultRenderFlow() override;

    void render(RenderFlowContext& context) override;
    void shutdown();

    [[nodiscard]] render_flow::RenderFlowBuilder& builder() noexcept;
    [[nodiscard]] const render_flow::RenderFlowBuilder& builder() const noexcept;

private:
    render_flow::default_scene::DrawQueue m_draw_queue{};
    render_flow::default_scene::ResourceManager m_resources{};
    Material m_default_material{};
    render_flow::default_scene::DefaultScenePassSharedState m_scene_state;
    render_flow::RenderFlowBuilder m_builder;
};

} // namespace luna







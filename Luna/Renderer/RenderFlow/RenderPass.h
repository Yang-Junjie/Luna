#pragma once

#include "Renderer/RenderFlow/RenderFlowTypes.h"
#include "Renderer/RenderWorld/RenderWorld.h"

namespace luna {
class RenderGraphBuilder;
} // namespace luna

namespace luna::render_flow {

class RenderPassContext {
public:
    RenderPassContext(RenderGraphBuilder& graph, const RenderWorld& world, const SceneRenderContext& scene_context);

    [[nodiscard]] RenderGraphBuilder& graph() const noexcept;
    [[nodiscard]] const RenderWorld& world() const noexcept;
    [[nodiscard]] const SceneRenderContext& sceneContext() const noexcept;

private:
    RenderGraphBuilder* m_graph{nullptr};
    const RenderWorld* m_world{nullptr};
    const SceneRenderContext* m_scene_context{nullptr};
};

class IRenderPass {
public:
    virtual ~IRenderPass() = default;

    [[nodiscard]] virtual const char* name() const noexcept = 0;
    virtual void setup(RenderPassContext& context) = 0;
};

} // namespace luna::render_flow




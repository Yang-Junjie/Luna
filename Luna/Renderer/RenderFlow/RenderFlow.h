#pragma once

#include "Renderer/RenderGraphBuilder.h"
#include "Renderer/RenderFlow/RenderFlowTypes.h"
#include "Renderer/RenderWorld/RenderWorld.h"

namespace luna {

class RenderFlowContext {
public:
    RenderFlowContext(RenderGraphBuilder& graph,
                      const RenderWorld& world,
                      const SceneRenderContext& scene_context);

    RenderGraphBuilder& graph() const;
    const RenderWorld& world() const;
    const SceneRenderContext& sceneContext() const;

private:
    RenderGraphBuilder* m_graph{nullptr};
    const RenderWorld* m_world{nullptr};
    const SceneRenderContext* m_scene_context{nullptr};
};

class IRenderFlow {
public:
    virtual ~IRenderFlow() = default;

    virtual void render(RenderFlowContext& context) = 0;
};

} // namespace luna






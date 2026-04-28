#pragma once

#include "Renderer/RenderGraphBuilder.h"
#include "Renderer/RenderFlow/RenderFlowTypes.h"
#include "Renderer/RenderWorld/RenderWorld.h"

namespace luna {

class RenderFlowContext {
public:
    RenderFlowContext(RenderGraphBuilder& graph,
                      const RenderWorld& world,
                      const SceneRenderContext& scene_context,
                      render_flow::RenderFeatureFrameContext feature_frame_context = {});

    RenderGraphBuilder& graph() const;
    const RenderWorld& world() const;
    const SceneRenderContext& sceneContext() const;
    const render_flow::RenderFeatureFrameContext& featureFrameContext() const;

private:
    RenderGraphBuilder* m_graph{nullptr};
    const RenderWorld* m_world{nullptr};
    const SceneRenderContext* m_scene_context{nullptr};
    render_flow::RenderFeatureFrameContext m_feature_frame_context{};
};

class IRenderFlow {
public:
    virtual ~IRenderFlow() = default;

    virtual void render(RenderFlowContext& context) = 0;
    virtual bool commitFrame()
    {
        return false;
    }
};

} // namespace luna






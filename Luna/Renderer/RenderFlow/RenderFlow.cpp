#include "Renderer/RenderFlow/RenderFlow.h"

#include <utility>

namespace luna {

RenderFlowContext::RenderFlowContext(RenderGraphBuilder& graph,
                                     const RenderWorld& world,
                                     const SceneRenderContext& scene_context,
                                     render_flow::RenderFeatureFrameContext feature_frame_context)
    : m_graph(&graph),
      m_world(&world),
      m_scene_context(&scene_context),
      m_feature_frame_context(std::move(feature_frame_context))
{}

RenderGraphBuilder& RenderFlowContext::graph() const
{
    return *m_graph;
}

const RenderWorld& RenderFlowContext::world() const
{
    return *m_world;
}

const SceneRenderContext& RenderFlowContext::sceneContext() const
{
    return *m_scene_context;
}

const render_flow::RenderFeatureFrameContext& RenderFlowContext::featureFrameContext() const
{
    return m_feature_frame_context;
}

} // namespace luna






#include "Renderer/RenderFlow/RenderFlow.h"

namespace luna {

RenderFlowContext::RenderFlowContext(RenderGraphBuilder& graph,
                                     const RenderWorld& world,
                                     const SceneRenderContext& scene_context)
    : m_graph(&graph), m_world(&world), m_scene_context(&scene_context)
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

} // namespace luna






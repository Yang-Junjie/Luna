#include "Renderer/RenderFlow/RenderPass.h"

#include "Renderer/RenderGraphBuilder.h"

namespace luna::render_flow {

RenderPassContext::RenderPassContext(RenderGraphBuilder& graph,
                                     const RenderWorld& world,
                                     const SceneRenderContext& scene_context)
    : m_graph(&graph), m_world(&world), m_scene_context(&scene_context)
{}

RenderGraphBuilder& RenderPassContext::graph() const noexcept
{
    return *m_graph;
}

const RenderWorld& RenderPassContext::world() const noexcept
{
    return *m_world;
}

const SceneRenderContext& RenderPassContext::sceneContext() const noexcept
{
    return *m_scene_context;
}

} // namespace luna::render_flow




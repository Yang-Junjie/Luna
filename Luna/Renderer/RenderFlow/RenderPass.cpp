#include "Renderer/RenderFlow/RenderPass.h"
#include "Renderer/RenderGraphBuilder.h"

namespace luna::render_flow {

void RenderPassBlackboard::set(RenderResourceKey<RenderGraphTextureHandle> key, RenderGraphTextureHandle handle)
{
    m_textures[std::string(key.name)] = handle;
}

std::optional<RenderGraphTextureHandle> RenderPassBlackboard::get(RenderResourceKey<RenderGraphTextureHandle> key) const
{
    const auto iterator = m_textures.find(std::string(key.name));
    if (iterator == m_textures.end()) {
        return std::nullopt;
    }

    return iterator->second;
}

bool RenderPassBlackboard::has(RenderResourceKey<RenderGraphTextureHandle> key) const
{
    return m_textures.find(std::string(key.name)) != m_textures.end();
}

void RenderPassBlackboard::clear() noexcept
{
    m_textures.clear();
}

RenderPassContext::RenderPassContext(RenderGraphBuilder& graph,
                                     const RenderWorld& world,
                                     const SceneRenderContext& scene_context,
                                     RenderPassBlackboard& blackboard)
    : m_graph(&graph),
      m_world(&world),
      m_scene_context(&scene_context),
      m_blackboard(&blackboard)
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

RenderPassBlackboard& RenderPassContext::blackboard() const noexcept
{
    return *m_blackboard;
}

} // namespace luna::render_flow

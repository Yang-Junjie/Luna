#include "Renderer/RenderFlow/RenderPass.h"

#include "Renderer/RenderGraphBuilder.h"

namespace luna::render_flow {

void RenderPassBlackboard::setTexture(std::string_view name, RenderGraphTextureHandle handle)
{
    m_textures[std::string(name)] = handle;
}

std::optional<RenderGraphTextureHandle> RenderPassBlackboard::getTexture(std::string_view name) const
{
    const auto iterator = m_textures.find(std::string(name));
    if (iterator == m_textures.end()) {
        return std::nullopt;
    }

    return iterator->second;
}

bool RenderPassBlackboard::hasTexture(std::string_view name) const
{
    return m_textures.find(std::string(name)) != m_textures.end();
}

void RenderPassBlackboard::clear() noexcept
{
    m_textures.clear();
}

RenderPassContext::RenderPassContext(RenderGraphBuilder& graph,
                                     const RenderWorld& world,
                                     const SceneRenderContext& scene_context,
                                     RenderPassBlackboard& blackboard)
    : m_graph(&graph), m_world(&world), m_scene_context(&scene_context), m_blackboard(&blackboard)
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




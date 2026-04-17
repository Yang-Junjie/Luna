#include "Renderer/RenderGraph.h"

#include <algorithm>

namespace luna::rhi {

RenderGraph::RenderGraph(PassList passes)
    : m_passes(std::move(passes))
{}

void RenderGraph::addPass(RenderGraphPass pass)
{
    m_passes.push_back(std::move(pass));
}

void RenderGraph::execute(luna::RHI::CommandBufferEncoder& encoder) const
{
    for (const auto& pass : m_passes) {
        if (pass.Execute) {
            pass.Execute(encoder);
        }
    }
}

const RenderGraphPass* RenderGraph::findPass(std::string_view name) const
{
    const auto it = std::find_if(m_passes.begin(), m_passes.end(), [name](const RenderGraphPass& pass) {
        return pass.Name == name;
    });
    return it == m_passes.end() ? nullptr : &(*it);
}

RenderGraphPass* RenderGraph::findPass(std::string_view name)
{
    const auto it = std::find_if(m_passes.begin(), m_passes.end(), [name](const RenderGraphPass& pass) {
        return pass.Name == name;
    });
    return it == m_passes.end() ? nullptr : &(*it);
}

} // namespace luna::rhi

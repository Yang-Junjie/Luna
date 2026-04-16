#include "Renderer/RenderGraphBuilder.h"

namespace luna::rhi {

RenderGraphBuilder& RenderGraphBuilder::AddRenderPass(const std::string& name, ExecuteCallback execute)
{
    m_passes.push_back(RenderGraphPass{
        .Name = name,
        .Execute = std::move(execute),
    });
    return *this;
}

std::unique_ptr<RenderGraph> RenderGraphBuilder::Build()
{
    return std::make_unique<RenderGraph>(std::move(m_passes));
}

} // namespace luna::rhi

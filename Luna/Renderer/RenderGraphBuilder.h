#pragma once

#include "Renderer/RenderGraph.h"

#include <memory>
#include <string>

namespace luna::rhi {

class RenderGraphBuilder {
public:
    using ExecuteCallback = std::function<void(Cacao::CommandBufferEncoder&)>;

    RenderGraphBuilder& AddRenderPass(const std::string& name, ExecuteCallback execute);
    std::unique_ptr<RenderGraph> Build();

private:
    RenderGraph::PassList m_passes;
};

} // namespace luna::rhi

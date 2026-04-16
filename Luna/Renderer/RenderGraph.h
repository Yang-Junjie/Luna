#pragma once

#include <CommandBufferEncoder.h>

#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace luna::rhi {

struct RenderGraphPass {
    std::string Name;
    std::function<void(Cacao::CommandBufferEncoder&)> Execute;
};

class RenderGraph {
public:
    using PassList = std::vector<RenderGraphPass>;

    RenderGraph() = default;
    explicit RenderGraph(PassList passes);

    void addPass(RenderGraphPass pass);
    void execute(Cacao::CommandBufferEncoder& encoder) const;

    const RenderGraphPass* findPass(std::string_view name) const;
    RenderGraphPass* findPass(std::string_view name);

    const PassList& passes() const
    {
        return m_passes;
    }

private:
    PassList m_passes;
};

} // namespace luna::rhi

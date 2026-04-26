#pragma once

#include "Renderer/RenderFlow/RenderPass.h"

#include <string>

namespace luna::render_flow {

class RenderSlotPass final : public IRenderPass {
public:
    explicit RenderSlotPass(std::string name);

    [[nodiscard]] const char* name() const noexcept override;
    void setup(RenderPassContext& context) override;

private:
    std::string m_name;
};

} // namespace luna::render_flow

#include "Renderer/RenderFlow/RenderSlotPass.h"

#include <utility>

namespace luna::render_flow {

RenderSlotPass::RenderSlotPass(std::string name) : m_name(std::move(name)) {}

const char* RenderSlotPass::name() const noexcept
{
    return m_name.c_str();
}

void RenderSlotPass::setup(RenderPassContext& context)
{
    (void) context;
}

} // namespace luna::render_flow

#pragma once

#include "Renderer/RenderPipeline.h"
#include "SwapchainLabState.h"

#include <memory>

namespace swapchain_lab {

class RhiSwapchainLabRenderPipeline final : public luna::IRenderPipeline {
public:
    explicit RhiSwapchainLabRenderPipeline(std::shared_ptr<State> state);

    bool init(luna::IRHIDevice& device) override;
    void shutdown(luna::IRHIDevice& device) override;
    bool render(luna::IRHIDevice& device, const luna::FrameContext& frameContext) override;

private:
    std::shared_ptr<State> m_state;
    luna::IRHISwapchain* m_swapchain = nullptr;
};

} // namespace swapchain_lab

#include "SwapchainLabPipeline.h"

#include "RHI/CommandContext.h"

#include <sstream>
#include <utility>

namespace swapchain_lab {

RhiSwapchainLabRenderPipeline::RhiSwapchainLabRenderPipeline(std::shared_ptr<State> state)
    : m_state(std::move(state))
{}

bool RhiSwapchainLabRenderPipeline::init(luna::IRHIDevice& device)
{
    m_swapchain = device.getPrimarySwapchain();
    if (m_state != nullptr) {
        m_state->observedState = m_swapchain != nullptr ? m_swapchain->getState() : device.getSwapchainState();
    }
    return true;
}

void RhiSwapchainLabRenderPipeline::shutdown(luna::IRHIDevice& device)
{
    (void)device;
    m_swapchain = nullptr;
}

bool RhiSwapchainLabRenderPipeline::render(luna::IRHIDevice& device, const luna::FrameContext& frameContext)
{
    if (m_state == nullptr || frameContext.commandContext == nullptr || !frameContext.backbuffer.isValid()) {
        return false;
    }

    if (m_state->recreateSwapchainRequested) {
        m_state->recreateSwapchainRequested = false;
        const luna::RHIResult recreateResult =
            m_swapchain != nullptr ? m_swapchain->requestRecreate() : luna::RHIResult::InvalidArgument;
        if (recreateResult == luna::RHIResult::Success) {
            m_state->status = "Swapchain recreate requested. Waiting for next acquire/present cycle.";
        } else {
            m_state->status = "Swapchain recreate request failed.";
        }
    }

    m_state->observedState = m_swapchain != nullptr ? m_swapchain->getState() : device.getSwapchainState();
    ++m_state->frameCounter;

    std::ostringstream status;
    status << "frame=" << m_state->frameCounter << " requested(vsync=" << (m_state->requestedDesc.vsync ? "true" : "false")
           << ", bufferCount=" << m_state->requestedDesc.bufferCount << ", format="
           << luna::to_string(m_state->requestedDesc.format) << ") actual(imageCount="
           << m_state->observedState.imageCount << ", format=" << luna::to_string(m_state->observedState.currentFormat)
           << ", deviceId=" << m_state->observedState.deviceId << ", swapchainId="
           << m_state->observedState.swapchainId << ", presentMode=" << m_state->observedState.presentModeName << ", vsyncActive="
           << (m_state->observedState.vsyncActive ? "true" : "false") << ")";
    m_state->status = status.str();

    return frameContext.commandContext->beginRendering({.width = frameContext.renderWidth,
                                                        .height = frameContext.renderHeight,
                                                        .colorAttachments = {{frameContext.backbuffer,
                                                                              frameContext.backbufferFormat,
                                                                              {0.05f, 0.07f, 0.09f, 1.0f}}}}) ==
               luna::RHIResult::Success &&
           frameContext.commandContext->endRendering() == luna::RHIResult::Success;
}

} // namespace swapchain_lab

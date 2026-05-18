#include "Renderer/SwapchainManager.h"

#include "Core/Log.h"

#include <Queue.h>
#include <Swapchain.h>
#include <Synchronization.h>

namespace luna {

bool AcquireResult::acquired() const noexcept
{
    return result == luna::RHI::Result::Success && image_index >= 0;
}

bool AcquireResult::requiresRecreate() const noexcept
{
    return result == luna::RHI::Result::OutOfDate || result == luna::RHI::Result::Suboptimal ||
           result == luna::RHI::Result::DeviceLost;
}

bool PresentResult::presented() const noexcept
{
    return result == luna::RHI::Result::Success;
}

bool PresentResult::requiresRecreate() const noexcept
{
    return result == luna::RHI::Result::OutOfDate || result == luna::RHI::Result::Suboptimal ||
           result == luna::RHI::Result::DeviceLost;
}

bool SwapchainManager::create(const SwapchainCreateRequest& request)
{
    m_factory.configure(request);
    releaseSwapchain();
    return m_factory.create(request.extent, m_resources);
}

AcquireResult SwapchainManager::acquireNextImage(FrameSyncHandle frame)
{
    if (!m_resources.swapchain || !m_resources.synchronization) {
        return {};
    }

    int acquired_image_index = -1;
    const auto result =
        m_resources.swapchain->AcquireNextImage(m_resources.synchronization, static_cast<int>(frame), acquired_image_index);
    if (result != luna::RHI::Result::Success || acquired_image_index < 0) {
        if (result == luna::RHI::Result::OutOfDate || result == luna::RHI::Result::Suboptimal) {
            requestResize();
        }
        return AcquireResult{
            .result = result,
            .image_index = acquired_image_index,
        };
    }

    m_resources.synchronization->ResetFrameFence(frame);
    return AcquireResult{
        .result = result,
        .image_index = acquired_image_index,
    };
}

PresentResult SwapchainManager::present(FrameSyncHandle frame)
{
    const auto& request = m_factory.request();
    if (!m_resources.swapchain || !m_resources.synchronization || !request.graphics_queue) {
        return {};
    }

    const auto result = m_resources.swapchain->Present(request.graphics_queue, m_resources.synchronization, frame);
    if (result == luna::RHI::Result::OutOfDate || result == luna::RHI::Result::Suboptimal) {
        requestResize();
    }
    return PresentResult{.result = result};
}

bool SwapchainManager::recreateIfRequested(const FramebufferExtentProvider& extent_provider)
{
    if (!m_resize_requested || !m_factory.isConfigured()) {
        return false;
    }

    const auto& request = m_factory.request();
    if (!request.device || !request.graphics_queue) {
        return false;
    }

    const luna::RHI::Extent2D requested_extent = extent_provider ? extent_provider() : luna::RHI::Extent2D{0, 0};
    if (requested_extent.width == 0 || requested_extent.height == 0) {
        LUNA_RENDERER_DEBUG("Resize requested while framebuffer is minimized; delaying swapchain recreation");
        return false;
    }

    LUNA_RENDERER_INFO("Recreating swapchain for framebuffer resize to {}x{}",
                       requested_extent.width,
                       requested_extent.height);
    request.graphics_queue->WaitIdle();
    if (request.before_recreate) {
        request.before_recreate();
    }
    releaseSwapchain();

    if (!m_factory.create(requested_extent, m_resources)) {
        return false;
    }

    m_resize_requested = false;
    LUNA_RENDERER_INFO("Swapchain recreation complete");
    return true;
}

void SwapchainManager::requestResize()
{
    m_resize_requested = true;
}

bool SwapchainManager::hasResources() const noexcept
{
    return m_resources.hasResources();
}

bool SwapchainManager::isReady() const noexcept
{
    return m_resources.isReady();
}

bool SwapchainManager::isResizeRequested() const noexcept
{
    return m_resize_requested;
}

void SwapchainManager::reset() noexcept
{
    releaseSwapchain();
    m_factory.reset();
    m_resize_requested = false;
}

void SwapchainManager::waitForFrame(FrameSyncHandle frame)
{
    if (m_resources.synchronization) {
        m_resources.synchronization->WaitForFrame(frame);
    }
}

const luna::RHI::Ref<luna::RHI::Swapchain>& SwapchainManager::swapchain() const noexcept
{
    return m_resources.swapchain;
}

const luna::RHI::Ref<luna::RHI::Synchronization>& SwapchainManager::synchronization() const noexcept
{
    return m_resources.synchronization;
}

luna::RHI::Format SwapchainManager::surfaceFormat() const noexcept
{
    return m_resources.surface_format;
}

uint32_t SwapchainManager::framesInFlight() const noexcept
{
    return m_resources.frames_in_flight;
}

uint32_t SwapchainManager::imageCount() const noexcept
{
    return m_resources.imageCount();
}

luna::RHI::Extent2D SwapchainManager::extent() const noexcept
{
    return m_resources.extent();
}

luna::RHI::Ref<luna::RHI::Texture> SwapchainManager::backBuffer(uint32_t image_index) const
{
    return m_resources.backBuffer(image_index);
}

void SwapchainManager::releaseSwapchain() noexcept
{
    m_resources.reset();
}

} // namespace luna

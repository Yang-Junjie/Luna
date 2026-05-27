#include "Core/Log.h"
#include "Renderer/RendererUtilities.h"
#include "Renderer/SwapchainFactory.h"

#include <algorithm>
#include <Builders.h>
#include <Device.h>
#include <stdexcept>
#include <Surface.h>
#include <Synchronization.h>

namespace luna {

bool SwapchainFactory::configure(const SwapchainCreateRequest& request)
{
    m_request = request;
    m_configured = true;
    return true;
}

bool SwapchainFactory::isConfigured() const noexcept
{
    return m_configured;
}

const SwapchainCreateRequest& SwapchainFactory::request() const noexcept
{
    return m_request;
}

void SwapchainFactory::reset() noexcept
{
    m_request = {};
    m_configured = false;
}

bool SwapchainFactory::create(RHI::Extent2D requested_extent, SwapchainResources& resources) const
{
    if (!m_configured || !m_request.device || !m_request.surface || !m_request.adapter) {
        LUNA_RENDERER_WARN("Cannot create swapchain because renderer device, surface, or adapter is missing");
        return false;
    }

    const auto capabilities = m_request.surface->GetCapabilities(m_request.adapter);
    const auto formats = m_request.surface->GetSupportedFormats(m_request.adapter);
    const auto supported_present_modes = m_request.surface->GetSupportedPresentModes(m_request.adapter);
    const auto surface_format = renderer_detail::chooseSurfaceFormat(formats);
    const auto selected_present_mode =
        renderer_detail::choosePresentMode(supported_present_modes, m_request.present_mode);

    const RHI::Extent2D clamped_extent{
        std::clamp(requested_extent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width),
        std::clamp(requested_extent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height),
    };
    LUNA_RENDERER_INFO("Creating swapchain: requested={}x{} clamped={}x{} surface_format={} ({})",
                       requested_extent.width,
                       requested_extent.height,
                       clamped_extent.width,
                       clamped_extent.height,
                       renderer_detail::formatToString(surface_format.format),
                       static_cast<int>(surface_format.format));

    uint32_t min_image_count = (std::max)(2u, capabilities.minImageCount);
    if (capabilities.maxImageCount != 0) {
        min_image_count = (std::min)(min_image_count, capabilities.maxImageCount);
    }

    if (selected_present_mode != m_request.present_mode) {
        LUNA_RENDERER_WARN("Requested present mode '{}' is unsupported; falling back to '{}'. Supported modes: {}",
                           renderer_detail::presentModeToString(m_request.present_mode),
                           renderer_detail::presentModeToString(selected_present_mode),
                           renderer_detail::describePresentModes(supported_present_modes));
    } else {
        LUNA_RENDERER_INFO("Using present mode '{}' (supported: {})",
                           renderer_detail::presentModeToString(selected_present_mode),
                           renderer_detail::describePresentModes(supported_present_modes));
    }

    resources.swapchain = m_request.device->CreateSwapchain(
        RHI::SwapchainBuilder()
            .SetExtent(clamped_extent)
            .SetFormat(surface_format.format)
            .SetColorSpace(surface_format.colorSpace)
            .SetPresentMode(selected_present_mode)
            .SetMinImageCount(min_image_count)
            .SetPreTransform(capabilities.currentTransform)
            .SetUsage(RHI::SwapchainUsageFlags::ColorAttachment | RHI::SwapchainUsageFlags::TransferDst)
            .SetSurface(m_request.surface)
            .Build());
    if (!resources.swapchain) {
        throw std::runtime_error("Failed to create swapchain");
    }

    resources.surface_format = surface_format.format;
    resources.frames_in_flight =
        (std::max)(1u, resources.swapchain->GetImageCount() > 1 ? resources.swapchain->GetImageCount() - 1 : 1u);
    resources.synchronization = m_request.device->CreateSynchronization(resources.frames_in_flight);
    if (!resources.synchronization) {
        throw std::runtime_error("Failed to create frame synchronization objects");
    }

    LUNA_RENDERER_INFO("Created swapchain {}x{} with {} image(s), {} frame(s) in flight",
                       clamped_extent.width,
                       clamped_extent.height,
                       resources.swapchain->GetImageCount(),
                       resources.frames_in_flight);
    return true;
}

} // namespace luna

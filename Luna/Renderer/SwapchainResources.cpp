#include "Renderer/SwapchainResources.h"

#include <Swapchain.h>

namespace luna {

bool SwapchainResources::hasResources() const noexcept
{
    return swapchain || synchronization;
}

bool SwapchainResources::isReady() const noexcept
{
    return swapchain && synchronization;
}

void SwapchainResources::reset() noexcept
{
    synchronization.reset();
    swapchain.reset();
    surface_format = RHI::Format::UNDEFINED;
    frames_in_flight = 0;
}

uint32_t SwapchainResources::imageCount() const noexcept
{
    return swapchain ? swapchain->GetImageCount() : 0;
}

RHI::Extent2D SwapchainResources::extent() const noexcept
{
    return swapchain ? swapchain->GetExtent() : RHI::Extent2D{0, 0};
}

RHI::Ref<RHI::Texture> SwapchainResources::backBuffer(uint32_t image_index) const
{
    return swapchain ? swapchain->GetBackBuffer(image_index) : nullptr;
}

} // namespace luna

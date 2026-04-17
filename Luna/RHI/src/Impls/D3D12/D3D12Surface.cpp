#include "Impls/D3D12/D3D12Instance.h"
#include "Impls/D3D12/D3D12Surface.h"

#include <Device.h>

namespace luna::RHI {
SurfaceCapabilities D3D12Surface::GetCapabilities(const Ref<Adapter>& adapter)
{
    RECT rect;
    GetClientRect(m_hwnd, &rect);
    uint32_t w = rect.right - rect.left;
    uint32_t h = rect.bottom - rect.top;

    return SurfaceCapabilities{.minImageCount = 2,
                               .maxImageCount = 16,
                               .currentExtent = {w, h},
                               .minImageExtent = {1, 1},
                               .maxImageExtent = {16'384, 16'384},
                               .currentTransform = {}};
}

std::vector<SurfaceFormat> D3D12Surface::GetSupportedFormats(const Ref<Adapter>& adapter)
{
    return {
        {Format::BGRA8_UNORM, ColorSpace::SRGB_NONLINEAR},
        {Format::RGBA8_UNORM, ColorSpace::SRGB_NONLINEAR},
        {Format::RGBA16_FLOAT, ColorSpace::EXTENDED_SRGB_LINEAR},
    };
}

Ref<Queue> D3D12Surface::GetPresentQueue(const Ref<Device>& device)
{
    return device->GetQueue(QueueType::Graphics, 0);
}

std::vector<PresentMode> D3D12Surface::GetSupportedPresentModes(const Ref<Adapter>& adapter)
{
    return {PresentMode::Immediate, PresentMode::Fifo, PresentMode::Mailbox};
}
} // namespace luna::RHI

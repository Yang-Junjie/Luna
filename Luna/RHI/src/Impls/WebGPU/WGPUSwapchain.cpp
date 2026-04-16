#include "Impls/WebGPU/WGPUCommon.h"
#include "Impls/WebGPU/WGPUDevice.h"
#include "Impls/WebGPU/WGPUSwapchain.h"
#include "Impls/WebGPU/WGPUTexture.h"

#include <algorithm>
#include <stdexcept>

namespace Cacao {
WGPUSwapchain::WGPUSwapchain(const Ref<Device>& device, const SwapchainCreateInfo& info)
    : m_createInfo(info)
{
    m_device = std::dynamic_pointer_cast<WGPUDevice>(device);
    if (!m_device) {
        throw std::runtime_error("WGPUSwapchain requires a WGPUDevice");
    }

    m_imageCount = std::max(info.MinImageCount, 2u);

    if (!info.CompatibleSurface) {
        throw std::runtime_error("WGPUSwapchain requires a compatible surface");
    }

    // Surface native handle would be extracted from WGPUSurface class
    // Configure the surface for presentation
    auto wgpuDevice = m_device->GetHandle();
    if (!wgpuDevice) {
        return;
    }

    // Modern Dawn/WebGPU surface configuration
    WGPUSurfaceConfiguration config = {};
    config.device = wgpuDevice;
    config.format = ToWGPUFormat(info.Format);
    config.width = info.Extent.width;
    config.height = info.Extent.height;
    config.usage = WGPUTextureUsage_RenderAttachment;

    switch (info.PresentMode) {
        case PresentMode::Immediate:
            config.presentMode = WGPUPresentMode_Immediate;
            break;
        case PresentMode::Mailbox:
            config.presentMode = WGPUPresentMode_Mailbox;
            break;
        case PresentMode::Fifo:
            config.presentMode = WGPUPresentMode_Fifo;
            break;
        default:
            config.presentMode = WGPUPresentMode_Fifo;
            break;
    }

    config.alphaMode = WGPUCompositeAlphaMode_Opaque;

    // m_surface must be extracted from the Cacao Surface wrapper
    // For now, store config; actual configure happens when surface is available
    // wgpuSurfaceConfigure(m_surface, &config);
}

WGPUSwapchain::~WGPUSwapchain()
{
    if (m_surface) {
        wgpuSurfaceUnconfigure(m_surface);
    }
}

Extent2D WGPUSwapchain::GetExtent() const
{
    return m_createInfo.Extent;
}

Format WGPUSwapchain::GetFormat() const
{
    return m_createInfo.Format;
}

PresentMode WGPUSwapchain::GetPresentMode() const
{
    return m_createInfo.PresentMode;
}

uint32_t WGPUSwapchain::GetImageCount() const
{
    return m_imageCount;
}

Result WGPUSwapchain::AcquireNextImage(const Ref<Synchronization>& sync, int frameIndex, int& imageIndex)
{
    if (!m_surface) {
        imageIndex = 0;
        return Result::Success;
    }

    WGPUSurfaceTexture surfaceTexture = {};
    wgpuSurfaceGetCurrentTexture(m_surface, &surfaceTexture);

    switch (surfaceTexture.status) {
        case WGPUSurfaceGetCurrentTextureStatus_Success: {
            TextureCreateInfo texCI;
            texCI.Type = TextureType::Texture2D;
            texCI.Width = m_createInfo.Extent.width;
            texCI.Height = m_createInfo.Extent.height;
            texCI.Format = m_createInfo.Format;
            texCI.Usage = TextureUsageFlags::ColorAttachment;
            m_currentBackBuffer = std::make_shared<WGPUTexture>(texCI);
            imageIndex = 0;
            return Result::Success;
        }
        case WGPUSurfaceGetCurrentTextureStatus_Timeout:
            return Result::Timeout;
        case WGPUSurfaceGetCurrentTextureStatus_Outdated:
            return Result::OutOfDate;
        case WGPUSurfaceGetCurrentTextureStatus_Lost:
            return Result::DeviceLost;
        default:
            return Result::Error;
    }
}

Result WGPUSwapchain::Present(const Ref<Queue>& queue, const Ref<Synchronization>& sync, uint32_t frameIndex)
{
    if (m_surface) {
        wgpuSurfacePresent(m_surface);
    }

    m_currentBackBuffer = nullptr;
    return Result::Success;
}

Ref<Texture> WGPUSwapchain::GetBackBuffer(uint32_t index) const
{
    return m_currentBackBuffer;
}
} // namespace Cacao

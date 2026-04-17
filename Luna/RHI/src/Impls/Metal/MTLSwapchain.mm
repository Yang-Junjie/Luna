#ifdef __APPLE__
#include "Impls/Metal/MTLSwapchain.h"
#include "Impls/Metal/MTLDevice.h"
#include "Impls/Metal/MTLTexture.h"
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>

namespace luna::RHI
{
    static MTLPixelFormat ToMTLPixelFormatSwap(Format format)
    {
        switch (format)
        {
        case Format::BGRA8_UNORM:  return MTLPixelFormatBGRA8Unorm;
        case Format::BGRA8_SRGB:   return MTLPixelFormatBGRA8Unorm_sRGB;
        case Format::RGBA8_UNORM:  return MTLPixelFormatRGBA8Unorm;
        case Format::RGBA16_FLOAT: return MTLPixelFormatRGBA16Float;
        default:                   return MTLPixelFormatBGRA8Unorm;
        }
    }

    MTLSwapchain::MTLSwapchain(const Ref<Device>& device, const SwapchainCreateInfo& info)
        : m_createInfo(info)
    {
        auto mtlDevice = std::dynamic_pointer_cast<MTLDevice>(device);
        if (!mtlDevice) return;

        id<MTLDevice> dev = (id<MTLDevice>)mtlDevice->GetHandle();
        if (!dev) return;

        // The surface provides a CAMetalLayer (set up by the window system)
        // For now, create a new CAMetalLayer and configure it
        CAMetalLayer* layer = [CAMetalLayer layer];
        layer.device = dev;
        layer.pixelFormat = ToMTLPixelFormatSwap(info.Format);
        layer.drawableSize = CGSizeMake(info.Extent.width, info.Extent.height);
        layer.framebufferOnly = YES;
        layer.maximumDrawableCount = std::max(info.MinImageCount, 2u);

        switch (info.PresentMode)
        {
        case PresentMode::Immediate:
            layer.displaySyncEnabled = NO;
            break;
        case PresentMode::Fifo:
        case PresentMode::FifoRelaxed:
        default:
            layer.displaySyncEnabled = YES;
            break;
        }

        m_metalLayer = layer;
    }

    Result MTLSwapchain::AcquireNextImage(const Ref<Synchronization>& sync, int idx, int& out)
    {
        out = 0;
        if (!m_metalLayer) return Result::Error;

        CAMetalLayer* layer = (CAMetalLayer*)m_metalLayer;
        id<CAMetalDrawable> drawable = [layer nextDrawable];
        if (!drawable) return Result::Timeout;

        // The drawable's texture is the back buffer for this frame
        // Store drawable reference for Present
        return Result::Success;
    }

    Result MTLSwapchain::Present(const Ref<Queue>& queue, const Ref<Synchronization>& sync, uint32_t frameIndex)
    {
        // In Metal, present is called on the drawable via the command buffer:
        // [commandBuffer presentDrawable:drawable];
        // [commandBuffer commit];
        // This is typically done in the command buffer encoder, not here.
        return Result::Success;
    }

    uint32_t MTLSwapchain::GetImageCount() const
    {
        if (m_metalLayer)
        {
            CAMetalLayer* layer = (CAMetalLayer*)m_metalLayer;
            return (uint32_t)layer.maximumDrawableCount;
        }
        return std::max(m_createInfo.MinImageCount, 2u);
    }

    Ref<Texture> MTLSwapchain::GetBackBuffer(uint32_t index) const
    {
        // Back buffer texture is obtained from the current drawable
        // The drawable must be acquired first via AcquireNextImage
        return nullptr;
    }

    Extent2D MTLSwapchain::GetExtent() const { return m_createInfo.Extent; }
    Format MTLSwapchain::GetFormat() const { return m_createInfo.Format; }
    PresentMode MTLSwapchain::GetPresentMode() const { return m_createInfo.PresentMode; }
}
#endif // __APPLE__

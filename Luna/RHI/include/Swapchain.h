#ifndef CACAO_CACAOSWAPCHAIN_H
#define CACAO_CACAOSWAPCHAIN_H
#include "Surface.h"
#include "Texture.h"
namespace Cacao
{
    class Queue;
    class Synchronization;
    enum class CompositeAlpha
    {
        Opaque, 
        PreMultiplied, 
        PostMultiplied, 
        Inherit 
    };
    enum class SwapchainUsageFlags : uint32_t
    {
        None = 0,
        ColorAttachment = 1 << 0, 
        TransferSrc = 1 << 1, 
        TransferDst = 1 << 2, 
        Storage = 1 << 3, 
    };
    inline SwapchainUsageFlags operator|(SwapchainUsageFlags a, SwapchainUsageFlags b)
    {
        return static_cast<SwapchainUsageFlags>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
    }
    inline SwapchainUsageFlags& operator|=(SwapchainUsageFlags& a, SwapchainUsageFlags b)
    {
        a = a | b;
        return a;
    }
    inline bool operator&(SwapchainUsageFlags a, SwapchainUsageFlags b)
    {
        return (static_cast<uint32_t>(a) & static_cast<uint32_t>(b)) != 0;
    }
    struct SwapchainCreateInfo
    {
        Extent2D Extent;
        Format Format = Format::BGRA8_UNORM;
        ColorSpace ColorSpace = ColorSpace::SRGB_NONLINEAR;
        PresentMode PresentMode = PresentMode::Mailbox;
        uint32_t MinImageCount = 3;
        SurfaceTransform PreTransform;
        CompositeAlpha CompositeAlpha = CompositeAlpha::Opaque;
        SwapchainUsageFlags Usage = SwapchainUsageFlags::ColorAttachment;
        bool Clipped = true;
        Ref<Surface> CompatibleSurface = nullptr;
        uint32_t ImageArrayLayers = 1;
    };
    enum class Result
    {
        Success, 
        Timeout, 
        NotReady, 
        Suboptimal, 
        OutOfDate, 
        DeviceLost, 
        Error 
    };
    class CACAO_API Swapchain : public std::enable_shared_from_this<Swapchain>
    {
    public:
        virtual ~Swapchain() = default;
        virtual Result Present(
            const Ref<Queue>& queue,
            const Ref<Synchronization>& sync,
            uint32_t frameIndex) = 0;
        virtual uint32_t GetImageCount() const = 0;
        virtual Ref<Texture> GetBackBuffer(uint32_t index) const = 0;
        virtual Extent2D GetExtent() const = 0;
        virtual Format GetFormat() const = 0;
        virtual PresentMode GetPresentMode() const = 0;
        virtual Result AcquireNextImage(const Ref<Synchronization>& sync, int idx, int& out) = 0;
    };
}
#endif 

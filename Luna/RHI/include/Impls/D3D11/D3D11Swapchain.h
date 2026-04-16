#ifndef CACAO_D3D11SWAPCHAIN_H
#define CACAO_D3D11SWAPCHAIN_H
#include "D3D11Common.h"

#include <Swapchain.h>

namespace Cacao {
class D3D11Device;

class CACAO_API D3D11Swapchain : public Swapchain {
public:
    D3D11Swapchain(Ref<D3D11Device> device, const SwapchainCreateInfo& createInfo);

    Result Present(const Ref<Queue>& queue, const Ref<Synchronization>& sync, uint32_t frameIndex) override;

    uint32_t GetImageCount() const override
    {
        return m_imageCount;
    }

    Ref<Texture> GetBackBuffer(uint32_t index) const override;

    Extent2D GetExtent() const override
    {
        return m_extent;
    }

    Format GetFormat() const override
    {
        return m_format;
    }

    PresentMode GetPresentMode() const override
    {
        return m_presentMode;
    }

    Result AcquireNextImage(const Ref<Synchronization>& sync, int idx, int& out) override;

    IDXGISwapChain1* GetNativeSwapchain() const
    {
        return m_swapchain.Get();
    }

private:
    void CreateBackBuffers();

    Ref<D3D11Device> m_device;
    ComPtr<IDXGISwapChain1> m_swapchain;
    std::vector<Ref<Texture>> m_backBuffers;
    uint32_t m_imageCount = 2;
    uint32_t m_currentImage = 0;
    Extent2D m_extent{};
    Format m_format = Format::BGRA8_UNORM;
    PresentMode m_presentMode = PresentMode::Mailbox;
};
} // namespace Cacao
#endif

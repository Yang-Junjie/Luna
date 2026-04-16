#ifndef CACAO_D3D12SWAPCHAIN_H
#define CACAO_D3D12SWAPCHAIN_H
#include "D3D12Common.h"
#include "Swapchain.h"

namespace Cacao
{
    class D3D12Queue;

    class CACAO_API D3D12Swapchain final : public Swapchain
    {
    private:
        ComPtr<IDXGISwapChain4> m_swapchain;
        std::vector<Ref<Texture>> m_backBuffers;
        Ref<Device> m_device;
        SwapchainCreateInfo m_createInfo;
        uint32_t m_currentBackBufferIndex = 0;
        bool m_hasAcquiredImage = false;

        friend class D3D12Device;
        friend class D3D12Synchronization;
        IDXGISwapChain4* GetHandle() const { return m_swapchain.Get(); }

        void CreateBackBuffers();

    public:
        D3D12Swapchain(const Ref<Device>& device, const SwapchainCreateInfo& info);
        ~D3D12Swapchain() override;

        Result Present(const Ref<Queue>& queue, const Ref<Synchronization>& sync,
                      uint32_t frameIndex) override;
        uint32_t GetImageCount() const override;
        Ref<Texture> GetBackBuffer(uint32_t index) const override;
        Extent2D GetExtent() const override;
        Format GetFormat() const override;
        PresentMode GetPresentMode() const override;
        Result AcquireNextImage(const Ref<Synchronization>& sync, int idx, int& out) override;
    };
}

#endif

#include "Impls/D3D11/D3D11Adapter.h"
#include "Impls/D3D11/D3D11Device.h"
#include "Impls/D3D11/D3D11Instance.h"
#include "Impls/D3D11/D3D11Surface.h"
#include "Impls/D3D11/D3D11Swapchain.h"
#include "Impls/D3D11/D3D11Texture.h"

namespace luna::RHI {
D3D11Swapchain::D3D11Swapchain(Ref<D3D11Device> device, const SwapchainCreateInfo& createInfo)
    : m_device(std::move(device)),
      m_extent(createInfo.Extent),
      m_format(createInfo.Format),
      m_presentMode(createInfo.PresentMode),
      m_imageCount(createInfo.MinImageCount)
{
    DXGI_SWAP_CHAIN_DESC1 desc{};
    desc.Width = m_extent.width;
    desc.Height = m_extent.height;
    desc.Format = D3D11_ToDXGIFormat(m_format);
    desc.SampleDesc.Count = 1;
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.BufferCount = m_imageCount;
    desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    desc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

    HWND hwnd = nullptr;
    if (createInfo.CompatibleSurface) {
        auto* d3dSurface = dynamic_cast<D3D11Surface*>(createInfo.CompatibleSurface.get());
        if (d3dSurface) {
            hwnd = d3dSurface->GetHWND();
        }
    }

    if (hwnd) {
        ComPtr<IDXGIDevice> dxgiDevice;
        m_device->GetNativeDevice()->QueryInterface(IID_PPV_ARGS(&dxgiDevice));
        ComPtr<IDXGIAdapter> dxgiAdapter;
        dxgiDevice->GetAdapter(&dxgiAdapter);
        ComPtr<IDXGIFactory2> factory;
        dxgiAdapter->GetParent(IID_PPV_ARGS(&factory));

        factory->CreateSwapChainForHwnd(m_device->GetNativeDevice(), hwnd, &desc, nullptr, nullptr, &m_swapchain);
    }

    if (m_swapchain) {
        CreateBackBuffers();
    }
}

void D3D11Swapchain::CreateBackBuffers()
{
    ComPtr<ID3D11Texture2D> backBuffer;
    m_swapchain->GetBuffer(0, IID_PPV_ARGS(&backBuffer));
    if (backBuffer) {
        auto tex = CreateRef<D3D11Texture>(m_device, backBuffer, m_format);
        m_backBuffers.resize(m_imageCount, tex);
    }
}

Result D3D11Swapchain::Present(const Ref<Queue>& queue, const Ref<Synchronization>& sync, uint32_t frameIndex)
{
    if (!m_swapchain) {
        return Result::Error;
    }
    UINT syncInterval = (m_presentMode == PresentMode::Immediate) ? 0 : 1;
    HRESULT hr = m_swapchain->Present(syncInterval, 0);
    if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET) {
        HRESULT reason = m_device->GetNativeDevice()->GetDeviceRemovedReason();
        fprintf(stderr, "D3D11: Present DeviceLost, reason=0x%08X\n", (unsigned) reason);
        return Result::DeviceLost;
    }
    if (hr == DXGI_STATUS_OCCLUDED) {
        return Result::Suboptimal;
    }
    return SUCCEEDED(hr) ? Result::Success : Result::Error;
}

Ref<Texture> D3D11Swapchain::GetBackBuffer(uint32_t index) const
{
    if (index < m_backBuffers.size()) {
        return m_backBuffers[index];
    }
    return nullptr;
}

Result D3D11Swapchain::AcquireNextImage(const Ref<Synchronization>& sync, int idx, int& out)
{
    out = 0;
    return Result::Success;
}
} // namespace luna::RHI

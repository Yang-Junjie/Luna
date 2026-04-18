#include "Impls/D3D12/D3D12Adapter.h"
#include "Impls/D3D12/D3D12Device.h"
#include "Impls/D3D12/D3D12Instance.h"
#include "Impls/D3D12/D3D12Queue.h"
#include "Impls/D3D12/D3D12Surface.h"
#include "Impls/D3D12/D3D12Swapchain.h"
#include "Impls/D3D12/D3D12Texture.h"
#include "Logging.h"

#include <cstdio>

namespace luna::RHI {
D3D12Swapchain::D3D12Swapchain(const Ref<Device>& device, const SwapchainCreateInfo& info)
    : m_device(device),
      m_createInfo(info)
{
    auto d3dDevice = std::dynamic_pointer_cast<D3D12Device>(device);
    auto adapter = std::dynamic_pointer_cast<D3D12Adapter>(d3dDevice->GetParentAdapter());
    auto d3dInstance = std::dynamic_pointer_cast<D3D12Instance>(adapter->m_instance);
    auto queue = std::dynamic_pointer_cast<D3D12Queue>(d3dDevice->GetQueue(QueueType::Graphics, 0));

    DXGI_SWAP_CHAIN_DESC1 swapchainDesc = {};
    swapchainDesc.Width = info.Extent.width;
    swapchainDesc.Height = info.Extent.height;
    swapchainDesc.Format = ToDXGIFormat(info.Format);
    swapchainDesc.SampleDesc.Count = 1;
    swapchainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapchainDesc.BufferCount = info.MinImageCount;
    swapchainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;

    HWND hwnd = nullptr;
    if (info.CompatibleSurface) {
        auto d3dSurface = std::dynamic_pointer_cast<D3D12Surface>(info.CompatibleSurface);
        if (d3dSurface) {
            hwnd = d3dSurface->GetHWND();
        }
    }

    if (hwnd && queue) {
        ComPtr<IDXGISwapChain1> swapchain1;
        d3dInstance->GetFactory()->CreateSwapChainForHwnd(
            queue->GetHandle(), hwnd, &swapchainDesc, nullptr, nullptr, &swapchain1);
        if (swapchain1) {
            swapchain1.As(&m_swapchain);
        }
    }

    if (m_swapchain) {
        CreateBackBuffers();
    }
}

D3D12Swapchain::~D3D12Swapchain() = default;

void D3D12Swapchain::CreateBackBuffers()
{
    m_backBuffers.clear();
    DXGI_SWAP_CHAIN_DESC1 desc;
    m_swapchain->GetDesc1(&desc);

    for (uint32_t i = 0; i < desc.BufferCount; i++) {
        ComPtr<ID3D12Resource> backBuffer;
        m_swapchain->GetBuffer(i, IID_PPV_ARGS(&backBuffer));

        TextureCreateInfo texInfo = {};
        texInfo.Width = desc.Width;
        texInfo.Height = desc.Height;
        texInfo.Depth = 1;
        texInfo.Format = m_createInfo.Format;
        texInfo.Usage = TextureUsageFlags::ColorAttachment;
        texInfo.MipLevels = 1;
        texInfo.ArrayLayers = 1;
        texInfo.InitialState = ResourceState::Present;

        auto tex = std::make_shared<D3D12Texture>(m_device, backBuffer, texInfo);
        m_backBuffers.push_back(tex);
    }
}

Result D3D12Swapchain::Present(const Ref<Queue>& queue, const Ref<Synchronization>& sync, uint32_t frameIndex)
{
    if (!m_swapchain || !m_hasAcquiredImage) {
        return Result::Error;
    }

    UINT syncInterval = (m_createInfo.PresentMode == PresentMode::Immediate) ? 0 : 1;
    HRESULT hr = m_swapchain->Present(syncInterval, 0);
    m_hasAcquiredImage = false;

    if (SUCCEEDED(hr)) {
        return Result::Success;
    }
    if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET) {
        auto d3dDev = std::dynamic_pointer_cast<D3D12Device>(m_device);
        if (d3dDev) {
            HRESULT reason = d3dDev->GetHandle()->GetDeviceRemovedReason();
            char buffer[128];
            snprintf(buffer,
                     sizeof(buffer),
                     "D3D12: Device removed reason: 0x%08X",
                     static_cast<unsigned>(reason));
            LogMessage(LogLevel::Error, buffer);
            ComPtr<ID3D12InfoQueue> iq;
            if (SUCCEEDED(d3dDev->GetHandle()->QueryInterface(IID_PPV_ARGS(&iq)))) {
                UINT64 n = iq->GetNumStoredMessages();
                for (UINT64 i = 0; i < n && i < 10; ++i) {
                    SIZE_T len = 0;
                    iq->GetMessage(i, nullptr, &len);
                    auto* msg = (D3D12_MESSAGE*) malloc(len);
                    if (msg) {
                        iq->GetMessage(i, msg, &len);
                        char messageBuffer[1024];
                        snprintf(messageBuffer,
                                 sizeof(messageBuffer),
                                 "D3D12 MSG[%llu]: %s",
                                 static_cast<unsigned long long>(i),
                                 msg->pDescription ? msg->pDescription : "");
                        LogMessage(LogLevel::Error, messageBuffer);
                        free(msg);
                    }
                }
            }
        }
        return Result::DeviceLost;
    }
    return Result::Error;
}

Result D3D12Swapchain::AcquireNextImage(const Ref<Synchronization>& sync, int idx, int& out)
{
    if (!m_swapchain) {
        return Result::Error;
    }

    out = static_cast<int>(m_swapchain->GetCurrentBackBufferIndex());
    m_currentBackBufferIndex = static_cast<uint32_t>(out);
    m_hasAcquiredImage = true;
    return Result::Success;
}

uint32_t D3D12Swapchain::GetImageCount() const
{
    return static_cast<uint32_t>(m_backBuffers.size());
}

Ref<Texture> D3D12Swapchain::GetBackBuffer(uint32_t index) const
{
    if (index < m_backBuffers.size()) {
        return m_backBuffers[index];
    }
    return nullptr;
}

Extent2D D3D12Swapchain::GetExtent() const
{
    if (m_swapchain) {
        DXGI_SWAP_CHAIN_DESC1 desc;
        m_swapchain->GetDesc1(&desc);
        return {desc.Width, desc.Height};
    }
    return m_createInfo.Extent;
}

Format D3D12Swapchain::GetFormat() const
{
    return m_createInfo.Format;
}

PresentMode D3D12Swapchain::GetPresentMode() const
{
    return m_createInfo.PresentMode;
}
} // namespace luna::RHI

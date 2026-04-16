#include "Impls/D3D11/D3D11Device.h"
#include "Impls/D3D11/D3D11Synchronization.h"

namespace Cacao {
D3D11Synchronization::D3D11Synchronization(const Ref<D3D11Device>& device, uint32_t maxFramesInFlight)
    : m_device(device),
      m_maxFramesInFlight(maxFramesInFlight)
{
    m_eventQueries.resize(maxFramesInFlight);
    D3D11_QUERY_DESC desc = {};
    desc.Query = D3D11_QUERY_EVENT;
    for (uint32_t i = 0; i < maxFramesInFlight; i++) {
        device->GetNativeDevice()->CreateQuery(&desc, &m_eventQueries[i]);
    }
}

void D3D11Synchronization::WaitForFrame(uint32_t frameIndex) const
{
    auto* ctx = m_device->GetImmediateContext();
    BOOL done = FALSE;
    while (ctx->GetData(m_eventQueries[frameIndex].Get(), &done, sizeof(BOOL), 0) == S_FALSE) {
        // Spin-wait
    }
}

void D3D11Synchronization::ResetFrameFence(uint32_t frameIndex) const
{
    // Event queries are reset implicitly
}

uint32_t D3D11Synchronization::AcquireNextImageIndex(const Ref<Swapchain>& swapchain, uint32_t frameIndex) const
{
    return 0; // DX11 swapchain has single back buffer model
}

void D3D11Synchronization::WaitIdle() const
{
    for (uint32_t i = 0; i < m_maxFramesInFlight; i++) {
        WaitForFrame(i);
    }
}

void D3D11Synchronization::SignalFrame(uint32_t frameIndex)
{
    m_device->GetImmediateContext()->End(m_eventQueries[frameIndex].Get());
}
} // namespace Cacao

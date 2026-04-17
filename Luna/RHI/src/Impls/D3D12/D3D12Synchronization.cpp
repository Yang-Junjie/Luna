#include "Impls/D3D12/D3D12Device.h"
#include "Impls/D3D12/D3D12Swapchain.h"
#include "Impls/D3D12/D3D12Synchronization.h"

namespace luna::RHI {
D3D12Synchronization::D3D12Synchronization(const Ref<Device>& device, uint32_t maxFramesInFlight)
    : m_device(device),
      m_maxFramesInFlight(maxFramesInFlight)
{
    auto d3dDevice = std::dynamic_pointer_cast<D3D12Device>(device);
    m_frameFences.resize(maxFramesInFlight);
    m_fenceValues.resize(maxFramesInFlight, 0);
    m_fenceEvents.resize(maxFramesInFlight);

    for (uint32_t i = 0; i < maxFramesInFlight; i++) {
        d3dDevice->GetHandle()->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_frameFences[i]));
        m_fenceEvents[i] = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    }
}

D3D12Synchronization::~D3D12Synchronization()
{
    for (auto& event : m_fenceEvents) {
        if (event) {
            CloseHandle(event);
        }
    }
}

Ref<D3D12Synchronization> D3D12Synchronization::Create(const Ref<Device>& device, uint32_t maxFramesInFlight)
{
    return std::make_shared<D3D12Synchronization>(device, maxFramesInFlight);
}

void D3D12Synchronization::WaitForFrame(uint32_t frameIndex) const
{
    if (m_frameFences[frameIndex]->GetCompletedValue() < m_fenceValues[frameIndex]) {
        m_frameFences[frameIndex]->SetEventOnCompletion(m_fenceValues[frameIndex], m_fenceEvents[frameIndex]);
        WaitForSingleObject(m_fenceEvents[frameIndex], INFINITE);
    }
}

void D3D12Synchronization::ResetFrameFence(uint32_t frameIndex) const
{
    // D3D12 fences are monotonically increasing; no explicit reset needed
}

uint32_t D3D12Synchronization::AcquireNextImageIndex(const Ref<Swapchain>& swapchain, uint32_t frameIndex) const
{
    auto d3dSwapchain = std::dynamic_pointer_cast<D3D12Swapchain>(swapchain);
    return d3dSwapchain->GetHandle()->GetCurrentBackBufferIndex();
}

uint32_t D3D12Synchronization::GetMaxFramesInFlight() const
{
    return m_maxFramesInFlight;
}

void D3D12Synchronization::WaitIdle() const
{
    for (uint32_t i = 0; i < m_maxFramesInFlight; i++) {
        WaitForFrame(i);
    }
}
} // namespace luna::RHI

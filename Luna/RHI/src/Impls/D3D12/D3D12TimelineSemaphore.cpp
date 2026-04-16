#include "Impls/D3D12/D3D12Device.h"
#include "Impls/D3D12/D3D12TimelineSemaphore.h"

namespace Cacao {
D3D12TimelineSemaphore::D3D12TimelineSemaphore(Ref<Device> device, uint64_t initialValue)
    : m_device(std::move(device))
{
    auto d3dDevice = std::dynamic_pointer_cast<D3D12Device>(m_device);
    d3dDevice->GetHandle()->CreateFence(initialValue, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence));
    m_event = CreateEvent(nullptr, FALSE, FALSE, nullptr);
}

D3D12TimelineSemaphore::~D3D12TimelineSemaphore()
{
    if (m_event) {
        CloseHandle(m_event);
    }
}

void D3D12TimelineSemaphore::Signal(uint64_t value)
{
    m_fence->Signal(value);
}

bool D3D12TimelineSemaphore::Wait(uint64_t value, uint64_t timeoutNs)
{
    if (m_fence->GetCompletedValue() >= value) {
        return true;
    }

    m_fence->SetEventOnCompletion(value, m_event);

    DWORD timeoutMs = INFINITE;
    if (timeoutNs != UINT64_MAX) {
        timeoutMs = static_cast<DWORD>(timeoutNs / 1'000'000);
    }

    return WaitForSingleObject(m_event, timeoutMs) == WAIT_OBJECT_0;
}

uint64_t D3D12TimelineSemaphore::GetValue() const
{
    return m_fence->GetCompletedValue();
}
} // namespace Cacao

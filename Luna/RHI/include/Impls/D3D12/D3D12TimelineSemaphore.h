#ifndef CACAO_D3D12TIMELINESEMAPHORE_H
#define CACAO_D3D12TIMELINESEMAPHORE_H
#include "D3D12Common.h"
#include "Synchronization.h"

namespace Cacao {
class D3D12Device;

class CACAO_API D3D12TimelineSemaphore : public TimelineSemaphore {
public:
    D3D12TimelineSemaphore(Ref<Device> device, uint64_t initialValue);
    ~D3D12TimelineSemaphore() override;

    void Signal(uint64_t value) override;
    bool Wait(uint64_t value, uint64_t timeoutNs = UINT64_MAX) override;
    uint64_t GetValue() const override;

    ID3D12Fence* GetNativeHandle() const
    {
        return m_fence.Get();
    }

private:
    Ref<Device> m_device;
    ComPtr<ID3D12Fence> m_fence;
    HANDLE m_event = nullptr;
};
} // namespace Cacao
#endif

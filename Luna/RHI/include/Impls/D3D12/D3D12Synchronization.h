#ifndef CACAO_D3D12SYNCHRONIZATION_H
#define CACAO_D3D12SYNCHRONIZATION_H
#include "D3D12Common.h"
#include "Device.h"
#include "Synchronization.h"

namespace Cacao {
class CACAO_API D3D12Synchronization : public Synchronization {
private:
    uint32_t m_maxFramesInFlight;
    std::vector<ComPtr<ID3D12Fence>> m_frameFences;
    std::vector<uint64_t> m_fenceValues;
    std::vector<HANDLE> m_fenceEvents;
    Ref<Device> m_device;

    friend class D3D12Swapchain;
    friend class D3D12Queue;

public:
    D3D12Synchronization(const Ref<Device>& device, uint32_t maxFramesInFlight);
    ~D3D12Synchronization() override;
    static Ref<D3D12Synchronization> Create(const Ref<Device>& device, uint32_t maxFramesInFlight);

    void WaitForFrame(uint32_t frameIndex) const override;
    void ResetFrameFence(uint32_t frameIndex) const override;
    uint32_t AcquireNextImageIndex(const Ref<Swapchain>& swapchain, uint32_t frameIndex) const override;
    uint32_t GetMaxFramesInFlight() const override;
    void WaitIdle() const override;

    ID3D12Fence* GetFence(uint32_t frameIndex) const
    {
        return m_frameFences[frameIndex].Get();
    }

    uint64_t GetFenceValue(uint32_t frameIndex) const
    {
        return m_fenceValues[frameIndex];
    }

    void IncrementFenceValue(uint32_t frameIndex)
    {
        m_fenceValues[frameIndex]++;
    }
};
} // namespace Cacao

#endif

#ifndef CACAO_D3D11SYNCHRONIZATION_H
#define CACAO_D3D11SYNCHRONIZATION_H
#include "D3D11Common.h"
#include "Synchronization.h"

namespace Cacao {
class D3D11Device;

class CACAO_API D3D11Synchronization : public Synchronization {
private:
    uint32_t m_maxFramesInFlight;
    std::vector<ComPtr<ID3D11Query>> m_eventQueries;
    Ref<D3D11Device> m_device;

public:
    D3D11Synchronization(const Ref<D3D11Device>& device, uint32_t maxFramesInFlight);
    void WaitForFrame(uint32_t frameIndex) const override;
    void ResetFrameFence(uint32_t frameIndex) const override;
    uint32_t AcquireNextImageIndex(const Ref<Swapchain>& swapchain, uint32_t frameIndex) const override;

    uint32_t GetMaxFramesInFlight() const override
    {
        return m_maxFramesInFlight;
    }

    void WaitIdle() const override;
    void SignalFrame(uint32_t frameIndex);
};
} // namespace Cacao

#endif

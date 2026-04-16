#ifndef CACAO_D3D11ADAPTER_H
#define CACAO_D3D11ADAPTER_H
#include "D3D11Common.h"

#include <Adapter.h>

namespace Cacao {
class D3D11Instance;

class CACAO_API D3D11Adapter : public Adapter {
public:
    D3D11Adapter(Ref<D3D11Instance> instance, ComPtr<IDXGIAdapter1> adapter);

    AdapterProperties GetProperties() const override;
    AdapterType GetAdapterType() const override;
    bool IsFeatureSupported(DeviceFeature feature) const override;
    DeviceLimits QueryLimits() const override;
    Ref<Device> CreateDevice(const DeviceCreateInfo& createInfo) override;

    uint32_t FindQueueFamilyIndex(QueueType type) const override
    {
        return 0;
    }

    IDXGIAdapter1* GetNativeAdapter() const
    {
        return m_adapter.Get();
    }

private:
    Ref<D3D11Instance> m_instance;
    ComPtr<IDXGIAdapter1> m_adapter;
    DXGI_ADAPTER_DESC1 m_desc{};
};
} // namespace Cacao
#endif

#ifndef CACAO_D3D12ADAPTER_H
#define CACAO_D3D12ADAPTER_H
#include "D3D12Common.h"
#include <Adapter.h>

namespace Cacao
{
    class Instance;
    class D3D12Instance;

    class CACAO_API D3D12Adapter : public Adapter
    {
    private:
        ComPtr<IDXGIAdapter4> m_adapter;
        AdapterProperties m_properties;
        AdapterType m_adapterType;
        Ref<Instance> m_instance;

        friend class D3D12Instance;
        friend class D3D12Device;
        friend class D3D12Swapchain;
        IDXGIAdapter4* GetHandle() const { return m_adapter.Get(); }

    public:
        D3D12Adapter(const Ref<Instance>& inst, ComPtr<IDXGIAdapter4> adapter);
        static Ref<D3D12Adapter> Create(const Ref<Instance>& inst, ComPtr<IDXGIAdapter4> adapter);

        AdapterProperties GetProperties() const override;
        AdapterType GetAdapterType() const override;
        bool IsFeatureSupported(DeviceFeature feature) const override;
        DeviceLimits QueryLimits() const override;
        Ref<Device> CreateDevice(const DeviceCreateInfo& info) override;
        uint32_t FindQueueFamilyIndex(QueueType type) const override;
    };
}

#endif

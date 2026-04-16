#ifndef CACAO_WGPU_ADAPTER_H
#define CACAO_WGPU_ADAPTER_H

#include "Adapter.h"

#include <webgpu/webgpu.h>

namespace Cacao {
class WGPUInstance;

class CACAO_API WGPUAdapter : public Adapter {
private:
    Ref<WGPUInstance> m_instance;
    ::WGPUAdapter m_adapter = nullptr;

    friend class WGPUDevice;

public:
    WGPUAdapter(Ref<WGPUInstance> instance, ::WGPUAdapter adapter);
    ~WGPUAdapter() override;

    AdapterProperties GetProperties() const override;
    AdapterType GetAdapterType() const override;
    bool IsFeatureSupported(DeviceFeature feature) const override;
    DeviceLimits QueryLimits() const override;
    Ref<Device> CreateDevice(const DeviceCreateInfo& info) override;
    uint32_t FindQueueFamilyIndex(QueueType type) const override;

    ::WGPUAdapter GetNativeAdapter() const
    {
        return m_adapter;
    }
};
} // namespace Cacao

#endif

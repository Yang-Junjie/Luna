#ifndef CACAO_MTL_ADAPTER_H
#define CACAO_MTL_ADAPTER_H

#ifdef __APPLE__

#include "Adapter.h"

#ifdef __OBJC__
#import <Metal/Metal.h>
#endif

namespace Cacao {
class MTLInstance;

class CACAO_API MTLAdapter : public Adapter {
private:
    Ref<MTLInstance> m_instance;
#ifdef __OBJC__
    id<MTLDevice> m_device;
#else
    void* m_device;
#endif

public:
#ifdef __OBJC__
    MTLAdapter(Ref<MTLInstance> instance, id<MTLDevice> device);
#endif
    ~MTLAdapter() override = default;

    AdapterProperties GetProperties() const override;
    AdapterType GetAdapterType() const override;
    bool IsFeatureSupported(DeviceFeature feature) const override;
    DeviceLimits QueryLimits() const override;
    Ref<Device> CreateDevice(const DeviceCreateInfo& info) override;
    uint32_t FindQueueFamilyIndex(QueueType type) const override;

#ifdef __OBJC__
    id<MTLDevice> GetNativeDevice() const
    {
        return m_device;
    }
#endif
};
} // namespace Cacao

#endif // __APPLE__
#endif

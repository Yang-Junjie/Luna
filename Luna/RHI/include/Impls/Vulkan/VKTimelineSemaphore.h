#ifndef LUNA_RHI_VKTIMELINESEMAPHORE_H
#define LUNA_RHI_VKTIMELINESEMAPHORE_H
#include "VKCommon.h"

#include <Synchronization.h>

namespace luna::RHI {
class VKDevice;

class LUNA_RHI_API VKTimelineSemaphore : public TimelineSemaphore {
public:
    VKTimelineSemaphore(Ref<VKDevice> device, uint64_t initialValue);
    ~VKTimelineSemaphore() override;

    void Signal(uint64_t value) override;
    bool Wait(uint64_t value, uint64_t timeoutNs) override;
    uint64_t GetValue() const override;

    vk::Semaphore GetNativeHandle() const
    {
        return m_semaphore;
    }

private:
    Ref<VKDevice> m_device;
    vk::Semaphore m_semaphore;
};
} // namespace luna::RHI
#endif

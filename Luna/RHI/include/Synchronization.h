#ifndef CACAO_CACAOSYNCHRONIZATION_H
#define CACAO_CACAOSYNCHRONIZATION_H
#include "Core.h"
namespace Cacao
{
    class Swapchain;
    class CACAO_API Synchronization : public std::enable_shared_from_this<Synchronization>
    {
    public:
        virtual void WaitForFrame(uint32_t frameIndex) const =0;
        virtual void ResetFrameFence(uint32_t frameIndex) const =0;
        virtual uint32_t AcquireNextImageIndex(const Ref<Swapchain>& swapchain, uint32_t frameIndex) const =0;
        virtual uint32_t GetMaxFramesInFlight() const =0;
        virtual ~Synchronization() = default;
        virtual void WaitIdle() const =0;
    };

    class CACAO_API TimelineSemaphore : public std::enable_shared_from_this<TimelineSemaphore>
    {
    public:
        virtual ~TimelineSemaphore() = default;
        virtual void Signal(uint64_t value) = 0;
        virtual bool Wait(uint64_t value, uint64_t timeoutNs = UINT64_MAX) = 0;
        virtual uint64_t GetValue() const = 0;
    };
}
#endif


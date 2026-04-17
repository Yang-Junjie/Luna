#ifndef LUNA_RHI_MTLQUEUE_H
#define LUNA_RHI_MTLQUEUE_H
#ifdef __APPLE__
#include "MTLCommon.h"
#include "Queue.h"

namespace luna::RHI {
class LUNA_RHI_API MTLQueue final : public Queue {
public:
    MTLQueue(id commandQueue);
    ~MTLQueue() override = default;

    void Submit(const Ref<CommandBufferEncoder>& cmd, const Ref<Synchronization>& sync, uint32_t frameIndex) override;
    void Submit(std::span<const Ref<CommandBufferEncoder>> cmds,
                const Ref<Synchronization>& sync,
                uint32_t frameIndex) override;
    void Submit(const Ref<CommandBufferEncoder>& cmd) override;
    void WaitIdle() override;

    id GetHandle() const
    {
        return m_commandQueue;
    }

private:
    id m_commandQueue = nullptr; // id<MTLCommandQueue>
};
} // namespace luna::RHI
#endif // __APPLE__
#endif

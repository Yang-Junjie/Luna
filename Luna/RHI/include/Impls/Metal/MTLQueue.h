#ifndef CACAO_MTLQUEUE_H
#define CACAO_MTLQUEUE_H
#ifdef __APPLE__
#include "MTLCommon.h"
#include "Queue.h"

namespace Cacao
{
    class CACAO_API MTLQueue final : public Queue
    {
    public:
        MTLQueue(id commandQueue);
        ~MTLQueue() override = default;

        void Submit(const Ref<CommandBufferEncoder>& cmd, const Ref<Synchronization>& sync, uint32_t frameIndex) override;
        void Submit(std::span<const Ref<CommandBufferEncoder>> cmds, const Ref<Synchronization>& sync, uint32_t frameIndex) override;
        void Submit(const Ref<CommandBufferEncoder>& cmd) override;
        void WaitIdle() override;

        id GetHandle() const { return m_commandQueue; }

    private:
        id m_commandQueue = nullptr; // id<MTLCommandQueue>
    };
}
#endif // __APPLE__
#endif

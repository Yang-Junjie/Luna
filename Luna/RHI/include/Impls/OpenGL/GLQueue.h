#ifndef CACAO_GLQUEUE_H
#define CACAO_GLQUEUE_H
#include "Queue.h"
#include "Adapter.h"
#include "GLCommon.h"

namespace Cacao
{
    class GLDevice;

    class CACAO_API GLQueue final : public Queue
    {
    public:
        GLQueue();
        static Ref<GLQueue> Create(const Ref<GLDevice>& device = nullptr);

        QueueType GetType() const override;
        uint32_t GetIndex() const override { return 0; }
        void Submit(const Ref<CommandBufferEncoder>& cmd,
                    const Ref<Synchronization>& sync, uint32_t frameIndex) override;
        void Submit(std::span<const Ref<CommandBufferEncoder>> cmds,
                    const Ref<Synchronization>& sync, uint32_t frameIndex) override;
        void Submit(const Ref<CommandBufferEncoder>& cmd) override;
        void WaitIdle() override;
    };
}

#endif

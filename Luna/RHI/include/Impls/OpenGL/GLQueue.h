#ifndef LUNA_RHI_GLQUEUE_H
#define LUNA_RHI_GLQUEUE_H
#include "Adapter.h"
#include "GLCommon.h"
#include "Queue.h"

namespace luna::RHI {
class GLDevice;

class LUNA_RHI_API GLQueue final : public Queue {
public:
    GLQueue();
    static Ref<GLQueue> Create(const Ref<GLDevice>& device = nullptr);

    QueueType GetType() const override;

    uint32_t GetIndex() const override
    {
        return 0;
    }

    void Submit(const Ref<CommandBufferEncoder>& cmd, const Ref<Synchronization>& sync, uint32_t frameIndex) override;
    void Submit(std::span<const Ref<CommandBufferEncoder>> cmds,
                const Ref<Synchronization>& sync,
                uint32_t frameIndex) override;
    void Submit(const Ref<CommandBufferEncoder>& cmd) override;
    void WaitIdle() override;
};
} // namespace luna::RHI

#endif

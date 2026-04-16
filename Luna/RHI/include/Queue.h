#ifndef CACAO_CACAOQUEUE_H
#define CACAO_CACAOQUEUE_H
#include "Core.h"

namespace Cacao {
class Synchronization;
class CommandBufferEncoder;
enum class QueueType;

class CACAO_API Queue : public std::enable_shared_from_this<Queue> {
public:
    virtual ~Queue() = default;
    virtual QueueType GetType() const = 0;
    virtual uint32_t GetIndex() const = 0;
    virtual void
        Submit(const Ref<CommandBufferEncoder>& cmd, const Ref<Synchronization>& sync, uint32_t frameIndex) = 0;
    virtual void Submit(std::span<const Ref<CommandBufferEncoder>> cmds,
                        const Ref<Synchronization>& sync,
                        uint32_t frameIndex) = 0;
    virtual void Submit(const Ref<CommandBufferEncoder>& cmd) = 0;
    virtual void WaitIdle() = 0;
};
} // namespace Cacao
#endif

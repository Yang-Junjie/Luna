#include "Impls/OpenGL/GLCommandBufferEncoder.h"
#include "Impls/OpenGL/GLQueue.h"
#include "Impls/OpenGL/GLSynchronization.h"

namespace luna::RHI {
GLQueue::GLQueue() = default;

Ref<GLQueue> GLQueue::Create(const Ref<GLDevice>&)
{
    return std::make_shared<GLQueue>();
}

void GLQueue::Submit(const Ref<CommandBufferEncoder>& cmd, const Ref<Synchronization>& sync, uint32_t frameIndex)
{
    if (auto glEncoder = std::dynamic_pointer_cast<GLCommandBufferEncoder>(cmd)) {
        glEncoder->Execute();
    }

    if (auto glSync = std::dynamic_pointer_cast<GLSynchronization>(sync)) {
        glSync->SignalFrame(frameIndex);
    }
}

void GLQueue::Submit(std::span<const Ref<CommandBufferEncoder>> cmds,
                     const Ref<Synchronization>& sync,
                     uint32_t frameIndex)
{
    for (auto& cmd : cmds) {
        if (auto glEncoder = std::dynamic_pointer_cast<GLCommandBufferEncoder>(cmd)) {
            glEncoder->Execute();
        }
    }

    if (auto glSync = std::dynamic_pointer_cast<GLSynchronization>(sync)) {
        glSync->SignalFrame(frameIndex);
    }
}

void GLQueue::Submit(const Ref<CommandBufferEncoder>& cmd)
{
    if (auto glEncoder = std::dynamic_pointer_cast<GLCommandBufferEncoder>(cmd)) {
        glEncoder->Execute();
    }
}

void GLQueue::WaitIdle()
{
    glFinish();
}

QueueType GLQueue::GetType() const
{
    return QueueType::Graphics;
}
} // namespace luna::RHI

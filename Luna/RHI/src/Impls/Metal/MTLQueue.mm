#ifdef __APPLE__
#import <Metal/Metal.h>
#include "Impls/Metal/MTLQueue.h"
#include "Impls/Metal/MTLCommandBufferEncoder.h"

namespace Cacao
{
    MTLQueue::MTLQueue(id commandQueue) : m_commandQueue(commandQueue) {}

    void MTLQueue::Submit(const Ref<CommandBufferEncoder>& cmd,
                          const Ref<Synchronization>& sync, uint32_t frameIndex)
    {
        auto* mtlCmd = static_cast<MTLCommandBufferEncoder*>(cmd.get());
        id<MTLCommandBuffer> cmdBuf = (id<MTLCommandBuffer>)mtlCmd->GetCommandBuffer();
        if (cmdBuf)
            [cmdBuf commit];
    }

    void MTLQueue::Submit(std::span<const Ref<CommandBufferEncoder>> cmds,
                          const Ref<Synchronization>& sync, uint32_t frameIndex)
    {
        for (auto& cmd : cmds)
            Submit(cmd, sync, frameIndex);
    }

    void MTLQueue::Submit(const Ref<CommandBufferEncoder>& cmd)
    {
        auto* mtlCmd = static_cast<MTLCommandBufferEncoder*>(cmd.get());
        id<MTLCommandBuffer> cmdBuf = (id<MTLCommandBuffer>)mtlCmd->GetCommandBuffer();
        if (cmdBuf)
            [cmdBuf commit];
    }

    void MTLQueue::WaitIdle()
    {
        // Create and immediately commit an empty command buffer, then wait
        id<MTLCommandQueue> queue = (id<MTLCommandQueue>)m_commandQueue;
        id<MTLCommandBuffer> cmdBuf = [queue commandBuffer];
        [cmdBuf commit];
        [cmdBuf waitUntilCompleted];
    }
}
#endif // __APPLE__

#ifndef LUNA_RHI_D3D12QUEUE_H
#define LUNA_RHI_D3D12QUEUE_H
#include "D3D12Common.h"
#include "Queue.h"

namespace luna::RHI {
class LUNA_RHI_API D3D12Queue final : public Queue {
private:
    ComPtr<ID3D12CommandQueue> m_queue;
    ComPtr<ID3D12Fence> m_fence;
    HANDLE m_fenceEvent = nullptr;
    uint64_t m_fenceValue = 0;
    QueueType m_type;
    uint32_t m_index = 0;
    Ref<Device> m_device;

    friend class D3D12Device;
    friend class D3D12Swapchain;
    friend class D3D12CommandBufferEncoder;

    ID3D12CommandQueue* GetHandle() const
    {
        return m_queue.Get();
    }

public:
    D3D12Queue(const Ref<Device>& device, QueueType type, uint32_t index = 0);
    ~D3D12Queue() override;

    QueueType GetType() const override
    {
        return m_type;
    }

    uint32_t GetIndex() const override
    {
        return m_index;
    }

    void Submit(const Ref<CommandBufferEncoder>& cmd, const Ref<Synchronization>& sync, uint32_t frameIndex) override;
    void Submit(std::span<const Ref<CommandBufferEncoder>> cmds,
                const Ref<Synchronization>& sync,
                uint32_t frameIndex) override;
    void Submit(const Ref<CommandBufferEncoder>& cmd) override;
    void WaitIdle() override;

    uint64_t Signal();
    void WaitForFence(uint64_t value);
};
} // namespace luna::RHI

#endif

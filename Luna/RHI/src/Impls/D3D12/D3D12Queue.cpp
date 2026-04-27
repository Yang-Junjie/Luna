#include "Impls/D3D12/D3D12CommandBufferEncoder.h"
#include "Impls/D3D12/D3D12Device.h"
#include "Impls/D3D12/D3D12Queue.h"
#include "Impls/D3D12/D3D12Synchronization.h"

namespace luna::RHI {
D3D12Queue::D3D12Queue(const Ref<Device>& device, QueueType type, uint32_t index)
    : m_device(device),
      m_type(type),
      m_index(index)
{
    auto d3dDevice = std::dynamic_pointer_cast<D3D12Device>(device);

    D3D12_COMMAND_QUEUE_DESC desc = {};
    desc.Type = ToD3D12CommandListType(type);
    desc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
    desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    d3dDevice->GetHandle()->CreateCommandQueue(&desc, IID_PPV_ARGS(&m_queue));
    d3dDevice->GetHandle()->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence));
    m_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
}

D3D12Queue::~D3D12Queue()
{
    if (m_fenceEvent) {
        CloseHandle(m_fenceEvent);
    }
}

void D3D12Queue::Submit(const Ref<CommandBufferEncoder>& cmd, const Ref<Synchronization>& sync, uint32_t frameIndex)
{
    auto d3dEncoder = std::dynamic_pointer_cast<D3D12CommandBufferEncoder>(cmd);
    ID3D12CommandList* cmdLists[] = {d3dEncoder->GetHandle()};
    m_queue->ExecuteCommandLists(1, cmdLists);

    if (sync) {
        auto d3dSync = std::dynamic_pointer_cast<D3D12Synchronization>(sync);
        d3dSync->IncrementFenceValue(frameIndex);
        m_queue->Signal(d3dSync->GetFence(frameIndex), d3dSync->GetFenceValue(frameIndex));
    }
}

void D3D12Queue::Submit(std::span<const Ref<CommandBufferEncoder>> cmds,
                        const Ref<Synchronization>& sync,
                        uint32_t frameIndex)
{
    std::vector<ID3D12CommandList*> cmdLists;
    cmdLists.reserve(cmds.size());
    for (auto& cmd : cmds) {
        auto d3dEncoder = std::dynamic_pointer_cast<D3D12CommandBufferEncoder>(cmd);
        cmdLists.push_back(d3dEncoder->GetHandle());
    }
    if (!cmdLists.empty()) {
        m_queue->ExecuteCommandLists(static_cast<UINT>(cmdLists.size()), cmdLists.data());
    }

    if (sync) {
        auto d3dSync = std::dynamic_pointer_cast<D3D12Synchronization>(sync);
        d3dSync->IncrementFenceValue(frameIndex);
        m_queue->Signal(d3dSync->GetFence(frameIndex), d3dSync->GetFenceValue(frameIndex));
    }
}

void D3D12Queue::Submit(const Ref<CommandBufferEncoder>& cmd)
{
    auto d3dEncoder = std::dynamic_pointer_cast<D3D12CommandBufferEncoder>(cmd);
    ID3D12CommandList* cmdLists[] = {d3dEncoder->GetHandle()};
    m_queue->ExecuteCommandLists(1, cmdLists);
}

void D3D12Queue::WaitIdle()
{
    uint64_t val = Signal();
    WaitForFence(val);
}

double D3D12Queue::GetTimestampPeriodNs() const
{
    UINT64 frequency = 0;
    if (!m_queue || FAILED(m_queue->GetTimestampFrequency(&frequency)) || frequency == 0) {
        return 0.0;
    }

    return 1000000000.0 / static_cast<double>(frequency);
}

uint64_t D3D12Queue::Signal()
{
    m_fenceValue++;
    m_queue->Signal(m_fence.Get(), m_fenceValue);
    return m_fenceValue;
}

void D3D12Queue::WaitForFence(uint64_t value)
{
    if (m_fence->GetCompletedValue() < value) {
        m_fence->SetEventOnCompletion(value, m_fenceEvent);
        WaitForSingleObject(m_fenceEvent, INFINITE);
    }
}
} // namespace luna::RHI

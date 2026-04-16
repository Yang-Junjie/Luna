#ifndef CACAO_D3D12STAGINGBUFFER_H
#define CACAO_D3D12STAGINGBUFFER_H
#include "D3D12Common.h"
#include "D3D12MemAlloc.h"

#include <StagingBuffer.h>

namespace Cacao {
class CACAO_API D3D12StagingBufferPool final : public StagingBufferPool {
public:
    D3D12StagingBufferPool(const Ref<Device>& device, uint64_t blockSize, uint32_t maxFramesInFlight);
    ~D3D12StagingBufferPool() override;

    StagingAllocation Allocate(uint64_t size, uint64_t alignment) override;
    void Reset() override;
    void AdvanceFrame() override;

    uint64_t GetTotalAllocated() const override
    {
        return m_totalAllocated;
    }

    uint64_t GetCapacity() const override
    {
        return m_blockSize;
    }

private:
    Ref<Device> m_device;
    ComPtr<ID3D12Resource> m_uploadBuffer;
    D3D12MA::Allocation* m_allocation = nullptr;
    uint64_t m_blockSize;
    uint64_t m_offset = 0;
    uint64_t m_totalAllocated = 0;
    void* m_mappedData = nullptr;
};
} // namespace Cacao
#endif

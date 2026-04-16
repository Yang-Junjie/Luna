#ifndef CACAO_D3D11STAGINGBUFFER_H
#define CACAO_D3D11STAGINGBUFFER_H
#include "D3D11Common.h"
#include "StagingBuffer.h"

#include <vector>

namespace Cacao {
class D3D11Device;

class CACAO_API D3D11StagingBufferPool final : public StagingBufferPool {
private:
    struct PoolEntry {
        ComPtr<ID3D11Buffer> buffer;
        uint64_t size;
        bool inUse;
    };

    std::vector<PoolEntry> m_pool;
    Ref<D3D11Device> m_device;
    uint64_t m_totalSize = 0;
    uint64_t m_usedSize = 0;

public:
    D3D11StagingBufferPool(const Ref<D3D11Device>& device, uint64_t initialSize);
    StagingAllocation Allocate(uint64_t size, uint64_t alignment = 256) override;
    void Reset() override;

    void AdvanceFrame() override {}

    uint64_t GetCapacity() const override
    {
        return m_totalSize;
    }

    uint64_t GetTotalAllocated() const override
    {
        return m_usedSize;
    }
};
} // namespace Cacao

#endif

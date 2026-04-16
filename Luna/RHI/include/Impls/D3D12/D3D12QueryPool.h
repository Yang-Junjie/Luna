#ifndef CACAO_D3D12QUERYPOOL_H
#define CACAO_D3D12QUERYPOOL_H
#include "D3D12Common.h"
#include "QueryPool.h"
#include "D3D12MemAlloc.h"

namespace Cacao
{
    class CACAO_API D3D12QueryPool final : public QueryPool
    {
    private:
        ComPtr<ID3D12QueryHeap> m_queryHeap;
        ComPtr<ID3D12Resource> m_readbackBuffer;
        D3D12MA::Allocation* m_readbackAllocation = nullptr;
        Ref<Device> m_device;
        QueryPoolCreateInfo m_createInfo;

    public:
        D3D12QueryPool(const Ref<Device>& device, const QueryPoolCreateInfo& info);
        QueryType GetType() const override { return m_createInfo.Type; }
        uint32_t GetCount() const override { return m_createInfo.Count; }
        void Reset(uint32_t firstQuery, uint32_t count) override;
        bool GetResults(uint32_t firstQuery, uint32_t queryCount,
                        std::vector<uint64_t>& outResults, bool wait) override;

        ID3D12QueryHeap* GetHeap() const { return m_queryHeap.Get(); }
        ID3D12Resource* GetReadbackBuffer() const { return m_readbackBuffer.Get(); }
    };
}

#endif

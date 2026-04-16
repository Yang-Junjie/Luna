#include "Impls/D3D12/D3D12Device.h"
#include "Impls/D3D12/D3D12QueryPool.h"

namespace Cacao {
static D3D12_QUERY_HEAP_TYPE ToD3D12QueryHeapType(QueryType type)
{
    switch (type) {
        case QueryType::Timestamp:
            return D3D12_QUERY_HEAP_TYPE_TIMESTAMP;
        case QueryType::Occlusion:
            return D3D12_QUERY_HEAP_TYPE_OCCLUSION;
        case QueryType::PipelineStatistics:
            return D3D12_QUERY_HEAP_TYPE_PIPELINE_STATISTICS;
        default:
            return D3D12_QUERY_HEAP_TYPE_TIMESTAMP;
    }
}

D3D12QueryPool::D3D12QueryPool(const Ref<Device>& device, const QueryPoolCreateInfo& info)
    : m_device(device),
      m_createInfo(info)
{
    auto d3dDevice = std::dynamic_pointer_cast<D3D12Device>(device);

    D3D12_QUERY_HEAP_DESC heapDesc = {};
    heapDesc.Type = ToD3D12QueryHeapType(info.Type);
    heapDesc.Count = info.Count;
    d3dDevice->GetHandle()->CreateQueryHeap(&heapDesc, IID_PPV_ARGS(&m_queryHeap));

    D3D12_RESOURCE_DESC bufDesc = {};
    bufDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    bufDesc.Width = info.Count * sizeof(uint64_t);
    bufDesc.Height = 1;
    bufDesc.DepthOrArraySize = 1;
    bufDesc.MipLevels = 1;
    bufDesc.Format = DXGI_FORMAT_UNKNOWN;
    bufDesc.SampleDesc.Count = 1;
    bufDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    D3D12MA::ALLOCATION_DESC allocDesc = {};
    allocDesc.HeapType = D3D12_HEAP_TYPE_READBACK;

    d3dDevice->GetAllocator()->CreateResource(&allocDesc,
                                              &bufDesc,
                                              D3D12_RESOURCE_STATE_COPY_DEST,
                                              nullptr,
                                              &m_readbackAllocation,
                                              IID_PPV_ARGS(&m_readbackBuffer));
}

void D3D12QueryPool::Reset(uint32_t firstQuery, uint32_t count) {}

bool D3D12QueryPool::GetResults(uint32_t firstQuery, uint32_t queryCount, std::vector<uint64_t>& outResults, bool wait)
{
    outResults.resize(queryCount);
    D3D12_RANGE readRange = {firstQuery * sizeof(uint64_t), (firstQuery + queryCount) * sizeof(uint64_t)};
    void* mapped = nullptr;
    HRESULT hr = m_readbackBuffer->Map(0, &readRange, &mapped);
    if (FAILED(hr)) {
        return false;
    }

    auto* src = static_cast<uint8_t*>(mapped) + firstQuery * sizeof(uint64_t);
    memcpy(outResults.data(), src, queryCount * sizeof(uint64_t));

    D3D12_RANGE writeRange = {0, 0};
    m_readbackBuffer->Unmap(0, &writeRange);
    return true;
}
} // namespace Cacao

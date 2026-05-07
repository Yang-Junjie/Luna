#include "Impls/D3D11/D3D11Device.h"
#include "Impls/D3D11/D3D11QueryPool.h"

namespace luna::RHI {
static D3D11_QUERY ToD3D11QueryType(QueryType type)
{
    switch (type) {
        case QueryType::Timestamp:
            return D3D11_QUERY_TIMESTAMP;
        case QueryType::TimestampDisjoint:
            return D3D11_QUERY_TIMESTAMP_DISJOINT;
        case QueryType::Occlusion:
            return D3D11_QUERY_OCCLUSION;
        case QueryType::PipelineStatistics:
            return D3D11_QUERY_PIPELINE_STATISTICS;
        default:
            return D3D11_QUERY_TIMESTAMP;
    }
}

D3D11QueryPool::D3D11QueryPool(const Ref<D3D11Device>& device, const QueryPoolCreateInfo& info)
    : m_device(device),
      m_createInfo(info)
{
    m_queries.resize(info.Count);
    D3D11_QUERY_DESC queryDesc = {};
    queryDesc.Query = ToD3D11QueryType(info.Type);

    for (uint32_t i = 0; i < info.Count; i++) {
        device->GetNativeDevice()->CreateQuery(&queryDesc, &m_queries[i]);
    }
}

void D3D11QueryPool::Reset(uint32_t firstQuery, uint32_t queryCount)
{
    // D3D11 queries are implicitly reset on Begin
}

bool D3D11QueryPool::GetResults(uint32_t firstQuery, uint32_t queryCount, std::vector<uint64_t>& outResults, bool wait)
{
    if (m_createInfo.Type == QueryType::TimestampDisjoint) {
        return false;
    }

    auto* ctx = m_device->GetImmediateContext();
    outResults.resize(queryCount);

    for (uint32_t i = 0; i < queryCount; i++) {
        uint64_t result = 0;
        HRESULT hr = ctx->GetData(
            m_queries[firstQuery + i].Get(), &result, sizeof(uint64_t), wait ? 0 : D3D11_ASYNC_GETDATA_DONOTFLUSH);
        if (hr == S_FALSE) {
            return false;
        }
        if (FAILED(hr)) {
            return false;
        }
        outResults[i] = result;
    }
    return true;
}

bool D3D11QueryPool::GetTimestampDisjointResult(uint32_t queryIndex, TimestampDisjointResult& outResult, bool wait)
{
    if (m_createInfo.Type != QueryType::TimestampDisjoint || queryIndex >= m_queries.size()) {
        return false;
    }

    D3D11_QUERY_DATA_TIMESTAMP_DISJOINT result{};
    const HRESULT hr = m_device->GetImmediateContext()->GetData(
        m_queries[queryIndex].Get(), &result, sizeof(result), wait ? 0 : D3D11_ASYNC_GETDATA_DONOTFLUSH);
    if (hr == S_FALSE || FAILED(hr)) {
        return false;
    }

    outResult.Frequency = result.Frequency;
    outResult.Disjoint = result.Disjoint != FALSE;
    return true;
}
} // namespace luna::RHI

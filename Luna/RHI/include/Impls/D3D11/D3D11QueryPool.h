#ifndef CACAO_D3D11QUERYPOOL_H
#define CACAO_D3D11QUERYPOOL_H
#include "D3D11Common.h"
#include "QueryPool.h"

namespace Cacao {
class D3D11Device;

class CACAO_API D3D11QueryPool final : public QueryPool {
private:
    std::vector<ComPtr<ID3D11Query>> m_queries;
    Ref<D3D11Device> m_device;
    QueryPoolCreateInfo m_createInfo;

public:
    D3D11QueryPool(const Ref<D3D11Device>& device, const QueryPoolCreateInfo& info);

    uint32_t GetCount() const override
    {
        return static_cast<uint32_t>(m_queries.size());
    }

    QueryType GetType() const override
    {
        return m_createInfo.Type;
    }

    void Reset(uint32_t firstQuery, uint32_t count) override;
    bool GetResults(uint32_t firstQuery,
                    uint32_t queryCount,
                    std::vector<uint64_t>& outResults,
                    bool wait = true) override;

    ID3D11Query* GetQuery(uint32_t index) const
    {
        return m_queries[index].Get();
    }
};
} // namespace Cacao

#endif

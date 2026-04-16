#ifndef CACAO_VKQUERYPOOL_H
#define CACAO_VKQUERYPOOL_H
#include "VKCommon.h"

#include <QueryPool.h>

namespace Cacao {
class VKDevice;

class CACAO_API VKQueryPool : public QueryPool {
public:
    VKQueryPool(Ref<VKDevice> device, const QueryPoolCreateInfo& info);
    ~VKQueryPool() override;

    void Reset(uint32_t firstQuery, uint32_t count) override;
    bool GetResults(uint32_t firstQuery, uint32_t queryCount, std::vector<uint64_t>& outResults, bool wait) override;

    QueryType GetType() const override
    {
        return m_type;
    }

    uint32_t GetCount() const override
    {
        return m_count;
    }

    vk::QueryPool GetNativeHandle() const
    {
        return m_pool;
    }

private:
    Ref<VKDevice> m_device;
    vk::QueryPool m_pool;
    QueryType m_type;
    uint32_t m_count;
};
} // namespace Cacao
#endif

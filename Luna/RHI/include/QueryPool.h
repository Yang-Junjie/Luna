#ifndef LUNA_RHI_QUERYPOOL_H
#define LUNA_RHI_QUERYPOOL_H
#include "Core.h"

#include <vector>

namespace luna::RHI {
enum class QueryType {
    Occlusion,
    Timestamp,
    TimestampDisjoint,
    PipelineStatistics
};

struct TimestampDisjointResult {
    uint64_t Frequency{0};
    bool Disjoint{false};
};

struct QueryPoolCreateInfo {
    QueryType Type = QueryType::Timestamp;
    uint32_t Count = 64;
};

class LUNA_RHI_API QueryPool : public std::enable_shared_from_this<QueryPool> {
public:
    virtual ~QueryPool() = default;
    virtual void Reset(uint32_t firstQuery, uint32_t count) = 0;
    virtual bool
        GetResults(uint32_t firstQuery, uint32_t queryCount, std::vector<uint64_t>& outResults, bool wait = true) = 0;
    virtual bool GetTimestampDisjointResult(uint32_t queryIndex,
                                            TimestampDisjointResult& outResult,
                                            bool wait = true)
    {
        return false;
    }
    virtual QueryType GetType() const = 0;
    virtual uint32_t GetCount() const = 0;
};
} // namespace luna::RHI
#endif

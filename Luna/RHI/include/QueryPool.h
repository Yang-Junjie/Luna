#ifndef CACAO_QUERYPOOL_H
#define CACAO_QUERYPOOL_H
#include "Core.h"

#include <vector>

namespace Cacao {
enum class QueryType {
    Occlusion,
    Timestamp,
    PipelineStatistics
};

struct QueryPoolCreateInfo {
    QueryType Type = QueryType::Timestamp;
    uint32_t Count = 64;
};

class CACAO_API QueryPool : public std::enable_shared_from_this<QueryPool> {
public:
    virtual ~QueryPool() = default;
    virtual void Reset(uint32_t firstQuery, uint32_t count) = 0;
    virtual bool
        GetResults(uint32_t firstQuery, uint32_t queryCount, std::vector<uint64_t>& outResults, bool wait = true) = 0;
    virtual QueryType GetType() const = 0;
    virtual uint32_t GetCount() const = 0;
};
} // namespace Cacao
#endif

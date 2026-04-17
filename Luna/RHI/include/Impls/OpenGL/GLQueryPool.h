#ifndef LUNA_RHI_GLQUERYPOOL_H
#define LUNA_RHI_GLQUERYPOOL_H
#include "GLCommon.h"
#include "QueryPool.h"

#include <vector>

namespace luna::RHI {
class LUNA_RHI_API GLQueryPool final : public QueryPool {
public:
    GLQueryPool(const QueryPoolCreateInfo& info);
    static Ref<GLQueryPool> Create(const QueryPoolCreateInfo& info);
    ~GLQueryPool() override;

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

    void Begin(uint32_t index);
    void End(uint32_t index);
    void WriteTimestamp(uint32_t index);

    GLuint GetQuery(uint32_t index) const
    {
        return m_queries[index];
    }

    GLenum GetGLTarget() const
    {
        return m_glTarget;
    }

private:
    std::vector<GLuint> m_queries;
    QueryType m_type;
    uint32_t m_count;
    GLenum m_glTarget;
};
} // namespace luna::RHI

#endif

#include "Impls/OpenGL/GLQueryPool.h"

namespace luna::RHI {
static GLenum QueryTypeToGL(QueryType type)
{
    switch (type) {
        case QueryType::Occlusion:
            return GL_SAMPLES_PASSED;
        case QueryType::Timestamp:
            return GL_TIMESTAMP;
        case QueryType::PipelineStatistics:
            return GL_SAMPLES_PASSED;
        default:
            return GL_SAMPLES_PASSED;
    }
}

GLQueryPool::GLQueryPool(const QueryPoolCreateInfo& info)
    : m_type(info.Type),
      m_count(info.Count)
{
    m_glTarget = QueryTypeToGL(info.Type);
    m_queries.resize(m_count);
    glGenQueries(m_count, m_queries.data());
}

Ref<GLQueryPool> GLQueryPool::Create(const QueryPoolCreateInfo& info)
{
    return std::make_shared<GLQueryPool>(info);
}

GLQueryPool::~GLQueryPool()
{
    if (!m_queries.empty()) {
        glDeleteQueries(static_cast<GLsizei>(m_queries.size()), m_queries.data());
    }
}

void GLQueryPool::Begin(uint32_t index)
{
    if (index < m_count && m_type != QueryType::Timestamp) {
        glBeginQuery(m_glTarget, m_queries[index]);
    }
}

void GLQueryPool::End(uint32_t index)
{
    if (index < m_count && m_type != QueryType::Timestamp) {
        glEndQuery(m_glTarget);
    }
}

void GLQueryPool::WriteTimestamp(uint32_t index)
{
    if (index < m_count && m_type == QueryType::Timestamp) {
        glQueryCounter(m_queries[index], GL_TIMESTAMP);
    }
}

void GLQueryPool::Reset(uint32_t firstQuery, uint32_t count)
{
    glDeleteQueries(static_cast<GLsizei>(m_queries.size()), m_queries.data());
    glGenQueries(static_cast<GLsizei>(m_count), m_queries.data());
}

bool GLQueryPool::GetResults(uint32_t firstQuery, uint32_t queryCount, std::vector<uint64_t>& outResults, bool wait)
{
    outResults.resize(queryCount);
    for (uint32_t i = 0; i < queryCount; i++) {
        uint32_t idx = firstQuery + i;
        if (idx >= m_count) {
            outResults[i] = 0;
            continue;
        }

        if (!wait) {
            GLint available = 0;
            glGetQueryObjectiv(m_queries[idx], GL_QUERY_RESULT_AVAILABLE, &available);
            if (!available) {
                return false;
            }
        }

        GLuint64 value = 0;
        glGetQueryObjectui64v(m_queries[idx], GL_QUERY_RESULT, &value);
        outResults[i] = value;
    }
    return true;
}
} // namespace luna::RHI

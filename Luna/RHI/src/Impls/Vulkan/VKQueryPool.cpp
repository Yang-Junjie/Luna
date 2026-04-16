#include "Impls/Vulkan/VKDevice.h"
#include "Impls/Vulkan/VKQueryPool.h"

namespace Cacao {
static vk::QueryType ToVkQueryType(QueryType type)
{
    switch (type) {
        case QueryType::Occlusion:
            return vk::QueryType::eOcclusion;
        case QueryType::Timestamp:
            return vk::QueryType::eTimestamp;
        case QueryType::PipelineStatistics:
            return vk::QueryType::ePipelineStatistics;
        default:
            return vk::QueryType::eTimestamp;
    }
}

VKQueryPool::VKQueryPool(Ref<VKDevice> device, const QueryPoolCreateInfo& info)
    : m_device(std::move(device)),
      m_type(info.Type),
      m_count(info.Count)
{
    vk::QueryPoolCreateInfo ci{};
    ci.queryType = ToVkQueryType(m_type);
    ci.queryCount = m_count;
    if (m_type == QueryType::PipelineStatistics) {
        ci.pipelineStatistics = vk::QueryPipelineStatisticFlagBits::eInputAssemblyVertices |
                                vk::QueryPipelineStatisticFlagBits::eInputAssemblyPrimitives |
                                vk::QueryPipelineStatisticFlagBits::eVertexShaderInvocations |
                                vk::QueryPipelineStatisticFlagBits::eFragmentShaderInvocations |
                                vk::QueryPipelineStatisticFlagBits::eComputeShaderInvocations;
    }
    m_pool = m_device->GetHandle().createQueryPool(ci);
}

VKQueryPool::~VKQueryPool()
{
    if (m_pool) {
        m_device->GetHandle().destroyQueryPool(m_pool);
    }
}

void VKQueryPool::Reset(uint32_t firstQuery, uint32_t count)
{
    m_device->GetHandle().resetQueryPool(m_pool, firstQuery, count);
}

bool VKQueryPool::GetResults(uint32_t firstQuery, uint32_t queryCount, std::vector<uint64_t>& outResults, bool wait)
{
    outResults.resize(queryCount);
    vk::QueryResultFlags flags = vk::QueryResultFlagBits::e64;
    if (wait) {
        flags |= vk::QueryResultFlagBits::eWait;
    }

    auto result = m_device->GetHandle().getQueryPoolResults(
        m_pool, firstQuery, queryCount, queryCount * sizeof(uint64_t), outResults.data(), sizeof(uint64_t), flags);

    return result == vk::Result::eSuccess;
}
} // namespace Cacao

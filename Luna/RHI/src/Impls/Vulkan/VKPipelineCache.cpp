#include "Impls/Vulkan/VKDevice.h"
#include "Impls/Vulkan/VKPipeline.h"

namespace luna::RHI {
Ref<VKPipelineCache> VKPipelineCache::Create(const Ref<Device>& device, std::span<const uint8_t> initialData)
{
    return CreateRef<VKPipelineCache>(device, initialData);
}

VKPipelineCache::VKPipelineCache(const Ref<Device>& device, std::span<const uint8_t> initialData)
    : m_device(std::dynamic_pointer_cast<VKDevice>(device))
{
    vk::PipelineCacheCreateInfo createInfo = {};
    createInfo.initialDataSize = initialData.size();
    createInfo.pInitialData = initialData.empty() ? nullptr : initialData.data();

    m_pipelineCache = m_device->GetHandle().createPipelineCache(createInfo);
}

VKPipelineCache::~VKPipelineCache()
{
    if (m_device && m_pipelineCache) {
        m_device->GetHandle().destroyPipelineCache(m_pipelineCache);
        m_pipelineCache = nullptr;
    }
}

std::vector<uint8_t> VKPipelineCache::GetData() const
{
    return m_device->GetHandle().getPipelineCacheData(m_pipelineCache);
}

void VKPipelineCache::Merge(std::span<const Ref<PipelineCache>> srcCaches)
{
    std::vector<vk::PipelineCache> vkCaches;
    vkCaches.reserve(srcCaches.size());
    for (const auto& cache : srcCaches) {
        auto vkCache = std::dynamic_pointer_cast<VKPipelineCache>(cache);
        if (vkCache) {
            vkCaches.push_back(vkCache->GetHandle());
        }
    }

    m_device->GetHandle().mergePipelineCaches(m_pipelineCache, vkCaches);
}
} // namespace luna::RHI

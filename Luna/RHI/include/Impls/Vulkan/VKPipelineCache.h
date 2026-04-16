#ifndef CACAO_VKPIPELINECACHE_H
#define CACAO_VKPIPELINECACHE_H

#include "Pipeline.h"
#include "Device.h"
#include <vulkan/vulkan.hpp>

namespace Cacao
{
    class VKDevice;

    class CACAO_API VKPipelineCache final : public PipelineCache
    {
    private:
        vk::PipelineCache m_cache;
        Ref<VKDevice> m_device;

    public:
        VKPipelineCache(const Ref<Device>& device, std::span<const uint8_t> initialData);
        ~VKPipelineCache() override;

        std::vector<uint8_t> GetData() const override;
        void Merge(std::span<const Ref<PipelineCache>> srcCaches) override;

        vk::PipelineCache GetHandle() const { return m_cache; }

        static Ref<VKPipelineCache> Create(const Ref<Device>& device, std::span<const uint8_t> initialData)
        {
            return CreateRef<VKPipelineCache>(device, initialData);
        }
    };
}

#endif

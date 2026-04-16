#ifndef CACAO_VKRAYTRACINGPIPELINE_H
#define CACAO_VKRAYTRACINGPIPELINE_H
#include <vulkan/vulkan.hpp>
#include "Pipeline.h"
#include "Device.h"

namespace Cacao
{
    class VKDevice;

    class CACAO_API VKRayTracingPipeline final : public RayTracingPipeline
    {
    public:
        VKRayTracingPipeline(const Ref<Device>& device, const RayTracingPipelineCreateInfo& info);
        ~VKRayTracingPipeline() override;

        Ref<PipelineLayout> GetLayout() const override { return m_layout; }
        VkPipeline GetHandle() const { return m_pipeline; }

    private:
        VkPipeline m_pipeline = VK_NULL_HANDLE;
        Ref<PipelineLayout> m_layout;
        Ref<Device> m_device;
    };
}

#endif

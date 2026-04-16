#ifndef CACAO_VKPIPELINELAYOUT_H
#define CACAO_VKPIPELINELAYOUT_H
#include "PipelineLayout.h"
#include <vulkan/vulkan.hpp>
namespace Cacao
{
    class Device;
}
namespace Cacao
{
    class VKDevice;
    class CACAO_API VKPipelineLayout : public PipelineLayout
    {
        vk::PipelineLayout m_pipelineLayout;
        Ref<VKDevice> m_device;
        std::vector<vk::DescriptorSetLayout> m_descriptorSetLayouts;
        std::vector<vk::PushConstantRange> m_pushConstantRanges;
    public:
        VKPipelineLayout(const Ref<Device>& device, const PipelineLayoutCreateInfo& info);
        ~VKPipelineLayout();
        static Ref<VKPipelineLayout> Create(const Ref<Device>& device, const PipelineLayoutCreateInfo& info);
        vk::PipelineLayout& GetHandle() { return m_pipelineLayout; }
    };
}
#endif 

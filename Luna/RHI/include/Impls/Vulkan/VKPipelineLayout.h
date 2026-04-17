#ifndef LUNA_RHI_VKPIPELINELAYOUT_H
#define LUNA_RHI_VKPIPELINELAYOUT_H
#include "PipelineLayout.h"

#include <vulkan/vulkan.hpp>

namespace luna::RHI {
class Device;
}

namespace luna::RHI {
class VKDevice;

class LUNA_RHI_API VKPipelineLayout : public PipelineLayout {
    vk::PipelineLayout m_pipelineLayout;
    Ref<VKDevice> m_device;
    std::vector<vk::DescriptorSetLayout> m_descriptorSetLayouts;
    std::vector<vk::PushConstantRange> m_pushConstantRanges;

public:
    VKPipelineLayout(const Ref<Device>& device, const PipelineLayoutCreateInfo& info);
    ~VKPipelineLayout();
    static Ref<VKPipelineLayout> Create(const Ref<Device>& device, const PipelineLayoutCreateInfo& info);

    vk::PipelineLayout& GetHandle()
    {
        return m_pipelineLayout;
    }
};
} // namespace luna::RHI
#endif

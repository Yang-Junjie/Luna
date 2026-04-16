#include "Impls/Vulkan/VKPipelineLayout.h"
#include "Impls/Vulkan/VKCommon.h"
#include "Impls/Vulkan/VKDescriptorSetLayout.h"
#include "Impls/Vulkan/VKDevice.h"
namespace Cacao
{
    VKPipelineLayout::VKPipelineLayout(const Ref<Device>& device, const PipelineLayoutCreateInfo& info)
    {
        if (!device)
        {
            throw std::runtime_error("VKPipelineLayout created with null device");
        }
        m_device = std::dynamic_pointer_cast<VKDevice>(device);
        m_descriptorSetLayouts.resize(info.SetLayouts.size());
        for (size_t i = 0; i < info.SetLayouts.size(); i++)
        {
            if (!info.SetLayouts[i])
            {
                throw std::runtime_error("VKPipelineLayout created with null descriptor set layout");
            }
            m_descriptorSetLayouts[i] = std::dynamic_pointer_cast<VKDescriptorSetLayout>(info.SetLayouts[i])->
                GetHandle();
        }
        m_pushConstantRanges.resize(info.PushConstantRanges.size());
        for (size_t i = 0; i < info.PushConstantRanges.size(); i++)
        {
            const auto& range = info.PushConstantRanges[i];
            vk::PushConstantRange vkRange{};
            vkRange.offset = range.Offset;
            vkRange.size = range.Size;
            vkRange.stageFlags = VKConverter::ConvertShaderStageFlags(range.StageFlags);
            m_pushConstantRanges[i] = vkRange;
        }
        vk::PipelineLayoutCreateInfo createInfo{};
        createInfo.setLayoutCount = static_cast<uint32_t>(m_descriptorSetLayouts.size());
        createInfo.pSetLayouts = m_descriptorSetLayouts.data();
        createInfo.pushConstantRangeCount = static_cast<uint32_t>(m_pushConstantRanges.size());
        createInfo.pPushConstantRanges = m_pushConstantRanges.data();
        m_pipelineLayout = m_device->GetHandle().createPipelineLayout(createInfo);
        if (!m_pipelineLayout)
        {
            throw std::runtime_error("Failed to create Vulkan Pipeline Layout");
        }
    }
    VKPipelineLayout::~VKPipelineLayout()
    {
        if (m_pipelineLayout && m_device)
        {
            m_device->GetHandle().destroyPipelineLayout(m_pipelineLayout);
            m_pipelineLayout = VK_NULL_HANDLE;
        }
    }
    Ref<VKPipelineLayout> VKPipelineLayout::Create(const Ref<Device>& device, const PipelineLayoutCreateInfo& info)
    {
        return CreateRef<VKPipelineLayout>(device, info);
    }
}

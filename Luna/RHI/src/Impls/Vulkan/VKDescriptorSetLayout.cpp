#include "Impls/Vulkan/VKDescriptorSetLayout.h"
#include "Impls/Vulkan/VKCommon.h"
#include "Impls/Vulkan/VKDevice.h"
#include "Impls/Vulkan/VKSampler.h"
namespace Cacao
{
    static std::vector<vk::Sampler> ConvertSamplers(std::span<const Ref<Sampler>> samplers)
    {
        std::vector<vk::Sampler> vkSamplers;
        vkSamplers.reserve(samplers.size());
        for (const auto& sampler : samplers)
        {
            auto vkSampler = std::dynamic_pointer_cast<VKSampler>(sampler);
            vkSamplers.push_back(vkSampler->GetHandle());
        }
        return vkSamplers;
    }
    static vk::DescriptorSetLayoutBinding ConvertBinding(const DescriptorSetLayoutBinding& binding)
    {
        vk::DescriptorSetLayoutBinding vkBinding{};
        vkBinding.binding = binding.Binding;
        vkBinding.descriptorType = VKConverter::Convert(binding.Type);
        vkBinding.descriptorCount = binding.Count;
        vkBinding.stageFlags = VKConverter::ConvertShaderStageFlags(binding.StageFlags);
        return vkBinding;
    }
    VKDescriptorSetLayout::VKDescriptorSetLayout(const Ref<Device>& device,
                                                 const DescriptorSetLayoutCreateInfo& info)
    {
        if (!device)
        {
            throw std::runtime_error("VKDescriptorLayout created with null device");
        }
        m_device = std::dynamic_pointer_cast<VKDevice>(device);
        std::vector<vk::DescriptorSetLayoutBinding> vkBindings(info.Bindings.size());
        m_samplerStorage.resize(info.Bindings.size());
        for (uint32_t i = 0; i < info.Bindings.size(); ++i)
        {
            vkBindings[i] = ConvertBinding(info.Bindings[i]);
            m_samplerStorage[i] = ConvertSamplers(info.Bindings[i].ImmutableSamplers);
            vkBindings[i].pImmutableSamplers = m_samplerStorage[i].data();
        }
        vk::DescriptorSetLayoutCreateInfo layoutCreateInfo{};
        layoutCreateInfo.bindingCount = static_cast<uint32_t>(vkBindings.size());
        layoutCreateInfo.pBindings = vkBindings.data();
        m_descriptorSetLayout = m_device->GetHandle().createDescriptorSetLayout(layoutCreateInfo);
        if (!m_descriptorSetLayout)
        {
            throw std::runtime_error("Failed to create Vulkan Descriptor Set Layout");
        }
    }
    Ref<VKDescriptorSetLayout> VKDescriptorSetLayout::Create(const Ref<Device>& device,
                                                             const DescriptorSetLayoutCreateInfo& info)
    {
        return CreateRef<VKDescriptorSetLayout>(device, info);
    }
}

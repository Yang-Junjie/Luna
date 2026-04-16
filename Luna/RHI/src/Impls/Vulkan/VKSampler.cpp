#include "Impls/Vulkan/VKSampler.h"
#include "Impls/Vulkan/VKCommon.h"
#include "Impls/Vulkan/VKDevice.h"
namespace Cacao
{
    VKSampler::VKSampler(const Ref<Device>& device, const SamplerCreateInfo& createInfo)
    {
        if (!device)
        {
            throw std::runtime_error("VKSampler created with null device");
        }
        m_device = std::static_pointer_cast<VKDevice>(device);
        vk::SamplerCreateInfo samplerInfo{};
        samplerInfo.setAddressModeU(VKConverter::Convert(createInfo.AddressModeU))
                   .setAddressModeV(VKConverter::Convert(createInfo.AddressModeV))
                   .setAddressModeW(VKConverter::Convert(createInfo.AddressModeW))
                   .setMagFilter(VKConverter::Convert(createInfo.MagFilter))
                   .setMinFilter(VKConverter::Convert(createInfo.MinFilter))
                   .setMipmapMode(VKConverter::Convert(createInfo.MipmapMode))
                   .setMipLodBias(createInfo.MipLodBias)
                   .setAnisotropyEnable(createInfo.AnisotropyEnable)
                   .setMaxAnisotropy(createInfo.MaxAnisotropy)
                   .setCompareEnable(createInfo.CompareEnable)
                   .setCompareOp(VKConverter::Convert(createInfo.CompareOp))
                   .setMinLod(createInfo.MinLod)
                   .setMaxLod(createInfo.MaxLod)
                   .setBorderColor(VKConverter::Convert(createInfo.BorderColor))
                   .setUnnormalizedCoordinates(VK_FALSE);
        m_sampler = m_device->GetHandle().createSampler(samplerInfo);
        if (!m_sampler)
        {
            throw std::runtime_error("Failed to create Vulkan sampler");
        }
    }
    Ref<VKSampler> VKSampler::Create(const Ref<Device>& device, const SamplerCreateInfo& createInfo)
    {
        return CreateRef<VKSampler>(device, createInfo);
    }
    const SamplerCreateInfo& VKSampler::GetInfo() const
    {
        return m_createInfo;
    }
}

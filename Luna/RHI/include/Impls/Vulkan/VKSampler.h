#ifndef CACAO_VKSAMPLER_H
#define CACAO_VKSAMPLER_H
#include "Sampler.h"
#include <vulkan/vulkan.hpp>
namespace Cacao
{
    class Device;
    class VKDevice;
    class CACAO_API VKSampler : public Sampler
    {
    private:
        vk::Sampler m_sampler;
        Ref<VKDevice> m_device;
        SamplerCreateInfo m_createInfo;
        friend class VKDescriptorSet;
        friend class VKDescriptorSetLayout;
    public:
        VKSampler(const Ref<Device>& device, const SamplerCreateInfo& createInfo);
        static Ref<VKSampler> Create(const Ref<Device>& device, const SamplerCreateInfo& createInfo);
        const SamplerCreateInfo& GetInfo() const override;
        vk::Sampler& GetHandle()
        {
            return m_sampler;
        }
    };
}
#endif 

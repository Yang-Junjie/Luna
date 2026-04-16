#ifndef CACAO_VKDESCRIPTORPOOL_H
#define CACAO_VKDESCRIPTORPOOL_H
#include "DescriptorPool.h"
#include <vulkan/vulkan.hpp>
namespace Cacao
{
    class Device;
    class VKDevice;
    class CACAO_API VKDescriptorPool : public DescriptorPool
    {
        Ref<VKDevice> m_device;
        vk::DescriptorPool m_descriptorPool;
        DescriptorPoolCreateInfo m_createInfo;
    public:
        VKDescriptorPool(const Ref<Device>& device, const DescriptorPoolCreateInfo& createInfo);
        static Ref<VKDescriptorPool> Create(const Ref<Device>& device, const DescriptorPoolCreateInfo& createInfo);
        void Reset() override;
        Ref<DescriptorSet> AllocateDescriptorSet(const Ref<DescriptorSetLayout>& layout) override;
    };
}
#endif 

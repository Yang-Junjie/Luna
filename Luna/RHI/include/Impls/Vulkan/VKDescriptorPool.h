#ifndef LUNA_RHI_VKDESCRIPTORPOOL_H
#define LUNA_RHI_VKDESCRIPTORPOOL_H
#include "DescriptorPool.h"

#include <vulkan/vulkan.hpp>

namespace luna::RHI {
class Device;
class VKDevice;

class LUNA_RHI_API VKDescriptorPool : public DescriptorPool {
    Ref<VKDevice> m_device;
    vk::DescriptorPool m_descriptorPool;
    DescriptorPoolCreateInfo m_createInfo;

public:
    VKDescriptorPool(const Ref<Device>& device, const DescriptorPoolCreateInfo& createInfo);
    static Ref<VKDescriptorPool> Create(const Ref<Device>& device, const DescriptorPoolCreateInfo& createInfo);
    void Reset() override;
    Ref<DescriptorSet> AllocateDescriptorSet(const Ref<DescriptorSetLayout>& layout) override;
};
} // namespace luna::RHI
#endif

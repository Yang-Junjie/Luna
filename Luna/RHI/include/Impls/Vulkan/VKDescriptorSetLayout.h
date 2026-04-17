#ifndef LUNA_RHI_VKDESCRIPTOR_H
#define LUNA_RHI_VKDESCRIPTOR_H
#include "DescriptorSetLayout.h"

#include <vulkan/vulkan.hpp>

namespace luna::RHI {
class VKDevice;
class Device;

class LUNA_RHI_API VKDescriptorSetLayout : public DescriptorSetLayout {
    vk::DescriptorSetLayout m_descriptorSetLayout;
    Ref<VKDevice> m_device;
    std::vector<std::vector<vk::Sampler>> m_samplerStorage;

public:
    VKDescriptorSetLayout(const Ref<Device>& device, const DescriptorSetLayoutCreateInfo& info);
    static Ref<VKDescriptorSetLayout> Create(const Ref<Device>& device, const DescriptorSetLayoutCreateInfo& info);

    vk::DescriptorSetLayout& GetHandle()
    {
        return m_descriptorSetLayout;
    }
};
} // namespace luna::RHI
#endif

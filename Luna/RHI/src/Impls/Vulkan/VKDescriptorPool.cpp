#include "Impls/Vulkan/VKDescriptorPool.h"
#include "Impls/Vulkan/VKDescriptorSet.h"
#include "Impls/Vulkan/VKDescriptorSetLayout.h"
#include "Impls/Vulkan/VKDevice.h"
namespace Cacao
{
    VKDescriptorPool::VKDescriptorPool(const Ref<Device>& device, const DescriptorPoolCreateInfo& createInfo)
    {
        if (!device)
        {
            throw std::runtime_error("VKDescriptorPool created with null device");
        }
        m_device = std::static_pointer_cast<VKDevice>(device);
        m_createInfo = createInfo;
        vk::DescriptorPoolCreateInfo poolInfo = {};
        poolInfo.maxSets = m_createInfo.MaxSets;
        std::vector<vk::DescriptorPoolSize> vkPoolSizes;
        for (const auto& size : m_createInfo.PoolSizes)
        {
            vk::DescriptorPoolSize vkSize = {};
            switch (size.Type)
            {
            case DescriptorType::Sampler:
                vkSize.type = vk::DescriptorType::eSampler;
                break;
            case DescriptorType::CombinedImageSampler:
                vkSize.type = vk::DescriptorType::eCombinedImageSampler;
                break;
            case DescriptorType::SampledImage:
                vkSize.type = vk::DescriptorType::eSampledImage;
                break;
            case DescriptorType::StorageImage:
                vkSize.type = vk::DescriptorType::eStorageImage;
                break;
            case DescriptorType::UniformBuffer:
                vkSize.type = vk::DescriptorType::eUniformBuffer;
                break;
            case DescriptorType::StorageBuffer:
                vkSize.type = vk::DescriptorType::eStorageBuffer;
                break;
            case DescriptorType::UniformBufferDynamic:
                vkSize.type = vk::DescriptorType::eUniformBufferDynamic;
                break;
            case DescriptorType::StorageBufferDynamic:
                vkSize.type = vk::DescriptorType::eStorageBufferDynamic;
                break;
            case DescriptorType::InputAttachment:
                vkSize.type = vk::DescriptorType::eInputAttachment;
                break;
            case DescriptorType::AccelerationStructure:
                vkSize.type = vk::DescriptorType::eAccelerationStructureKHR;
                break;
            default:
                throw std::runtime_error("Unsupported DescriptorType in VKDescriptorPool");
            }
            vkSize.descriptorCount = size.Count;
            vkPoolSizes.push_back(vkSize);
        }
        poolInfo.poolSizeCount = static_cast<uint32_t>(vkPoolSizes.size());
        poolInfo.pPoolSizes = vkPoolSizes.data();
        try
        {
            m_descriptorPool = m_device->GetHandle().createDescriptorPool(poolInfo);
        }
        catch (const vk::SystemError& err)
        {
            throw std::runtime_error("Failed to create Vulkan descriptor pool");
        }
    }
    Ref<VKDescriptorPool> VKDescriptorPool::Create(const Ref<Device>& device,
                                                   const DescriptorPoolCreateInfo& createInfo)
    {
        return CreateRef<VKDescriptorPool>(device, createInfo);
    }
    void VKDescriptorPool::Reset()
    {
        m_device->GetHandle().resetDescriptorPool(m_descriptorPool);
    }
    Ref<DescriptorSet> VKDescriptorPool::AllocateDescriptorSet(const Ref<DescriptorSetLayout>& layout)
    {
        if (!layout)
        {
            throw std::runtime_error("AllocateDescriptorSet called with null layout");
        }
        auto vkLayout = std::dynamic_pointer_cast<VKDescriptorSetLayout>(layout);
        vk::DescriptorSetAllocateInfo allocInfo = {};
        allocInfo.descriptorPool = m_descriptorPool;
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts = &vkLayout->GetHandle();
        vk::DescriptorSet vkDescriptorSet;
        try
        {
            vkDescriptorSet = m_device->GetHandle().allocateDescriptorSets(allocInfo).front();
        }
        catch (const vk::SystemError& err)
        {
            throw std::runtime_error("Failed to allocate descriptor set from Vulkan descriptor pool");
        }
        return VKDescriptorSet::Create(m_device, shared_from_this(), layout, vkDescriptorSet);
    }
}

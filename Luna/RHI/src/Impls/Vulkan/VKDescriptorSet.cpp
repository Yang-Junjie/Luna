#include "Impls/Vulkan/VKDescriptorSet.h"
#include "Impls/Vulkan/VKCommon.h"
#include "Impls/Vulkan/VKBuffer.h"
#include "Impls/Vulkan/VKDescriptorPool.h"
#include "Impls/Vulkan/VKDescriptorSetLayout.h"
#include "Impls/Vulkan/VKDevice.h"
#include "Impls/Vulkan/VKTexture.h"
#include "Impls/Vulkan/VKSampler.h"
#include "Impls/Vulkan/VKAccelerationStructure.h"

namespace Cacao
{
    vk::DescriptorBufferInfo VKDescriptorSet::ConvertToVkBufferInfo(const BufferWriteInfo& info)
    {
        vk::DescriptorBufferInfo vkInfo = {};
        auto vkBuffer = std::static_pointer_cast<VKBuffer>(info.Buffer);
        vkInfo.buffer = vkBuffer->GetHandle();
        vkInfo.offset = info.Offset;
        vkInfo.range = info.Size == UINT64_MAX ? vkBuffer->GetSize() - info.Offset : info.Size;
        return vkInfo;
    }

    vk::DescriptorImageInfo VKDescriptorSet::ConvertToVkImageInfo(const TextureWriteInfo& info)
    {
        vk::DescriptorImageInfo vkInfo = {};
        if (info.TextureView)
        {
            auto vkView = std::static_pointer_cast<VKTextureView>(info.TextureView);
            vkInfo.imageView = vkView->GetHandle();
            vkInfo.imageLayout = VKConverter::ConvertResourceStateToLayout(info.Layout);
        }
        if (info.Sampler)
        {
            auto vkSampler = std::static_pointer_cast<VKSampler>(info.Sampler);
            vkInfo.sampler = vkSampler->GetHandle();
        }
        return vkInfo;
    }

    VKDescriptorSet::VKDescriptorSet(const Ref<Device>& device, const Ref<DescriptorPool>& parent,
                                     const Ref<DescriptorSetLayout>& layout,
                                     const vk::DescriptorSet& descriptorSet)
    {
        if (!device)
        {
            throw std::runtime_error("VKDescriptorSet created with null device");
        }
        m_device = std::static_pointer_cast<VKDevice>(device);
        if (!parent)
        {
            throw std::runtime_error("VKDescriptorSet created with null parent pool");
        }
        if (!layout)
        {
            throw std::runtime_error("VKDescriptorSet created with null layout");
        }
        m_parentPool = std::static_pointer_cast<VKDescriptorPool>(parent);
        m_layout = std::static_pointer_cast<VKDescriptorSetLayout>(layout);
        m_descriptorSet = descriptorSet;
        if (!m_descriptorSet)
        {
            throw std::runtime_error("VKDescriptorSet created with invalid vk::DescriptorSet");
        }
    }

    Ref<VKDescriptorSet> VKDescriptorSet::Create(const Ref<Device>& device, const Ref<DescriptorPool>& parent,
                                                 const Ref<DescriptorSetLayout>& layout,
                                                 const vk::DescriptorSet& descriptorSet)
    {
        return CreateRef<VKDescriptorSet>(device, parent, layout, descriptorSet);
    }

    void VKDescriptorSet::Update()
    {
        if (!m_pendingWrites.empty())
        {
            m_device->GetHandle().updateDescriptorSets(
                static_cast<uint32_t>(m_pendingWrites.size()),
                m_pendingWrites.data(),
                0,
                nullptr);
            m_pendingWrites.clear();
            m_pendingBufferInfos.clear();
            m_pendingImageInfos.clear();
            m_pendingASInfos.clear();
            m_pendingASHandles.clear();
            m_pendingBufferInfoArrays.clear();
            m_pendingImageInfoArrays.clear();
            m_pendingASHandleArrays.clear();
        }
    }

    void VKDescriptorSet::WriteBuffer(const BufferWriteInfo& info)
    {
        m_pendingBufferInfos.push_back(ConvertToVkBufferInfo(info));
        vk::DescriptorBufferInfo* pBufferInfo = &m_pendingBufferInfos.back();
        vk::WriteDescriptorSet vkWriteDescriptorSet = {};
        vkWriteDescriptorSet.dstSet = m_descriptorSet;
        vkWriteDescriptorSet.dstBinding = info.Binding;
        vkWriteDescriptorSet.dstArrayElement = info.ArrayElement;
        vkWriteDescriptorSet.descriptorType = VKConverter::Convert(info.Type);
        vkWriteDescriptorSet.descriptorCount = 1;
        vkWriteDescriptorSet.pBufferInfo = pBufferInfo;
        m_pendingWrites.push_back(vkWriteDescriptorSet);
    }

    void VKDescriptorSet::WriteTexture(const TextureWriteInfo& info)
    {
        m_pendingImageInfos.push_back(ConvertToVkImageInfo(info));
        vk::DescriptorImageInfo* pImageInfo = &m_pendingImageInfos.back();
        vk::WriteDescriptorSet vkWriteDescriptorSet = {};
        vkWriteDescriptorSet.dstSet = m_descriptorSet;
        vkWriteDescriptorSet.dstBinding = info.Binding;
        vkWriteDescriptorSet.dstArrayElement = info.ArrayElement;
        vkWriteDescriptorSet.descriptorType = VKConverter::Convert(info.Type);
        vkWriteDescriptorSet.descriptorCount = 1;
        vkWriteDescriptorSet.pImageInfo = pImageInfo;
        m_pendingWrites.push_back(vkWriteDescriptorSet);
    }

    void VKDescriptorSet::WriteSampler(const SamplerWriteInfo& info)
    {
        vk::DescriptorImageInfo vkInfo = {};
        if (info.Sampler)
        {
            auto vkSampler = std::static_pointer_cast<VKSampler>(info.Sampler);
            vkInfo.sampler = vkSampler->GetHandle();
        }
        m_pendingImageInfos.push_back(vkInfo);
        vk::DescriptorImageInfo* pImageInfo = &m_pendingImageInfos.back();
        vk::WriteDescriptorSet vkWriteDescriptorSet = {};
        vkWriteDescriptorSet.dstSet = m_descriptorSet;
        vkWriteDescriptorSet.dstBinding = info.Binding;
        vkWriteDescriptorSet.dstArrayElement = info.ArrayElement;
        vkWriteDescriptorSet.descriptorType = vk::DescriptorType::eSampler;
        vkWriteDescriptorSet.descriptorCount = 1;
        vkWriteDescriptorSet.pImageInfo = pImageInfo;
        m_pendingWrites.push_back(vkWriteDescriptorSet);
    }

    void VKDescriptorSet::WriteAccelerationStructure(const AccelerationStructureWriteInfo& info)
    {
        auto* vkAS = static_cast<const VKAccelerationStructure*>(info.AccelerationStructureHandle);
        m_pendingASHandles.push_back(vk::AccelerationStructureKHR(vkAS->GetHandle()));
        vk::AccelerationStructureKHR* pASHandle = &m_pendingASHandles.back();
        vk::WriteDescriptorSetAccelerationStructureKHR vkASInfo = {};
        vkASInfo.accelerationStructureCount = 1;
        vkASInfo.pAccelerationStructures = pASHandle;
        m_pendingASInfos.push_back(vkASInfo);
        vk::WriteDescriptorSetAccelerationStructureKHR* pASInfo = &m_pendingASInfos.back();
        vk::WriteDescriptorSet vkWriteDescriptorSet = {};
        vkWriteDescriptorSet.dstSet = m_descriptorSet;
        vkWriteDescriptorSet.dstBinding = info.Binding;
        vkWriteDescriptorSet.dstArrayElement = 0;
        vkWriteDescriptorSet.descriptorType = VKConverter::Convert(info.Type);
        vkWriteDescriptorSet.descriptorCount = 1;
        vkWriteDescriptorSet.pNext = pASInfo;
        m_pendingWrites.push_back(vkWriteDescriptorSet);
    }

    void VKDescriptorSet::WriteBuffers(const BufferWriteInfos& infos)
    {
        std::vector<vk::DescriptorBufferInfo> vkBufferInfos;
        vkBufferInfos.resize(infos.Buffers.size());
        for (size_t i = 0; i < infos.Buffers.size(); ++i)
        {
            vkBufferInfos[i] = ConvertToVkBufferInfo(
                BufferWriteInfo{
                    infos.Binding,
                    infos.Buffers[i],
                    infos.Offsets.size() > i ? infos.Offsets[i] : 0,
                    infos.Strides.size() > i ? infos.Strides[i] : 0,
                    infos.Sizes.size() > i ? infos.Sizes[i] : UINT64_MAX,
                    infos.Type,
                    infos.ArrayElement + static_cast<uint32_t>(i)
                });
        }
        m_pendingBufferInfoArrays.push_back(std::move(vkBufferInfos));
        std::vector<vk::DescriptorBufferInfo>* pBufferInfoArray = &m_pendingBufferInfoArrays.back();
        vk::WriteDescriptorSet vkWriteDescriptorSet = {};
        vkWriteDescriptorSet.dstSet = m_descriptorSet;
        vkWriteDescriptorSet.dstBinding = infos.Binding;
        vkWriteDescriptorSet.dstArrayElement = infos.ArrayElement;
        vkWriteDescriptorSet.descriptorType = VKConverter::Convert(infos.Type);
        vkWriteDescriptorSet.descriptorCount = static_cast<uint32_t>(pBufferInfoArray->size());
        vkWriteDescriptorSet.pBufferInfo = pBufferInfoArray->data();
        m_pendingWrites.push_back(vkWriteDescriptorSet);
    }

    void VKDescriptorSet::WriteTextures(const TextureWriteInfos& infos)
    {
        std::vector<vk::DescriptorImageInfo> vkImageInfos;
        vkImageInfos.resize(infos.TextureViews.size());
        for (size_t i = 0; i < infos.TextureViews.size(); ++i)
        {
            vkImageInfos[i] = ConvertToVkImageInfo(
                TextureWriteInfo{
                    infos.Binding,
                    infos.TextureViews[i],
                    infos.Layouts.size() > i ? infos.Layouts[i] : ResourceState::ShaderRead,
                    infos.Type,
                    infos.Samplers.size() > i ? infos.Samplers[i] : nullptr,
                    infos.ArrayElement + static_cast<uint32_t>(i)
                });
        }
        m_pendingImageInfoArrays.push_back(std::move(vkImageInfos));
        std::vector<vk::DescriptorImageInfo>* pImageInfoArray = &m_pendingImageInfoArrays.back();
        vk::WriteDescriptorSet vkWriteDescriptorSet = {};
        vkWriteDescriptorSet.dstSet = m_descriptorSet;
        vkWriteDescriptorSet.dstBinding = infos.Binding;
        vkWriteDescriptorSet.dstArrayElement = infos.ArrayElement;
        vkWriteDescriptorSet.descriptorType = VKConverter::Convert(infos.Type);
        vkWriteDescriptorSet.descriptorCount = static_cast<uint32_t>(pImageInfoArray->size());
        vkWriteDescriptorSet.pImageInfo = pImageInfoArray->data();
        m_pendingWrites.push_back(vkWriteDescriptorSet);
    }

    void VKDescriptorSet::WriteSamplers(const SamplerWriteInfos& infos)
    {
        std::vector<vk::DescriptorImageInfo> vkImageInfos;
        vkImageInfos.resize(infos.Samplers.size());
        for (size_t i = 0; i < infos.Samplers.size(); ++i)
        {
            vk::DescriptorImageInfo vkInfo = {};
            if (infos.Samplers[i])
            {
                auto vkSampler = std::static_pointer_cast<VKSampler>(infos.Samplers[i]);
                vkInfo.sampler = vkSampler->GetHandle();
            }
            vkImageInfos[i] = vkInfo;
        }
        m_pendingImageInfoArrays.push_back(std::move(vkImageInfos));
        std::vector<vk::DescriptorImageInfo>* pImageInfoArray = &m_pendingImageInfoArrays.back();
        vk::WriteDescriptorSet vkWriteDescriptorSet = {};
        vkWriteDescriptorSet.dstSet = m_descriptorSet;
        vkWriteDescriptorSet.dstBinding = infos.Binding;
        vkWriteDescriptorSet.dstArrayElement = infos.ArrayElement;
        vkWriteDescriptorSet.descriptorType = vk::DescriptorType::eSampler;
        vkWriteDescriptorSet.descriptorCount = static_cast<uint32_t>(pImageInfoArray->size());
        vkWriteDescriptorSet.pImageInfo = pImageInfoArray->data();
        m_pendingWrites.push_back(vkWriteDescriptorSet);
    }

    void VKDescriptorSet::WriteAccelerationStructures(const AccelerationStructureWriteInfos& infos)
    {
        std::vector<vk::AccelerationStructureKHR> vkASHandles;
        vkASHandles.resize(infos.AccelerationStructureHandles.size());
        for (size_t i = 0; i < infos.AccelerationStructureHandles.size(); ++i)
        {
            vkASHandles[i] = *static_cast<const vk::AccelerationStructureKHR*>(
                infos.AccelerationStructureHandles[i]);
        }
        m_pendingASHandleArrays.push_back(std::move(vkASHandles));
        std::vector<vk::AccelerationStructureKHR>* pASHandleArray = &m_pendingASHandleArrays.back();
        vk::WriteDescriptorSetAccelerationStructureKHR vkASInfo = {};
        vkASInfo.accelerationStructureCount = static_cast<uint32_t>(pASHandleArray->size());
        vkASInfo.pAccelerationStructures = pASHandleArray->data();
        m_pendingASInfos.push_back(vkASInfo);
        vk::WriteDescriptorSetAccelerationStructureKHR* pASInfo = &m_pendingASInfos.back();
        vk::WriteDescriptorSet vkWriteDescriptorSet = {};
        vkWriteDescriptorSet.dstSet = m_descriptorSet;
        vkWriteDescriptorSet.dstBinding = infos.Binding;
        vkWriteDescriptorSet.dstArrayElement = infos.ArrayElement;
        vkWriteDescriptorSet.descriptorType = VKConverter::Convert(infos.Type);
        vkWriteDescriptorSet.descriptorCount = static_cast<uint32_t>(pASHandleArray->size());
        vkWriteDescriptorSet.pNext = pASInfo;
        m_pendingWrites.push_back(vkWriteDescriptorSet);
    }
}

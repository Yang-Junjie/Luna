#ifndef CACAO_VKDESCRIPTORSET_H
#define CACAO_VKDESCRIPTORSET_H
#include <list>
#include "DescriptorSet.h"
#include <vulkan/vulkan.hpp>
namespace Cacao
{
    class VKDescriptorSetLayout;
    class VKDescriptorPool;
    class Device;
    class VKDevice;
    class CACAO_API VKDescriptorSet : public DescriptorSet
    {
    private:
        vk::DescriptorSet m_descriptorSet;
        Ref<VKDevice> m_device;
        Ref<VKDescriptorPool> m_parentPool;
        Ref<VKDescriptorSetLayout> m_layout;
        std::vector<vk::WriteDescriptorSet> m_pendingWrites;
        std::list<vk::DescriptorBufferInfo> m_pendingBufferInfos;
        std::list<vk::DescriptorImageInfo> m_pendingImageInfos;
        std::list<vk::WriteDescriptorSetAccelerationStructureKHR> m_pendingASInfos;
        std::list<vk::AccelerationStructureKHR> m_pendingASHandles;
        std::list<std::vector<vk::DescriptorBufferInfo>> m_pendingBufferInfoArrays;
        std::list<std::vector<vk::DescriptorImageInfo>> m_pendingImageInfoArrays;
        std::list<std::vector<vk::AccelerationStructureKHR>> m_pendingASHandleArrays;
    public:
        static vk::DescriptorBufferInfo ConvertToVkBufferInfo(const BufferWriteInfo& info);
        static vk::DescriptorImageInfo ConvertToVkImageInfo(const TextureWriteInfo& info);
        VKDescriptorSet(const Ref<Device>& device, const Ref<DescriptorPool>& parent,
                        const Ref<DescriptorSetLayout>& layout,
                        const vk::DescriptorSet& descriptorSet);
        static Ref<VKDescriptorSet> Create(const Ref<Device>& device, const Ref<DescriptorPool>& parent,
                                           const Ref<DescriptorSetLayout>& layout,
                                           const vk::DescriptorSet& descriptorSet);
        void Update() override;
        void WriteBuffer(const BufferWriteInfo& info) override;
        void WriteTexture(const TextureWriteInfo& info) override;
        void WriteSampler(const SamplerWriteInfo& info) override;
        void WriteAccelerationStructure(const AccelerationStructureWriteInfo& info) override;
        void WriteBuffers(const BufferWriteInfos& infos) override;
        void WriteTextures(const TextureWriteInfos& infos) override;
        void WriteSamplers(const SamplerWriteInfos& infos) override;
        void WriteAccelerationStructures(const AccelerationStructureWriteInfos& infos) override;
        vk::DescriptorSet& GetHandle() { return m_descriptorSet; }
    };
}
#endif

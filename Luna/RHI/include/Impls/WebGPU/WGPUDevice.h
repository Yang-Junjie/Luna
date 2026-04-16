#ifndef CACAO_WGPU_DEVICE_H
#define CACAO_WGPU_DEVICE_H

#include <webgpu/webgpu.h>
#include "Device.h"

namespace Cacao
{
    class WGPUAdapter;

    class CACAO_API WGPUDevice final : public Device
    {
    private:
        Ref<Adapter> m_parentAdapter;
        ::WGPUDevice m_device = nullptr;
        ::WGPUQueue m_queue = nullptr;

    public:
        WGPUDevice(Ref<Adapter> adapter, const DeviceCreateInfo& info);
        ~WGPUDevice() override;

        ::WGPUDevice GetHandle() const { return m_device; }
        ::WGPUQueue GetNativeQueue() const { return m_queue; }

        Ref<Queue> GetQueue(QueueType type, uint32_t index) override;
        Ref<Swapchain> CreateSwapchain(const SwapchainCreateInfo& createInfo) override;
        std::vector<uint32_t> GetAllQueueFamilyIndices() const override;
        Ref<Adapter> GetParentAdapter() const override;
        Ref<CommandBufferEncoder> CreateCommandBufferEncoder(CommandBufferType type) override;
        void ResetCommandPool() override;
        void ReturnCommandBuffer(const Ref<CommandBufferEncoder>& encoder) override;
        void FreeCommandBuffer(const Ref<CommandBufferEncoder>& encoder) override;
        void ResetCommandBuffer(const Ref<CommandBufferEncoder>& encoder) override;
        Ref<Texture> CreateTexture(const TextureCreateInfo& createInfo) override;
        Ref<Buffer> CreateBuffer(const BufferCreateInfo& createInfo) override;
        Ref<Sampler> CreateSampler(const SamplerCreateInfo& createInfo) override;
        Ref<DescriptorSetLayout> CreateDescriptorSetLayout(const DescriptorSetLayoutCreateInfo& info) override;
        Ref<DescriptorPool> CreateDescriptorPool(const DescriptorPoolCreateInfo& info) override;
        Ref<ShaderModule> CreateShaderModule(const ShaderBlob& blob, const ShaderCreateInfo& info) override;
        Ref<PipelineLayout> CreatePipelineLayout(const PipelineLayoutCreateInfo& info) override;
        Ref<PipelineCache> CreatePipelineCache(std::span<const uint8_t> initialData) override;
        Ref<GraphicsPipeline> CreateGraphicsPipeline(const GraphicsPipelineCreateInfo& info) override;
        Ref<ComputePipeline> CreateComputePipeline(const ComputePipelineCreateInfo& info) override;
        Ref<Synchronization> CreateSynchronization(uint32_t maxFramesInFlight) override;
    };
}

#endif

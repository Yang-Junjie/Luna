#ifndef LUNA_RHI_D3D11DEVICE_H
#define LUNA_RHI_D3D11DEVICE_H
#include "D3D11Common.h"

#include <Adapter.h>
#include <Device.h>
#include <Queue.h>

namespace luna::RHI {
class D3D11Adapter;
class D3D11Device;

class LUNA_RHI_API D3D11Queue : public Queue {
public:
    D3D11Queue(Ref<D3D11Device> device)
        : m_device(std::move(device))
    {}

    QueueType GetType() const override
    {
        return QueueType::Graphics;
    }

    uint32_t GetIndex() const override
    {
        return 0;
    }

    void Submit(const Ref<CommandBufferEncoder>& cmd, const Ref<Synchronization>& sync, uint32_t frameIndex) override {}

    void Submit(std::span<const Ref<CommandBufferEncoder>> cmds,
                const Ref<Synchronization>& sync,
                uint32_t frameIndex) override
    {}

    void Submit(const Ref<CommandBufferEncoder>& cmd) override {}

    void WaitIdle() override {}

private:
    Ref<D3D11Device> m_device;
};

class LUNA_RHI_API D3D11Device : public Device {
public:
    D3D11Device(Ref<D3D11Adapter> adapter);

    Ref<Queue> GetQueue(QueueType type, uint32_t index) override;
    Ref<Swapchain> CreateSwapchain(const SwapchainCreateInfo& createInfo) override;

    std::vector<uint32_t> GetAllQueueFamilyIndices() const override
    {
        return {0};
    }

    Ref<Adapter> GetParentAdapter() const override;
    Ref<CommandBufferEncoder> CreateCommandBufferEncoder(CommandBufferType type) override;

    void ResetCommandPool() override {}

    void ReturnCommandBuffer(const Ref<CommandBufferEncoder>& encoder) override {}

    void FreeCommandBuffer(const Ref<CommandBufferEncoder>& encoder) override {}

    void ResetCommandBuffer(const Ref<CommandBufferEncoder>& encoder) override {}

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

    ID3D11Device5* GetNativeDevice() const
    {
        return m_device.Get();
    }

    ID3D11DeviceContext4* GetImmediateContext() const
    {
        return m_immediateContext.Get();
    }

private:
    bool InitDevice();

    Ref<D3D11Adapter> m_adapter;
    ComPtr<ID3D11Device5> m_device;
    ComPtr<ID3D11DeviceContext4> m_immediateContext;
    D3D_FEATURE_LEVEL m_featureLevel = D3D_FEATURE_LEVEL_11_0;
    Ref<D3D11Queue> m_pseudoQueue;
};
} // namespace luna::RHI
#endif

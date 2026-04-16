#ifndef CACAO_MTL_DEVICE_H
#define CACAO_MTL_DEVICE_H

#ifdef __APPLE__

#include "Device.h"

#ifdef __OBJC__
#import <Metal/Metal.h>
#endif

namespace Cacao {
class MTLAdapter;

class CACAO_API MTLDevice final : public Device {
private:
    Ref<Adapter> m_parentAdapter;
#ifdef __OBJC__
    id<MTLDevice> m_device;
    id<MTLCommandQueue> m_commandQueue;
#else
    void* m_device;
    void* m_commandQueue;
#endif

public:
    MTLDevice(Ref<Adapter> adapter, const DeviceCreateInfo& info);
    ~MTLDevice() override;

#ifdef __OBJC__
    id<MTLDevice> GetNativeDevice() const
    {
        return m_device;
    }

    id<MTLCommandQueue> GetNativeCommandQueue() const
    {
        return m_commandQueue;
    }
#endif

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
} // namespace Cacao

#endif // __APPLE__
#endif

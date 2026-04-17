#ifndef LUNA_RHI_GLDEVICE_H
#define LUNA_RHI_GLDEVICE_H
#include "Device.h"
#include "GLCommon.h"

namespace luna::RHI {
class LUNA_RHI_API GLDevice final : public Device {
public:
    static Ref<GLDevice> Create(const Ref<Adapter>& adapter, const DeviceCreateInfo& createInfo);
    GLDevice(const Ref<Adapter>& adapter, const DeviceCreateInfo& createInfo);
    ~GLDevice() override;

    Ref<Queue> GetQueue(QueueType type, uint32_t index) override;
    Ref<Swapchain> CreateSwapchain(const SwapchainCreateInfo& createInfo) override;
    std::vector<uint32_t> GetAllQueueFamilyIndices() const override;
    Ref<Adapter> GetParentAdapter() const override;
    Ref<CommandBufferEncoder> CreateCommandBufferEncoder(CommandBufferType type = CommandBufferType::Primary) override;
    void ResetCommandPool() override;
    void ReturnCommandBuffer(const Ref<CommandBufferEncoder>& encoder) override;
    void FreeCommandBuffer(const Ref<CommandBufferEncoder>& encoder) override;
    void ResetCommandBuffer(const Ref<CommandBufferEncoder>& encoder) override;
    Ref<Texture> CreateTexture(const TextureCreateInfo& createInfo) override;
    Ref<Buffer> CreateBuffer(const BufferCreateInfo& createInfo) override;
    Ref<Sampler> CreateSampler(const SamplerCreateInfo& createInfo) override;
    std::shared_ptr<DescriptorSetLayout> CreateDescriptorSetLayout(const DescriptorSetLayoutCreateInfo& info) override;
    std::shared_ptr<DescriptorPool> CreateDescriptorPool(const DescriptorPoolCreateInfo& info) override;
    Ref<ShaderModule> CreateShaderModule(const ShaderBlob& blob, const ShaderCreateInfo& info) override;
    Ref<PipelineLayout> CreatePipelineLayout(const PipelineLayoutCreateInfo& info) override;
    Ref<PipelineCache> CreatePipelineCache(std::span<const uint8_t> initialData) override;
    Ref<GraphicsPipeline> CreateGraphicsPipeline(const GraphicsPipelineCreateInfo& info) override;
    Ref<ComputePipeline> CreateComputePipeline(const ComputePipelineCreateInfo& info) override;
    Ref<Synchronization> CreateSynchronization(uint32_t maxFramesInFlight) override;

    GLint GetMaxTextureSize() const
    {
        return m_maxTextureSize;
    }

private:
    Ref<Adapter> m_parentAdapter;
    Ref<Queue> m_queue;
    GLint m_maxTextureSize = 0;
    GLint m_maxComputeWorkGroupCount[3] = {};
};
} // namespace luna::RHI

#endif

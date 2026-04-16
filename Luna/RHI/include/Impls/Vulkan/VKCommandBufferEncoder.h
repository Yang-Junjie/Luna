#ifndef CACAO_VKCOMMANDBUFFERENCODER_H
#define CACAO_VKCOMMANDBUFFERENCODER_H
#include "CommandBufferEncoder.h"

#include <vulkan/vulkan.hpp>

namespace Cacao {
class VKDevice;
class Device;

class CACAO_API VKCommandBufferEncoder : public CommandBufferEncoder {
private:
    vk::CommandBuffer m_commandBuffer;
    Ref<VKDevice> m_device;
    friend class VKDevice;
    friend class VKQueue;
    CommandBufferType m_type;
    vk::CommandBufferInheritanceRenderingInfo ConvertRenderingInfo(const RenderingInfo& info);
    vk::RenderingInfo ConvertRenderingInfoBegin(const RenderingInfo& info);
    std::vector<vk::RenderingAttachmentInfo> m_vkColorAttachments;
    vk::RenderingAttachmentInfo m_vkDepthAttachment;
    vk::RenderingAttachmentInfo m_vkStencilAttachment;
    std::vector<vk::Format> m_inheritanceColorFormats;
    std::vector<vk::DescriptorSet> m_boundDescriptorSets;
    std::vector<vk::MemoryBarrier> m_cachedMemoryBarriers;
    std::vector<vk::BufferMemoryBarrier> m_cachedBufferBarriers;
    std::vector<vk::ImageMemoryBarrier> m_cachedImageBarriers;

public:
    static Ref<VKCommandBufferEncoder>
        Create(const Ref<Device>& device, vk::CommandBuffer commandBuffer, CommandBufferType type);
    VKCommandBufferEncoder(const Ref<Device>& device, vk::CommandBuffer commandBuffer, CommandBufferType type);
    void Free() override;
    void Reset() override;
    void ReturnToPool() override;
    void Begin(const CommandBufferBeginInfo& info = CommandBufferBeginInfo()) override;
    void End() override;
    void BeginRendering(const RenderingInfo& info) override;
    void EndRendering() override;
    void BindGraphicsPipeline(const Ref<GraphicsPipeline>& pipeline) override;
    void BindComputePipeline(const Ref<ComputePipeline>& pipeline) override;
    void SetViewport(const Viewport& viewport) override;
    void SetScissor(const Rect2D& scissor) override;
    void BindVertexBuffer(uint32_t binding, const Ref<Buffer>& buffer, uint64_t offset) override;
    void BindIndexBuffer(const Ref<Buffer>& buffer, uint64_t offset, IndexType indexType) override;
    void BindDescriptorSets(const Ref<GraphicsPipeline>& pipeline,
                            uint32_t firstSet,
                            std::span<const Ref<DescriptorSet>> descriptorSets) override;
    void PushConstants(const Ref<GraphicsPipeline>& pipeline,
                       ShaderStage stageFlags,
                       uint32_t offset,
                       uint32_t size,
                       const void* data) override;
    void Draw(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance) override;
    void DrawIndexed(uint32_t indexCount,
                     uint32_t instanceCount,
                     uint32_t firstIndex,
                     int32_t vertexOffset,
                     uint32_t firstInstance) override;
    void Dispatch(uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ) override;
    void BindComputeDescriptorSets(const Ref<ComputePipeline>& pipeline,
                                   uint32_t firstSet,
                                   std::span<const Ref<DescriptorSet>> descriptorSets) override;
    void ComputePushConstants(const Ref<ComputePipeline>& pipeline,
                              ShaderStage stageFlags,
                              uint32_t offset,
                              uint32_t size,
                              const void* data) override;
    void PipelineBarrier(PipelineStage srcStage,
                         PipelineStage dstStage,
                         std::span<const CMemoryBarrier> globalBarriers,
                         std::span<const BufferBarrier> bufferBarriers,
                         std::span<const TextureBarrier> textureBarriers) override;
    void TransitionImage(const Ref<Texture>& texture,
                         ImageTransition transition,
                         const ImageSubresourceRange& range) override;
    void TransitionImageFast(VkImage image,
                             ImageTransition transition,
                             uint32_t baseMipLevel = 0,
                             uint32_t levelCount = 1,
                             uint32_t baseArrayLayer = 0,
                             uint32_t layerCount = 1,
                             VkImageAspectFlags aspectMask = VK_IMAGE_ASPECT_COLOR_BIT);
    void TransitionBuffer(const Ref<Buffer>& buffer,
                          BufferTransition transition,
                          uint64_t offset = 0,
                          uint64_t size = UINT64_MAX) override;
    void TransitionBufferFast(VkBuffer buffer,
                              BufferTransition transition,
                              uint64_t offset = 0,
                              uint64_t size = UINT64_MAX);
    void MemoryBarrierFast(MemoryTransition transition) override;
    void ResolveTexture(const Ref<Texture>& srcTexture,
                        const Ref<Texture>& dstTexture,
                        const ImageSubresourceLayers& srcSubresource = {},
                        const ImageSubresourceLayers& dstSubresource = {}) override;
    void ExecuteNative(const std::function<void(void* nativeCommandBuffer)>& func) override;

    void* GetNativeHandle() override
    {
        return &m_commandBuffer;
    }

    void CopyBufferToImage(const Ref<Buffer>& srcBuffer,
                           const Ref<Texture>& dstImage,
                           ImageLayout dstImageLayout,
                           std::span<const BufferImageCopy> regions) override;

    CommandBufferType GetCommandBufferType() const override
    {
        return m_type;
    }

    const vk::CommandBuffer& GetHandle()
    {
        return m_commandBuffer;
    }

    void CopyBuffer(const Ref<Buffer>& srcBuffer,
                    const Ref<Buffer>& dstBuffer,
                    uint64_t srcOffset,
                    uint64_t dstOffset,
                    uint64_t size) override;

    void DrawIndirect(const Ref<Buffer>& argBuffer, uint64_t offset, uint32_t drawCount, uint32_t stride) override;
    void DrawIndexedIndirect(const Ref<Buffer>& argBuffer,
                             uint64_t offset,
                             uint32_t drawCount,
                             uint32_t stride) override;
    void DispatchIndirect(const Ref<Buffer>& argBuffer, uint64_t offset) override;
    void DrawIndirectCount(const Ref<Buffer>& argBuffer,
                           uint64_t offset,
                           const Ref<Buffer>& countBuffer,
                           uint64_t countOffset,
                           uint32_t maxDrawCount,
                           uint32_t stride) override;
    void DrawIndexedIndirectCount(const Ref<Buffer>& argBuffer,
                                  uint64_t offset,
                                  const Ref<Buffer>& countBuffer,
                                  uint64_t countOffset,
                                  uint32_t maxDrawCount,
                                  uint32_t stride) override;
    void DispatchMesh(uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ) override;

    void BeginDebugLabel(const std::string& name, float r, float g, float b, float a) override;
    void EndDebugLabel() override;
    void InsertDebugLabel(const std::string& name, float r, float g, float b, float a) override;

    void BeginQuery(const Ref<QueryPool>& pool, uint32_t queryIndex) override;
    void EndQuery(const Ref<QueryPool>& pool, uint32_t queryIndex) override;
    void WriteTimestamp(const Ref<QueryPool>& pool, uint32_t queryIndex) override;
    void ResetQueryPool(const Ref<QueryPool>& pool, uint32_t first, uint32_t count) override;

    void TraceRays(const Ref<ShaderBindingTable>& sbt, uint32_t w, uint32_t h, uint32_t d) override;
    void BuildAccelerationStructure(const Ref<AccelerationStructure>& as) override;
    void BindRayTracingPipeline(const Ref<RayTracingPipeline>& pipeline) override;
    void BindRayTracingDescriptorSets(const Ref<RayTracingPipeline>& pipeline,
                                      uint32_t firstSet,
                                      std::span<const Ref<DescriptorSet>> descriptorSets) override;

    void CopyTexture2D(const Ref<Texture>& src, const Ref<Texture>& dst) override;
};
} // namespace Cacao
#endif

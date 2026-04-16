#ifndef CACAO_MTLCOMMANDBUFFERENCODER_H
#define CACAO_MTLCOMMANDBUFFERENCODER_H
#ifdef __APPLE__
#include "CommandBufferEncoder.h"
#include "MTLCommon.h"

namespace Cacao {
class CACAO_API MTLCommandBufferEncoder final : public CommandBufferEncoder {
public:
    MTLCommandBufferEncoder(const Ref<Device>& device, CommandBufferType type);
    ~MTLCommandBufferEncoder() override = default;

    void Free() override {}

    void Reset() override;

    void ReturnToPool() override {}

    void Begin(const CommandBufferBeginInfo& info) override;
    void End() override;

    void BeginRendering(const RenderingInfo& info) override;
    void EndRendering() override;
    void SetViewport(const Viewport& viewport) override;
    void SetScissor(const Rect2D& scissor) override;

    void BindGraphicsPipeline(const Ref<GraphicsPipeline>& pipeline) override;
    void BindComputePipeline(const Ref<ComputePipeline>& pipeline) override;
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
    void TransitionBuffer(const Ref<Buffer>& buffer,
                          BufferTransition transition,
                          uint64_t offset,
                          uint64_t size) override;

    void MemoryBarrierFast(MemoryTransition transition) override {}

    void ExecuteNative(const std::function<void(void* nativeCommandBuffer)>& func) override;
    void* GetNativeHandle() override;

    void CopyTexture2D(const Ref<Texture>& src, const Ref<Texture>& dst) override;
    void CopyBufferToImage(const Ref<Buffer>& srcBuffer,
                           const Ref<Texture>& dstImage,
                           ImageLayout dstImageLayout,
                           std::span<const BufferImageCopy> regions) override;
    void CopyBuffer(const Ref<Buffer>& srcBuffer,
                    const Ref<Buffer>& dstBuffer,
                    uint64_t srcOffset,
                    uint64_t dstOffset,
                    uint64_t size) override;

    CommandBufferType GetCommandBufferType() const override
    {
        return m_type;
    }

    void ResolveTexture(const Ref<Texture>& srcTexture,
                        const Ref<Texture>& dstTexture,
                        const ImageSubresourceLayers& srcSubresource,
                        const ImageSubresourceLayers& dstSubresource) override;

    void DrawIndirect(const Ref<Buffer>& argBuffer, uint64_t offset, uint32_t drawCount, uint32_t stride) override;
    void DrawIndexedIndirect(const Ref<Buffer>& argBuffer,
                             uint64_t offset,
                             uint32_t drawCount,
                             uint32_t stride) override;
    void DispatchIndirect(const Ref<Buffer>& argBuffer, uint64_t offset) override;

    void BeginQuery(const Ref<QueryPool>& pool, uint32_t queryIndex) override;
    void EndQuery(const Ref<QueryPool>& pool, uint32_t queryIndex) override;
    void WriteTimestamp(const Ref<QueryPool>& pool, uint32_t queryIndex) override;
    void ResetQueryPool(const Ref<QueryPool>& pool, uint32_t first, uint32_t count) override;

    id GetCommandBuffer() const
    {
        return m_commandBuffer;
    }

private:
    Ref<Device> m_device;
    CommandBufferType m_type;
    id m_commandBuffer = nullptr;    // id<MTLCommandBuffer>
    id m_renderEncoder = nullptr;    // id<MTLRenderCommandEncoder>
    id m_computeEncoder = nullptr;   // id<MTLComputeCommandEncoder>
    id m_blitEncoder = nullptr;      // id<MTLBlitCommandEncoder>
    id m_boundIndexBuffer = nullptr; // id<MTLBuffer> for indexed draw
    uint32_t m_indexBufferOffset = 0;
    bool m_indexIs32Bit = true;
};
} // namespace Cacao
#endif // __APPLE__
#endif

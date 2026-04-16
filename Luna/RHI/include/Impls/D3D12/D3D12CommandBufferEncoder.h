#ifndef CACAO_D3D12COMMANDBUFFERENCODER_H
#define CACAO_D3D12COMMANDBUFFERENCODER_H
#include "CommandBufferEncoder.h"
#include "D3D12Common.h"

namespace Cacao {
class CACAO_API D3D12CommandBufferEncoder final : public CommandBufferEncoder {
private:
    ComPtr<ID3D12GraphicsCommandList6> m_commandList;
    ComPtr<ID3D12CommandAllocator> m_allocator;
    Ref<Device> m_device;
    CommandBufferType m_type;
    bool m_isRecording = false;
    class D3D12GraphicsPipeline* m_currentPipeline = nullptr;

    friend class D3D12Device;
    friend class D3D12Queue;

    ID3D12GraphicsCommandList6* GetHandle() const
    {
        return m_commandList.Get();
    }

public:
    D3D12CommandBufferEncoder(const Ref<Device>& device,
                              CommandBufferType type,
                              ComPtr<ID3D12CommandAllocator> allocator);

    void Free() override {}

    void Reset() override;

    void ReturnToPool() override {}

    void Begin(const CommandBufferBeginInfo& info = CommandBufferBeginInfo()) override;
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
    void MemoryBarrierFast(MemoryTransition transition) override;
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
};
} // namespace Cacao

#endif

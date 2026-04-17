#ifndef LUNA_RHI_WGPU_COMMANDBUFFERENCODER_H
#define LUNA_RHI_WGPU_COMMANDBUFFERENCODER_H

#include "CommandBufferEncoder.h"
#include "Impls/WebGPU/WGPUCommon.h"

namespace luna::RHI {
class WGPUDeviceImpl; // forward decl to avoid name clash with webgpu WGPUDevice

class LUNA_RHI_API WGPUCommandBufferEncoder : public CommandBufferEncoder {
private:
    ::WGPUDevice m_wgpuDevice = nullptr;
    ::WGPUCommandEncoder m_encoder = nullptr;
    ::WGPURenderPassEncoder m_renderPass = nullptr;
    ::WGPUComputePassEncoder m_computePass = nullptr;
    ::WGPUCommandBuffer m_commandBuffer = nullptr;
    CommandBufferType m_type = CommandBufferType::Primary;

public:
    WGPUCommandBufferEncoder(::WGPUDevice device, CommandBufferType type);
    ~WGPUCommandBufferEncoder() override;

    void Free() override;
    void Reset() override;
    void ReturnToPool() override;
    void Begin(const CommandBufferBeginInfo& info) override;
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
    void CopyBuffer(
        const Ref<Buffer>& src, const Ref<Buffer>& dst, uint64_t srcOffset, uint64_t dstOffset, uint64_t size) override;
    void CopyBufferToImage(const Ref<Buffer>& src,
                           const Ref<Texture>& dst,
                           ResourceState dstState,
                           std::span<const BufferImageCopy> regions) override;
    void CopyTexture2D(const Ref<Texture>& src, const Ref<Texture>& dst) override;
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

    CommandBufferType GetCommandBufferType() const override
    {
        return m_type;
    }

    void ResolveTexture(const Ref<Texture>& src,
                        const Ref<Texture>& dst,
                        const ImageSubresourceLayers& srcSub,
                        const ImageSubresourceLayers& dstSub) override;
    void DrawIndirect(const Ref<Buffer>& argBuffer, uint64_t offset, uint32_t drawCount, uint32_t stride) override;
    void DrawIndexedIndirect(const Ref<Buffer>& argBuffer,
                             uint64_t offset,
                             uint32_t drawCount,
                             uint32_t stride) override;
    void DispatchIndirect(const Ref<Buffer>& argBuffer, uint64_t offset) override;

    void BuildAccelerationStructure(const Ref<AccelerationStructure>& as) override;
    void BindRayTracingPipeline(const Ref<RayTracingPipeline>& pipeline) override;
    void BindRayTracingDescriptorSets(const Ref<RayTracingPipeline>& pipeline,
                                      uint32_t firstSet,
                                      std::span<const Ref<DescriptorSet>> descriptorSets) override;
    void TraceRays(const Ref<ShaderBindingTable>& sbt, uint32_t width, uint32_t height, uint32_t depth) override;

    ::WGPUCommandBuffer GetFinishedCommandBuffer() const
    {
        return m_commandBuffer;
    }
};
} // namespace luna::RHI

#endif

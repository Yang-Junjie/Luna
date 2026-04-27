#ifndef LUNA_RHI_COMMANDBUFFERENCODER_H
#define LUNA_RHI_COMMANDBUFFERENCODER_H
#include "Barrier.h"

#include <functional>

namespace luna::RHI {
class Buffer;
class Texture;
class GraphicsPipeline;
class ComputePipeline;
class DescriptorSet;
class QueryPool;
class AccelerationStructure;
class RayTracingPipeline;
struct CMemoryBarrier;

enum class ImageTransition : uint8_t {
    UndefinedToColorAttachment,
    UndefinedToDepthAttachment,
    UndefinedToTransferDst,
    UndefinedToShaderRead,
    UndefinedToGeneral,
    ColorAttachmentToPresent,
    ColorAttachmentToShaderRead,
    ColorAttachmentToTransferSrc,
    DepthAttachmentToShaderRead,
    TransferDstToShaderRead,
    TransferDstToColorAttachment,
    TransferSrcToShaderRead,
    ShaderReadToColorAttachment,
    ShaderReadToTransferSrc,
    ShaderReadToTransferDst,
    ShaderReadToGeneral,
    PresentToColorAttachment,
    GeneralToShaderRead,
    GeneralToColorAttachment,
    GeneralToTransferDst,
    Count
};

enum class BufferTransition : uint8_t {
    HostWriteToVertexRead,
    HostWriteToIndexRead,
    HostWriteToUniformRead,
    HostWriteToShaderRead,
    HostWriteToIndirectRead,
    HostWriteToTransferSrc,
    TransferDstToVertexRead,
    TransferDstToIndexRead,
    TransferDstToUniformRead,
    TransferDstToShaderRead,
    ShaderWriteToShaderRead,
    ShaderWriteToHostRead,
    ShaderWriteToTransferSrc,
    ComputeWriteToVertexRead,
    ComputeWriteToIndirectRead,
    Count
};

enum class MemoryTransition : uint8_t {
    ComputeWriteToComputeRead,
    ComputeWriteToGraphicsRead,
    GraphicsWriteToComputeRead,
    TransferWriteToShaderRead,
    ShaderWriteToTransferRead,
    AllWriteToAllRead,
    Count
};

struct Viewport {
    float X;
    float Y;
    float Width;
    float Height;
    float MinDepth = 0.0f;
    float MaxDepth = 1.0f;
};

struct Rect2D {
    int32_t OffsetX;
    int32_t OffsetY;
    uint32_t Width;
    uint32_t Height;
};

struct ClearValue {
    union {
        float Color[4];

        struct {
            float Depth;
            uint32_t Stencil;
        } DepthStencil;
    };

    static ClearValue ColorFloat(float r, float g, float b, float a)
    {
        ClearValue v;
        v.Color[0] = r;
        v.Color[1] = g;
        v.Color[2] = b;
        v.Color[3] = a;
        return v;
    }

    static ClearValue DepthStencilValue(float d, uint32_t s)
    {
        ClearValue v;
        v.DepthStencil.Depth = d;
        v.DepthStencil.Stencil = s;
        return v;
    }
};

struct ClearDepthStencilValue {
    float Depth;
    uint32_t Stencil;
};

enum class IndexType {
    UInt16,
    UInt32
};

struct ImageSubresourceLayers {
    ImageAspectFlags AspectMask = ImageAspectFlags::Color;
    uint32_t MipLevel = 0;
    uint32_t BaseArrayLayer = 0;
    uint32_t LayerCount = 1;
};

struct BufferImageCopy {
    uint64_t BufferOffset = 0;
    uint32_t BufferRowLength = 0;
    uint32_t BufferImageHeight = 0;
    ImageSubresourceLayers ImageSubresource;
    int32_t ImageOffsetX = 0;
    int32_t ImageOffsetY = 0;
    int32_t ImageOffsetZ = 0;
    uint32_t ImageExtentWidth = 0;
    uint32_t ImageExtentHeight = 0;
    uint32_t ImageExtentDepth = 1;
};

enum class AttachmentLoadOp {
    Load,
    Clear,
    DontCare
};

enum class AttachmentStoreOp {
    Store,
    DontCare
};

struct RenderingAttachmentInfo {
    Ref<Texture> Texture;
    AttachmentLoadOp LoadOp = AttachmentLoadOp::Clear;
    AttachmentStoreOp StoreOp = AttachmentStoreOp::Store;
    ClearValue ClearValue = {0.0f, 0.0f, 0.0f, 1.0f};
    ClearDepthStencilValue ClearDepthStencil = {1.0f, 0};
};

struct RenderingInfo {
    Rect2D RenderArea;
    std::vector<RenderingAttachmentInfo> ColorAttachments;
    Ref<RenderingAttachmentInfo> DepthAttachment = nullptr;
    Ref<RenderingAttachmentInfo> StencilAttachment = nullptr;
    uint32_t LayerCount = 1;
};

struct CommandBufferInheritanceInfo {
    const RenderingInfo* pRenderingInfo = nullptr;
    bool OcclusionQueryEnable = false;
    bool PipelineStatistics = false;
};

struct CommandBufferBeginInfo {
    bool OneTimeSubmit = true;
    bool SimultaneousUse = false;
    const CommandBufferInheritanceInfo* InheritanceInfo = nullptr;
};

enum class CommandBufferType;

class LUNA_RHI_API CommandBufferEncoder : public std::enable_shared_from_this<CommandBufferEncoder> {
public:
    virtual void Free() = 0;
    virtual void Reset() = 0;
    virtual void ReturnToPool() = 0;
    virtual void Begin(const CommandBufferBeginInfo& info = CommandBufferBeginInfo()) = 0;
    virtual void End() = 0;
    virtual void BeginRendering(const RenderingInfo& info) = 0;
    virtual void EndRendering() = 0;
    virtual void BindGraphicsPipeline(const Ref<GraphicsPipeline>& pipeline) = 0;
    virtual void BindComputePipeline(const Ref<ComputePipeline>& pipeline) = 0;
    virtual void SetViewport(const Viewport& viewport) = 0;
    virtual void SetScissor(const Rect2D& scissor) = 0;
    virtual void BindVertexBuffer(uint32_t binding, const Ref<Buffer>& buffer, uint64_t offset = 0) = 0;
    virtual void BindIndexBuffer(const Ref<Buffer>& buffer, uint64_t offset, IndexType indexType) = 0;
    virtual void BindDescriptorSets(const Ref<GraphicsPipeline>& pipeline,
                                    uint32_t firstSet,
                                    std::span<const Ref<DescriptorSet>> descriptorSets) = 0;
    virtual void PushConstants(const Ref<GraphicsPipeline>& pipeline,
                               ShaderStage stageFlags,
                               uint32_t offset,
                               uint32_t size,
                               const void* data) = 0;
    virtual void Draw(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance) = 0;
    virtual void DrawIndexed(uint32_t indexCount,
                             uint32_t instanceCount,
                             uint32_t firstIndex,
                             int32_t vertexOffset,
                             uint32_t firstInstance) = 0;
    virtual void Dispatch(uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ) = 0;
    virtual void BindComputeDescriptorSets(const Ref<ComputePipeline>& pipeline,
                                           uint32_t firstSet,
                                           std::span<const Ref<DescriptorSet>> descriptorSets) = 0;
    virtual void ComputePushConstants(const Ref<ComputePipeline>& pipeline,
                                      ShaderStage stageFlags,
                                      uint32_t offset,
                                      uint32_t size,
                                      const void* data) = 0;
    virtual void PipelineBarrier(PipelineStage srcStage,
                                 PipelineStage dstStage,
                                 std::span<const CMemoryBarrier> globalBarriers,
                                 std::span<const BufferBarrier> bufferBarriers,
                                 std::span<const TextureBarrier> textureBarriers) = 0;
    virtual void PipelineBarrier(PipelineStage srcStage,
                                 PipelineStage dstStage,
                                 std::span<const TextureBarrier> textureBarriers);
    virtual void
        PipelineBarrier(PipelineStage srcStage, PipelineStage dstStage, std::span<const BufferBarrier> bufferBarriers);
    virtual void TransitionImage(const Ref<Texture>& texture,
                                 ImageTransition transition,
                                 const ImageSubresourceRange& range = {0, 1, 0, 1, ImageAspectFlags::Color}) = 0;
    virtual void TransitionBuffer(const Ref<Buffer>& buffer,
                                  BufferTransition transition,
                                  uint64_t offset = 0,
                                  uint64_t size = UINT64_MAX) = 0;
    virtual void MemoryBarrierFast(MemoryTransition transition) = 0;
    virtual void ExecuteNative(const std::function<void(void* nativeCommandBuffer)>& func) = 0;
    virtual void* GetNativeHandle() = 0;
    virtual void CopyTexture2D(const Ref<Texture>& src, const Ref<Texture>& dst) = 0;
    virtual void CopyBufferToImage(const Ref<Buffer>& srcBuffer,
                                   const Ref<Texture>& dstImage,
                                   ImageLayout dstImageLayout,
                                   std::span<const BufferImageCopy> regions) = 0;
    virtual void CopyImageToBuffer(const Ref<Texture>& srcImage,
                                   ImageLayout srcImageLayout,
                                   const Ref<Buffer>& dstBuffer,
                                   std::span<const BufferImageCopy> regions)
    {}
    virtual ~CommandBufferEncoder() = default;
    virtual CommandBufferType GetCommandBufferType() const = 0;
    virtual void CopyBuffer(const Ref<Buffer>& srcBuffer,
                            const Ref<Buffer>& dstBuffer,
                            uint64_t srcOffset,
                            uint64_t dstOffset,
                            uint64_t size) = 0;

    virtual void ResolveTexture(const Ref<Texture>& srcTexture,
                                const Ref<Texture>& dstTexture,
                                const ImageSubresourceLayers& srcSubresource = {},
                                const ImageSubresourceLayers& dstSubresource = {}) = 0;

    virtual void DrawIndirect(const Ref<Buffer>& argBuffer, uint64_t offset, uint32_t drawCount, uint32_t stride) = 0;
    virtual void
        DrawIndexedIndirect(const Ref<Buffer>& argBuffer, uint64_t offset, uint32_t drawCount, uint32_t stride) = 0;
    virtual void DispatchIndirect(const Ref<Buffer>& argBuffer, uint64_t offset) = 0;

    virtual void DrawIndirectCount(const Ref<Buffer>& argBuffer,
                                   uint64_t offset,
                                   const Ref<Buffer>& countBuffer,
                                   uint64_t countOffset,
                                   uint32_t maxDrawCount,
                                   uint32_t stride)
    {}

    virtual void DrawIndexedIndirectCount(const Ref<Buffer>& argBuffer,
                                          uint64_t offset,
                                          const Ref<Buffer>& countBuffer,
                                          uint64_t countOffset,
                                          uint32_t maxDrawCount,
                                          uint32_t stride)
    {}

    virtual void DispatchMesh(uint32_t groupCountX, uint32_t groupCountY = 1, uint32_t groupCountZ = 1) {}

    virtual void
        BeginDebugLabel(const std::string& name, float r = 1.0f, float g = 1.0f, float b = 1.0f, float a = 1.0f)
    {}

    virtual void EndDebugLabel() {}

    virtual void
        InsertDebugLabel(const std::string& name, float r = 1.0f, float g = 1.0f, float b = 1.0f, float a = 1.0f)
    {}

    virtual void BeginQuery(const Ref<QueryPool>& pool, uint32_t queryIndex) = 0;
    virtual void EndQuery(const Ref<QueryPool>& pool, uint32_t queryIndex) = 0;
    virtual void WriteTimestamp(const Ref<QueryPool>& pool, uint32_t queryIndex) = 0;
    virtual void ResetQueryPool(const Ref<QueryPool>& pool, uint32_t first, uint32_t count) = 0;
    virtual void ResolveQueryPool(const Ref<QueryPool>& pool, uint32_t first, uint32_t count) {}

    virtual void
        TraceRays(const Ref<class ShaderBindingTable>& sbt, uint32_t width, uint32_t height, uint32_t depth = 1)
    {}

    virtual void BuildAccelerationStructure(const Ref<AccelerationStructure>& accelStruct) {}

    virtual void BindRayTracingPipeline(const Ref<RayTracingPipeline>& pipeline) {}

    virtual void BindRayTracingDescriptorSets(const Ref<RayTracingPipeline>& pipeline,
                                              uint32_t firstSet,
                                              std::span<const Ref<DescriptorSet>> descriptorSets)
    {}
};
} // namespace luna::RHI
#endif

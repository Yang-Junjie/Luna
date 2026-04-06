#pragma once

#include "CommandBarriers.h"
#include "CommandRendering.h"

#include <span>
#include <string_view>

namespace luna {

class IRHICommandContext {
public:
    virtual ~IRHICommandContext() = default;

    virtual RHIResult beginRendering(const RenderingInfo& renderingInfo) = 0;
    virtual RHIResult endRendering() = 0;
    virtual RHIResult clearColor(const ClearColorValue& color) = 0;
    virtual RHIResult imageBarrier(const ImageBarrierInfo& barrierInfo) = 0;
    virtual RHIResult bufferBarrier(const BufferBarrierInfo& barrierInfo) = 0;
    virtual RHIResult transitionImage(ImageHandle image, ImageLayout newLayout) = 0;
    virtual RHIResult copyBuffer(const BufferCopyInfo& copyInfo) = 0;
    virtual RHIResult copyImage(const ImageCopyInfo& copyInfo) = 0;
    virtual RHIResult copyBufferToImage(const BufferImageCopyInfo& copyInfo) = 0;
    virtual RHIResult copyImageToBuffer(const BufferImageCopyInfo& copyInfo) = 0;
    virtual RHIResult bindGraphicsPipeline(PipelineHandle pipeline) = 0;
    virtual RHIResult bindComputePipeline(PipelineHandle pipeline) = 0;
    virtual RHIResult bindVertexBuffer(BufferHandle buffer, uint64_t offset = 0) = 0;
    virtual RHIResult bindIndexBuffer(BufferHandle buffer, IndexFormat indexFormat, uint64_t offset = 0) = 0;
    virtual RHIResult setViewport(const Viewport& viewport) = 0;
    virtual RHIResult setScissor(const ScissorRect& scissor) = 0;
    virtual RHIResult bindResourceSet(ResourceSetHandle resourceSet, std::span<const uint32_t> dynamicOffsets = {}) = 0;
    virtual RHIResult pushConstants(const void* data,
                                    uint32_t size,
                                    uint32_t offset = 0,
                                    ShaderType visibility = ShaderType::AllGraphics) = 0;
    virtual RHIResult draw(const DrawArguments& arguments) = 0;
    virtual RHIResult drawIndexed(const IndexedDrawArguments& arguments) = 0;
    virtual RHIResult drawIndirect(BufferHandle argumentsBuffer, uint64_t offset = 0) = 0;
    virtual RHIResult dispatch(uint32_t groupCountX, uint32_t groupCountY = 1, uint32_t groupCountZ = 1) = 0;
    virtual RHIResult dispatchIndirect(BufferHandle argumentsBuffer, uint64_t offset = 0) = 0;
    virtual RHIResult beginDebugLabel(std::string_view label,
                                      const ClearColorValue& color = {0.20f, 0.45f, 0.85f, 1.0f}) = 0;
    virtual RHIResult endDebugLabel() = 0;
};

} // namespace luna

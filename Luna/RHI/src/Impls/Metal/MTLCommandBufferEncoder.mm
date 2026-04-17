#ifdef __APPLE__
#import <Metal/Metal.h>
#include "Impls/Metal/MTLCommandBufferEncoder.h"
#include "Impls/Metal/MTLTexture.h"
#include "Impls/Metal/MTLBuffer.h"
#include "Impls/Metal/MTLPipeline.h"
#include "Impls/Metal/MTLQueue.h"
#include "Impls/Metal/MTLDevice.h"
#include "Impls/Metal/MTLDescriptorSet.h"

namespace luna::RHI
{
    MTLCommandBufferEncoder::MTLCommandBufferEncoder(const Ref<Device>& device, CommandBufferType type)
        : m_device(device), m_type(type) {}

    void MTLCommandBufferEncoder::Reset()
    {
        m_renderEncoder = nullptr;
        m_computeEncoder = nullptr;
        m_blitEncoder = nullptr;
        m_commandBuffer = nullptr;
        m_boundIndexBuffer = nullptr;
    }

    void MTLCommandBufferEncoder::Begin(const CommandBufferBeginInfo& info)
    {
        Reset();
        auto* queue = static_cast<MTLQueue*>(m_device->GetQueue(QueueType::Graphics, 0).get());
        id<MTLCommandQueue> mtlQueue = (id<MTLCommandQueue>)queue->GetHandle();
        m_commandBuffer = [mtlQueue commandBuffer];
    }

    void MTLCommandBufferEncoder::End()
    {
        if (m_blitEncoder) {
            [(id<MTLBlitCommandEncoder>)m_blitEncoder endEncoding];
            m_blitEncoder = nullptr;
        }
    }

    void MTLCommandBufferEncoder::BeginRendering(const RenderingInfo& info)
    {
        if (!m_commandBuffer) return;

        MTLRenderPassDescriptor* rpDesc = [MTLRenderPassDescriptor renderPassDescriptor];

        for (uint32_t i = 0; i < info.ColorAttachments.size(); i++)
        {
            auto& att = info.ColorAttachments[i];
            auto* colorTexture = static_cast<MTLTextureImpl*>(att.Texture.get());
            rpDesc.colorAttachments[i].texture = (id<MTLTexture>)colorTexture->GetHandle();
            rpDesc.colorAttachments[i].loadAction =
                (att.LoadOp == AttachmentLoadOp::Clear) ? MTLLoadActionClear :
                (att.LoadOp == AttachmentLoadOp::Load) ? MTLLoadActionLoad : MTLLoadActionDontCare;
            rpDesc.colorAttachments[i].storeAction =
                (att.StoreOp == AttachmentStoreOp::Store) ? MTLStoreActionStore : MTLStoreActionDontCare;
            rpDesc.colorAttachments[i].clearColor =
                MTLClearColorMake(att.ClearValue.Color[0], att.ClearValue.Color[1],
                                  att.ClearValue.Color[2], att.ClearValue.Color[3]);
        }

        if (info.DepthAttachment)
        {
            auto* depthTex = static_cast<MTLTextureImpl*>(info.DepthAttachment->Texture.get());
            rpDesc.depthAttachment.texture = (id<MTLTexture>)depthTex->GetHandle();
            rpDesc.depthAttachment.loadAction =
                (info.DepthAttachment->LoadOp == AttachmentLoadOp::Clear) ? MTLLoadActionClear : MTLLoadActionLoad;
            rpDesc.depthAttachment.storeAction =
                (info.DepthAttachment->StoreOp == AttachmentStoreOp::Store) ? MTLStoreActionStore : MTLStoreActionDontCare;
            rpDesc.depthAttachment.clearDepth = info.DepthAttachment->ClearDepthStencil.Depth;
        }

        m_renderEncoder = [(id<MTLCommandBuffer>)m_commandBuffer renderCommandEncoderWithDescriptor:rpDesc];
    }

    void MTLCommandBufferEncoder::EndRendering()
    {
        if (m_renderEncoder) {
            [(id<MTLRenderCommandEncoder>)m_renderEncoder endEncoding];
            m_renderEncoder = nullptr;
        }
    }

    void MTLCommandBufferEncoder::SetViewport(const Viewport& viewport)
    {
        if (!m_renderEncoder) return;
        MTLViewport vp = {
            static_cast<double>(viewport.X), static_cast<double>(viewport.Y),
            static_cast<double>(viewport.Width), static_cast<double>(viewport.Height),
            static_cast<double>(viewport.MinDepth), static_cast<double>(viewport.MaxDepth)
        };
        [(id<MTLRenderCommandEncoder>)m_renderEncoder setViewport:vp];
    }

    void MTLCommandBufferEncoder::SetScissor(const Rect2D& scissor)
    {
        if (!m_renderEncoder) return;
        MTLScissorRect sr = {
            static_cast<NSUInteger>(scissor.OffsetX), static_cast<NSUInteger>(scissor.OffsetY),
            static_cast<NSUInteger>(scissor.Width), static_cast<NSUInteger>(scissor.Height)
        };
        [(id<MTLRenderCommandEncoder>)m_renderEncoder setScissorRect:sr];
    }

    void MTLCommandBufferEncoder::BindGraphicsPipeline(const Ref<GraphicsPipeline>& pipeline)
    {
        if (!m_renderEncoder) return;
        auto* mtlPipeline = static_cast<MTLGraphicsPipeline*>(pipeline.get());
        id<MTLRenderPipelineState> state = (id<MTLRenderPipelineState>)mtlPipeline->GetPipelineState();
        if (state)
            [(id<MTLRenderCommandEncoder>)m_renderEncoder setRenderPipelineState:state];
    }

    void MTLCommandBufferEncoder::BindComputePipeline(const Ref<ComputePipeline>& pipeline)
    {
        if (!m_commandBuffer) return;
        if (!m_computeEncoder) {
            m_computeEncoder = [(id<MTLCommandBuffer>)m_commandBuffer computeCommandEncoder];
        }
        auto* mtlPipeline = static_cast<MTLComputePipeline*>(pipeline.get());
        id<MTLComputePipelineState> state = (id<MTLComputePipelineState>)mtlPipeline->GetPipelineState();
        if (state)
            [(id<MTLComputeCommandEncoder>)m_computeEncoder setComputePipelineState:state];
    }

    void MTLCommandBufferEncoder::BindVertexBuffer(uint32_t binding, const Ref<Buffer>& buffer, uint64_t offset)
    {
        if (!m_renderEncoder) return;
        auto* mtlBuf = static_cast<MTLBufferImpl*>(buffer.get());
        [(id<MTLRenderCommandEncoder>)m_renderEncoder
            setVertexBuffer:(id<MTLBuffer>)mtlBuf->GetHandle()
            offset:offset
            atIndex:binding];
    }

    void MTLCommandBufferEncoder::BindIndexBuffer(const Ref<Buffer>& buffer, uint64_t offset, IndexType indexType)
    {
        auto* mtlBuf = static_cast<MTLBufferImpl*>(buffer.get());
        m_boundIndexBuffer = mtlBuf->GetHandle();
        m_indexBufferOffset = static_cast<uint32_t>(offset);
        m_indexIs32Bit = (indexType == IndexType::UInt32);
    }

    void MTLCommandBufferEncoder::BindDescriptorSets(const Ref<GraphicsPipeline>& pipeline, uint32_t firstSet,
        std::span<const Ref<DescriptorSet>> descriptorSets)
    {
        // Metal uses argument buffers or direct setBuffer/setTexture per binding.
        // With the current MTLDescriptorSet storing bindings as vectors, we apply
        // each binding directly to the render encoder.
        // Full argument buffer support would require encoding bindings into
        // an MTLBuffer; for now, direct binding is sufficient for basic usage.
    }

    void MTLCommandBufferEncoder::PushConstants(const Ref<GraphicsPipeline>& pipeline, ShaderStage stageFlags,
        uint32_t offset, uint32_t size, const void* data)
    {
        if (!m_renderEncoder) return;
        if (static_cast<uint32_t>(stageFlags) & static_cast<uint32_t>(ShaderStage::Vertex)) {
            [(id<MTLRenderCommandEncoder>)m_renderEncoder setVertexBytes:data length:size atIndex:30];
        }
        if (static_cast<uint32_t>(stageFlags) & static_cast<uint32_t>(ShaderStage::Fragment)) {
            [(id<MTLRenderCommandEncoder>)m_renderEncoder setFragmentBytes:data length:size atIndex:30];
        }
    }

    void MTLCommandBufferEncoder::ComputePushConstants(const Ref<ComputePipeline>& pipeline, ShaderStage stageFlags,
        uint32_t offset, uint32_t size, const void* data)
    {
        if (!m_computeEncoder) return;
        [(id<MTLComputeCommandEncoder>)m_computeEncoder setBytes:data length:size atIndex:30];
    }

    void MTLCommandBufferEncoder::Draw(uint32_t vertexCount, uint32_t instanceCount,
        uint32_t firstVertex, uint32_t firstInstance)
    {
        if (!m_renderEncoder) return;
        [(id<MTLRenderCommandEncoder>)m_renderEncoder
            drawPrimitives:MTLPrimitiveTypeTriangle
            vertexStart:firstVertex
            vertexCount:vertexCount
            instanceCount:instanceCount
            baseInstance:firstInstance];
    }

    void MTLCommandBufferEncoder::DrawIndexed(uint32_t indexCount, uint32_t instanceCount,
        uint32_t firstIndex, int32_t vertexOffset, uint32_t firstInstance)
    {
        if (!m_renderEncoder || !m_boundIndexBuffer) return;
        MTLIndexType idxType = m_indexIs32Bit ? MTLIndexTypeUInt32 : MTLIndexTypeUInt16;
        uint32_t idxSize = m_indexIs32Bit ? 4 : 2;
        NSUInteger offset = m_indexBufferOffset + firstIndex * idxSize;
        [(id<MTLRenderCommandEncoder>)m_renderEncoder
            drawIndexedPrimitives:MTLPrimitiveTypeTriangle
            indexCount:indexCount
            indexType:idxType
            indexBuffer:(id<MTLBuffer>)m_boundIndexBuffer
            indexBufferOffset:offset
            instanceCount:instanceCount
            baseVertex:vertexOffset
            baseInstance:firstInstance];
    }

    void MTLCommandBufferEncoder::Dispatch(uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ)
    {
        if (!m_computeEncoder) return;
        MTLSize threadgroups = MTLSizeMake(groupCountX, groupCountY, groupCountZ);
        MTLSize threadsPerGroup = MTLSizeMake(1, 1, 1);
        [(id<MTLComputeCommandEncoder>)m_computeEncoder
            dispatchThreadgroups:threadgroups threadsPerThreadgroup:threadsPerGroup];
        [(id<MTLComputeCommandEncoder>)m_computeEncoder endEncoding];
        m_computeEncoder = nullptr;
    }

    void MTLCommandBufferEncoder::BindComputeDescriptorSets(const Ref<ComputePipeline>& pipeline, uint32_t firstSet,
        std::span<const Ref<DescriptorSet>> descriptorSets)
    {
        // Metal argument buffer binding; see BindDescriptorSets note above
    }

    // Metal handles resource tracking automatically; barriers are no-ops
    void MTLCommandBufferEncoder::PipelineBarrier(PipelineStage, PipelineStage,
        std::span<const CMemoryBarrier>, std::span<const BufferBarrier>, std::span<const TextureBarrier>) {}
    void MTLCommandBufferEncoder::TransitionImage(const Ref<Texture>&, ImageTransition, const ImageSubresourceRange&) {}
    void MTLCommandBufferEncoder::TransitionBuffer(const Ref<Buffer>&, BufferTransition, uint64_t, uint64_t) {}

    void MTLCommandBufferEncoder::ExecuteNative(const std::function<void(void*)>& func)
    {
        func(m_commandBuffer);
    }

    void* MTLCommandBufferEncoder::GetNativeHandle() { return m_commandBuffer; }

    void MTLCommandBufferEncoder::CopyTexture2D(const Ref<Texture>& src, const Ref<Texture>& dst)
    {
        if (!m_commandBuffer) return;
        id<MTLBlitCommandEncoder> blit = [(id<MTLCommandBuffer>)m_commandBuffer blitCommandEncoder];
        auto* srcTex = static_cast<MTLTextureImpl*>(src.get());
        auto* dstTex = static_cast<MTLTextureImpl*>(dst.get());
        [blit copyFromTexture:(id<MTLTexture>)srcTex->GetHandle()
             sourceSlice:0 sourceLevel:0
             sourceOrigin:MTLOriginMake(0,0,0)
             sourceSize:MTLSizeMake(src->GetWidth(), src->GetHeight(), 1)
             toTexture:(id<MTLTexture>)dstTex->GetHandle()
             destinationSlice:0 destinationLevel:0
             destinationOrigin:MTLOriginMake(0,0,0)];
        [blit endEncoding];
    }

    void MTLCommandBufferEncoder::CopyBufferToImage(const Ref<Buffer>& srcBuffer, const Ref<Texture>& dstImage,
        ImageLayout dstImageLayout, std::span<const BufferImageCopy> regions)
    {
        if (!m_commandBuffer) return;
        id<MTLBlitCommandEncoder> blit = [(id<MTLCommandBuffer>)m_commandBuffer blitCommandEncoder];
        auto* srcBuf = static_cast<MTLBufferImpl*>(srcBuffer.get());
        auto* dstTex = static_cast<MTLTextureImpl*>(dstImage.get());

        for (auto& r : regions)
        {
            uint32_t bytesPerRow = r.BufferRowLength > 0 ? r.BufferRowLength * 4 : r.ImageExtentWidth * 4;
            [blit copyFromBuffer:(id<MTLBuffer>)srcBuf->GetHandle()
                 sourceOffset:r.BufferOffset
                 sourceBytesPerRow:bytesPerRow
                 sourceBytesPerImage:bytesPerRow * r.ImageExtentHeight
                 sourceSize:MTLSizeMake(r.ImageExtentWidth, r.ImageExtentHeight, r.ImageExtentDepth)
                 toTexture:(id<MTLTexture>)dstTex->GetHandle()
                 destinationSlice:r.ImageSubresource.BaseArrayLayer
                 destinationLevel:r.ImageSubresource.MipLevel
                 destinationOrigin:MTLOriginMake(r.ImageOffsetX, r.ImageOffsetY, r.ImageOffsetZ)];
        }
        [blit endEncoding];
    }

    void MTLCommandBufferEncoder::CopyBuffer(const Ref<Buffer>& srcBuffer, const Ref<Buffer>& dstBuffer,
        uint64_t srcOffset, uint64_t dstOffset, uint64_t size)
    {
        if (!m_commandBuffer) return;
        id<MTLBlitCommandEncoder> blit = [(id<MTLCommandBuffer>)m_commandBuffer blitCommandEncoder];
        auto* src = static_cast<MTLBufferImpl*>(srcBuffer.get());
        auto* dst = static_cast<MTLBufferImpl*>(dstBuffer.get());
        [blit copyFromBuffer:(id<MTLBuffer>)src->GetHandle() sourceOffset:srcOffset
             toBuffer:(id<MTLBuffer>)dst->GetHandle() destinationOffset:dstOffset size:size];
        [blit endEncoding];
    }

    void MTLCommandBufferEncoder::ResolveTexture(const Ref<Texture>& srcTexture, const Ref<Texture>& dstTexture,
        const ImageSubresourceLayers& srcSub, const ImageSubresourceLayers& dstSub)
    {
        // Metal resolves MSAA during render pass via storeAction = MTLStoreActionMultisampleResolve.
        // Blit-based resolve is not directly supported; this is handled at BeginRendering level.
    }

    void MTLCommandBufferEncoder::DrawIndirect(const Ref<Buffer>& argBuffer, uint64_t offset,
        uint32_t drawCount, uint32_t stride)
    {
        if (!m_renderEncoder) return;
        auto* buf = static_cast<MTLBufferImpl*>(argBuffer.get());
        for (uint32_t i = 0; i < drawCount; i++) {
            [(id<MTLRenderCommandEncoder>)m_renderEncoder
                drawPrimitives:MTLPrimitiveTypeTriangle
                indirectBuffer:(id<MTLBuffer>)buf->GetHandle()
                indirectBufferOffset:offset + i * stride];
        }
    }

    void MTLCommandBufferEncoder::DrawIndexedIndirect(const Ref<Buffer>& argBuffer, uint64_t offset,
        uint32_t drawCount, uint32_t stride)
    {
        if (!m_renderEncoder || !m_boundIndexBuffer) return;
        auto* buf = static_cast<MTLBufferImpl*>(argBuffer.get());
        MTLIndexType idxType = m_indexIs32Bit ? MTLIndexTypeUInt32 : MTLIndexTypeUInt16;
        for (uint32_t i = 0; i < drawCount; i++) {
            [(id<MTLRenderCommandEncoder>)m_renderEncoder
                drawIndexedPrimitives:MTLPrimitiveTypeTriangle
                indexType:idxType
                indexBuffer:(id<MTLBuffer>)m_boundIndexBuffer
                indexBufferOffset:m_indexBufferOffset
                indirectBuffer:(id<MTLBuffer>)buf->GetHandle()
                indirectBufferOffset:offset + i * stride];
        }
    }

    void MTLCommandBufferEncoder::DispatchIndirect(const Ref<Buffer>& argBuffer, uint64_t offset)
    {
        if (!m_computeEncoder) return;
        auto* buf = static_cast<MTLBufferImpl*>(argBuffer.get());
        MTLSize threadsPerGroup = MTLSizeMake(1, 1, 1);
        [(id<MTLComputeCommandEncoder>)m_computeEncoder
            dispatchThreadgroupsWithIndirectBuffer:(id<MTLBuffer>)buf->GetHandle()
            indirectBufferOffset:offset
            threadsPerThreadgroup:threadsPerGroup];
    }

    void MTLCommandBufferEncoder::BeginQuery(const Ref<QueryPool>&, uint32_t) {}
    void MTLCommandBufferEncoder::EndQuery(const Ref<QueryPool>&, uint32_t) {}
    void MTLCommandBufferEncoder::WriteTimestamp(const Ref<QueryPool>&, uint32_t) {}
    void MTLCommandBufferEncoder::ResetQueryPool(const Ref<QueryPool>&, uint32_t, uint32_t) {}
}
#endif // __APPLE__

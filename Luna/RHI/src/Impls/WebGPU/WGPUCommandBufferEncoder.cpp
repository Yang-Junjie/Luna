#include "Impls/WebGPU/WGPUCommandBufferEncoder.h"
#include "Impls/WebGPU/WGPUBuffer.h"
#include "Impls/WebGPU/WGPUTexture.h"
#include "Impls/WebGPU/WGPUPipeline.h"
#include "Impls/WebGPU/WGPUDescriptorSet.h"

namespace Cacao
{
    WGPUCommandBufferEncoder::WGPUCommandBufferEncoder(::WGPUDevice device, CommandBufferType type)
        : m_wgpuDevice(device), m_type(type) {}

    WGPUCommandBufferEncoder::~WGPUCommandBufferEncoder()
    {
        if (m_commandBuffer) { wgpuCommandBufferRelease(m_commandBuffer); m_commandBuffer = nullptr; }
        if (m_encoder) { wgpuCommandEncoderRelease(m_encoder); m_encoder = nullptr; }
    }

    void WGPUCommandBufferEncoder::Free()
    {
        if (m_commandBuffer) { wgpuCommandBufferRelease(m_commandBuffer); m_commandBuffer = nullptr; }
        if (m_encoder) { wgpuCommandEncoderRelease(m_encoder); m_encoder = nullptr; }
    }

    void WGPUCommandBufferEncoder::Reset()
    {
        if (m_commandBuffer) { wgpuCommandBufferRelease(m_commandBuffer); m_commandBuffer = nullptr; }
        if (m_encoder) { wgpuCommandEncoderRelease(m_encoder); m_encoder = nullptr; }
        m_renderPass = nullptr;
        m_computePass = nullptr;
    }

    void WGPUCommandBufferEncoder::ReturnToPool() {}

    void WGPUCommandBufferEncoder::Begin(const CommandBufferBeginInfo& info)
    {
        if (m_commandBuffer) { wgpuCommandBufferRelease(m_commandBuffer); m_commandBuffer = nullptr; }
        if (m_encoder) { wgpuCommandEncoderRelease(m_encoder); m_encoder = nullptr; }

        WGPUCommandEncoderDescriptor desc = {};
        desc.label = "CacaoCommandEncoder";
        m_encoder = wgpuDeviceCreateCommandEncoder(m_wgpuDevice, &desc);
    }

    void WGPUCommandBufferEncoder::End()
    {
        WGPUCommandBufferDescriptor desc = {};
        desc.label = "CacaoCommandBuffer";
        m_commandBuffer = wgpuCommandEncoderFinish(m_encoder, &desc);
    }

    void WGPUCommandBufferEncoder::BeginRendering(const RenderingInfo& info)
    {
        if (!m_encoder) return;

        std::vector<WGPURenderPassColorAttachment> colorAttachments;
        for (auto& att : info.ColorAttachments)
        {
            WGPURenderPassColorAttachment ca = {};
            auto* tex = static_cast<WGPUTextureImpl*>(att.Texture.get());
            auto defaultView = tex->GetDefaultView();
            if (defaultView) {
                ca.view = static_cast<WGPUTextureViewImpl*>(defaultView.get())->GetNativeView();
            }
            ca.loadOp = (att.LoadOp == AttachmentLoadOp::Clear) ? WGPULoadOp_Clear : WGPULoadOp_Load;
            ca.storeOp = (att.StoreOp == AttachmentStoreOp::Store) ? WGPUStoreOp_Store : WGPUStoreOp_Discard;
            ca.clearValue = {att.ClearValue.Color[0], att.ClearValue.Color[1],
                             att.ClearValue.Color[2], att.ClearValue.Color[3]};
            colorAttachments.push_back(ca);
        }

        WGPURenderPassDescriptor rpDesc = {};
        rpDesc.colorAttachmentCount = static_cast<uint32_t>(colorAttachments.size());
        rpDesc.colorAttachments = colorAttachments.data();

        WGPURenderPassDepthStencilAttachment depthAtt = {};
        if (info.DepthAttachment)
        {
            auto* depthTex = static_cast<WGPUTextureImpl*>(info.DepthAttachment->Texture.get());
            auto depthView = depthTex->GetDefaultView();
            if (depthView) {
                depthAtt.view = static_cast<WGPUTextureViewImpl*>(depthView.get())->GetNativeView();
            }
            depthAtt.depthLoadOp = (info.DepthAttachment->LoadOp == AttachmentLoadOp::Clear)
                ? WGPULoadOp_Clear : WGPULoadOp_Load;
            depthAtt.depthStoreOp = (info.DepthAttachment->StoreOp == AttachmentStoreOp::Store)
                ? WGPUStoreOp_Store : WGPUStoreOp_Discard;
            depthAtt.depthClearValue = info.DepthAttachment->ClearDepthStencil.Depth;
            rpDesc.depthStencilAttachment = &depthAtt;
        }

        m_renderPass = wgpuCommandEncoderBeginRenderPass(m_encoder, &rpDesc);
    }

    void WGPUCommandBufferEncoder::EndRendering()
    {
        if (m_renderPass) {
            wgpuRenderPassEncoderEnd(m_renderPass);
            wgpuRenderPassEncoderRelease(m_renderPass);
            m_renderPass = nullptr;
        }
    }

    void WGPUCommandBufferEncoder::BindGraphicsPipeline(const Ref<GraphicsPipeline>& pipeline)
    {
        if (!m_renderPass) return;
        auto* gp = static_cast<WGPUGraphicsPipeline*>(pipeline.get());
        wgpuRenderPassEncoderSetPipeline(m_renderPass, gp->GetHandle());
    }

    void WGPUCommandBufferEncoder::BindComputePipeline(const Ref<ComputePipeline>& pipeline)
    {
        if (!m_computePass) {
            if (!m_encoder) return;
            WGPUComputePassDescriptor cpDesc = {};
            m_computePass = wgpuCommandEncoderBeginComputePass(m_encoder, &cpDesc);
        }
        auto* cp = static_cast<WGPUComputePipelineImpl*>(pipeline.get());
        wgpuComputePassEncoderSetPipeline(m_computePass, cp->GetHandle());
    }

    void WGPUCommandBufferEncoder::SetViewport(const Viewport& viewport)
    {
        if (!m_renderPass) return;
        wgpuRenderPassEncoderSetViewport(m_renderPass,
            viewport.X, viewport.Y, viewport.Width, viewport.Height,
            viewport.MinDepth, viewport.MaxDepth);
    }

    void WGPUCommandBufferEncoder::SetScissor(const Rect2D& scissor)
    {
        if (!m_renderPass) return;
        wgpuRenderPassEncoderSetScissorRect(m_renderPass,
            static_cast<uint32_t>(scissor.OffsetX), static_cast<uint32_t>(scissor.OffsetY),
            scissor.Width, scissor.Height);
    }

    void WGPUCommandBufferEncoder::BindVertexBuffer(uint32_t binding, const Ref<Buffer>& buffer, uint64_t offset)
    {
        if (!m_renderPass) return;
        auto* buf = static_cast<WGPUBufferImpl*>(buffer.get());
        wgpuRenderPassEncoderSetVertexBuffer(m_renderPass, binding,
            buf->GetNativeBuffer(), offset, buf->GetSize() - offset);
    }

    void WGPUCommandBufferEncoder::BindIndexBuffer(const Ref<Buffer>& buffer, uint64_t offset, IndexType indexType)
    {
        if (!m_renderPass) return;
        auto* buf = static_cast<WGPUBufferImpl*>(buffer.get());
        WGPUIndexFormat fmt = (indexType == IndexType::UInt16) ? WGPUIndexFormat_Uint16 : WGPUIndexFormat_Uint32;
        wgpuRenderPassEncoderSetIndexBuffer(m_renderPass,
            buf->GetNativeBuffer(), fmt, offset, buf->GetSize() - offset);
    }

    void WGPUCommandBufferEncoder::BindDescriptorSets(const Ref<GraphicsPipeline>& pipeline, uint32_t firstSet,
                                                       std::span<const Ref<DescriptorSet>> descriptorSets)
    {
        if (!m_renderPass) return;
        for (uint32_t i = 0; i < descriptorSets.size(); i++)
        {
            auto* ds = static_cast<WGPUDescriptorSet*>(descriptorSets[i].get());
            // WebGPU BindGroup access pending WGPUDescriptorSet::GetBindGroup() implementation
            (void)ds;
        }
    }

    void WGPUCommandBufferEncoder::PushConstants(const Ref<GraphicsPipeline>&, ShaderStage,
                                                  uint32_t, uint32_t, const void*)
    {
        // WebGPU has no push constants; emulate via uniform buffer
    }

    void WGPUCommandBufferEncoder::ComputePushConstants(const Ref<ComputePipeline>&, ShaderStage,
                                                         uint32_t, uint32_t, const void*)
    {
        // WebGPU has no push constants
    }

    void WGPUCommandBufferEncoder::Draw(uint32_t vertexCount, uint32_t instanceCount,
                                         uint32_t firstVertex, uint32_t firstInstance)
    {
        if (!m_renderPass) return;
        wgpuRenderPassEncoderDraw(m_renderPass, vertexCount, instanceCount, firstVertex, firstInstance);
    }

    void WGPUCommandBufferEncoder::DrawIndexed(uint32_t indexCount, uint32_t instanceCount,
                                                uint32_t firstIndex, int32_t vertexOffset, uint32_t firstInstance)
    {
        if (!m_renderPass) return;
        wgpuRenderPassEncoderDrawIndexed(m_renderPass, indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
    }

    void WGPUCommandBufferEncoder::Dispatch(uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ)
    {
        if (!m_encoder) return;
        if (!m_computePass) {
            WGPUComputePassDescriptor cpDesc = {};
            m_computePass = wgpuCommandEncoderBeginComputePass(m_encoder, &cpDesc);
        }
        wgpuComputePassEncoderDispatchWorkgroups(m_computePass, groupCountX, groupCountY, groupCountZ);
        wgpuComputePassEncoderEnd(m_computePass);
        wgpuComputePassEncoderRelease(m_computePass);
        m_computePass = nullptr;
    }

    void WGPUCommandBufferEncoder::BindComputeDescriptorSets(const Ref<ComputePipeline>& pipeline, uint32_t firstSet,
                                                              std::span<const Ref<DescriptorSet>> descriptorSets)
    {
        if (!m_computePass) return;
        for (uint32_t i = 0; i < descriptorSets.size(); i++)
        {
            auto* ds = static_cast<WGPUDescriptorSet*>(descriptorSets[i].get());
            // WebGPU BindGroup access pending WGPUDescriptorSet::GetBindGroup() implementation
            (void)ds;
        }
    }

    void WGPUCommandBufferEncoder::CopyBuffer(const Ref<Buffer>& src, const Ref<Buffer>& dst,
                                               uint64_t srcOffset, uint64_t dstOffset, uint64_t size)
    {
        if (!m_encoder) return;
        auto* srcBuf = static_cast<WGPUBufferImpl*>(src.get());
        auto* dstBuf = static_cast<WGPUBufferImpl*>(dst.get());
        wgpuCommandEncoderCopyBufferToBuffer(m_encoder,
            srcBuf->GetNativeBuffer(), srcOffset,
            dstBuf->GetNativeBuffer(), dstOffset, size);
    }

    void WGPUCommandBufferEncoder::CopyBufferToImage(const Ref<Buffer>& src, const Ref<Texture>& dst,
                                                      ResourceState dstState, std::span<const BufferImageCopy> regions)
    {
        if (!m_encoder) return;
        auto* srcBuf = static_cast<WGPUBufferImpl*>(src.get());
        auto* dstTex = static_cast<WGPUTextureImpl*>(dst.get());

        for (auto& region : regions)
        {
            WGPUImageCopyBuffer srcCopy = {};
            srcCopy.buffer = srcBuf->GetNativeBuffer();
            srcCopy.layout.offset = region.BufferOffset;
            srcCopy.layout.bytesPerRow = region.BufferRowLength > 0 ? region.BufferRowLength * 4 : region.ImageExtentWidth * 4;
            srcCopy.layout.rowsPerImage = region.BufferImageHeight > 0 ? region.BufferImageHeight : region.ImageExtentHeight;

            WGPUImageCopyTexture dstCopy = {};
            dstCopy.texture = dstTex->GetNativeTexture();
            dstCopy.mipLevel = region.ImageSubresource.MipLevel;
            dstCopy.origin = {
                static_cast<uint32_t>(region.ImageOffsetX),
                static_cast<uint32_t>(region.ImageOffsetY),
                static_cast<uint32_t>(region.ImageOffsetZ)
            };

            WGPUExtent3D extent = {region.ImageExtentWidth, region.ImageExtentHeight, region.ImageExtentDepth};
            wgpuCommandEncoderCopyBufferToTexture(m_encoder, &srcCopy, &dstCopy, &extent);
        }
    }

    void WGPUCommandBufferEncoder::CopyTexture2D(const Ref<Texture>& src, const Ref<Texture>& dst)
    {
        if (!m_encoder) return;
        auto* srcTex = static_cast<WGPUTextureImpl*>(src.get());
        auto* dstTex = static_cast<WGPUTextureImpl*>(dst.get());

        WGPUImageCopyTexture srcCopy = {};
        srcCopy.texture = srcTex->GetNativeTexture();
        srcCopy.mipLevel = 0;
        srcCopy.origin = {0, 0, 0};

        WGPUImageCopyTexture dstCopy = {};
        dstCopy.texture = dstTex->GetNativeTexture();
        dstCopy.mipLevel = 0;
        dstCopy.origin = {0, 0, 0};

        WGPUExtent3D extent = {src->GetWidth(), src->GetHeight(), 1};
        wgpuCommandEncoderCopyTextureToTexture(m_encoder, &srcCopy, &dstCopy, &extent);
    }

    // WebGPU manages resource barriers automatically
    void WGPUCommandBufferEncoder::PipelineBarrier(PipelineStage, PipelineStage,
                                                    std::span<const CMemoryBarrier>,
                                                    std::span<const BufferBarrier>,
                                                    std::span<const TextureBarrier>) {}

    void WGPUCommandBufferEncoder::TransitionImage(const Ref<Texture>&, ImageTransition,
                                                    const ImageSubresourceRange&) {}

    void WGPUCommandBufferEncoder::TransitionBuffer(const Ref<Buffer>&, BufferTransition, uint64_t, uint64_t) {}

    void WGPUCommandBufferEncoder::MemoryBarrierFast(MemoryTransition) {}

    void WGPUCommandBufferEncoder::ExecuteNative(const std::function<void(void*)>& func)
    {
        if (m_encoder) func(m_encoder);
    }

    void* WGPUCommandBufferEncoder::GetNativeHandle() { return m_encoder; }

    void WGPUCommandBufferEncoder::ResolveTexture(const Ref<Texture>&, const Ref<Texture>&,
                                                   const ImageSubresourceLayers&, const ImageSubresourceLayers&) {}

    void WGPUCommandBufferEncoder::DrawIndirect(const Ref<Buffer>& argBuffer, uint64_t offset,
                                                 uint32_t drawCount, uint32_t stride)
    {
        if (!m_renderPass) return;
        auto* buf = static_cast<WGPUBufferImpl*>(argBuffer.get());
        for (uint32_t i = 0; i < drawCount; i++) {
            wgpuRenderPassEncoderDrawIndirect(m_renderPass, buf->GetNativeBuffer(), offset + i * stride);
        }
    }

    void WGPUCommandBufferEncoder::DrawIndexedIndirect(const Ref<Buffer>& argBuffer, uint64_t offset,
                                                        uint32_t drawCount, uint32_t stride)
    {
        if (!m_renderPass) return;
        auto* buf = static_cast<WGPUBufferImpl*>(argBuffer.get());
        for (uint32_t i = 0; i < drawCount; i++) {
            wgpuRenderPassEncoderDrawIndexedIndirect(m_renderPass, buf->GetNativeBuffer(), offset + i * stride);
        }
    }

    void WGPUCommandBufferEncoder::DispatchIndirect(const Ref<Buffer>& argBuffer, uint64_t offset)
    {
        if (!m_encoder) return;
        if (!m_computePass) {
            WGPUComputePassDescriptor cpDesc = {};
            m_computePass = wgpuCommandEncoderBeginComputePass(m_encoder, &cpDesc);
        }
        auto* buf = static_cast<WGPUBufferImpl*>(argBuffer.get());
        wgpuComputePassEncoderDispatchWorkgroupsIndirect(m_computePass, buf->GetNativeBuffer(), offset);
        wgpuComputePassEncoderEnd(m_computePass);
        wgpuComputePassEncoderRelease(m_computePass);
        m_computePass = nullptr;
    }

    // Ray tracing not supported in WebGPU
    void WGPUCommandBufferEncoder::BuildAccelerationStructure(const Ref<AccelerationStructure>&) {}
    void WGPUCommandBufferEncoder::BindRayTracingPipeline(const Ref<RayTracingPipeline>&) {}
    void WGPUCommandBufferEncoder::BindRayTracingDescriptorSets(const Ref<RayTracingPipeline>&, uint32_t,
                                                                 std::span<const Ref<DescriptorSet>>) {}
    void WGPUCommandBufferEncoder::TraceRays(const Ref<ShaderBindingTable>&, uint32_t, uint32_t, uint32_t) {}
}

#include "Buffer.h"
#include "Impls/D3D11/D3D11BindingGroup.h"
#include "Impls/D3D11/D3D11Buffer.h"
#include "Impls/D3D11/D3D11CommandBufferEncoder.h"
#include "Impls/D3D11/D3D11Device.h"
#include "Impls/D3D11/D3D11Pipeline.h"
#include "Impls/D3D11/D3D11Texture.h"

namespace luna::RHI {
D3D11CommandBufferEncoder::D3D11CommandBufferEncoder(Ref<D3D11Device> device, CommandBufferType type)
    : m_device(std::move(device)),
      m_type(type)
{
    if (type == CommandBufferType::Secondary) {
        m_device->GetNativeDevice()->CreateDeferredContext(
            0, reinterpret_cast<ID3D11DeviceContext**>(m_context.GetAddressOf()));
        m_isDeferred = true;
    } else {
        m_context = m_device->GetImmediateContext();
        m_isDeferred = false;
    }
}

void D3D11CommandBufferEncoder::Reset()
{ /* DX11: no-op for immediate context */
}

void D3D11CommandBufferEncoder::Begin(const CommandBufferBeginInfo& info)
{ /* no-op */
}

void D3D11CommandBufferEncoder::End()
{ /* no-op for immediate context */
}

void D3D11CommandBufferEncoder::BeginRendering(const RenderingInfo& info)
{
#ifndef NDEBUG
    for (auto& att : info.ColorAttachments) {
        if (att.Texture && m_transitionedTextures.find(att.Texture.get()) == m_transitionedTextures.end()) {
            fprintf(stderr,
                    "[Luna RHI WARNING] DX11: Texture used in BeginRendering without TransitionImage(). "
                    "This will cause errors on Vulkan/DX12.\n");
        }
    }
#endif
    std::vector<ID3D11RenderTargetView*> rtvs;
    for (auto& att : info.ColorAttachments) {
        auto* d3dTex = static_cast<D3D11Texture*>(att.Texture.get());
        rtvs.push_back(d3dTex->GetRTV());
        if (att.LoadOp == AttachmentLoadOp::Clear) {
            m_context->ClearRenderTargetView(d3dTex->GetRTV(), att.ClearValue.Color);
        }
    }

    ID3D11DepthStencilView* dsv = nullptr;
    if (info.DepthAttachment) {
        auto* depthTex = static_cast<D3D11Texture*>(info.DepthAttachment->Texture.get());
        dsv = depthTex->GetDSV();
        if (info.DepthAttachment->LoadOp == AttachmentLoadOp::Clear) {
            m_context->ClearDepthStencilView(dsv,
                                             D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL,
                                             info.DepthAttachment->ClearDepthStencil.Depth,
                                             info.DepthAttachment->ClearDepthStencil.Stencil);
        }
    }

    m_context->OMSetRenderTargets(static_cast<UINT>(rtvs.size()), rtvs.data(), dsv);
}

void D3D11CommandBufferEncoder::EndRendering()
{
    m_context->OMSetRenderTargets(0, nullptr, nullptr);
}

void D3D11CommandBufferEncoder::BindGraphicsPipeline(const Ref<GraphicsPipeline>& pipeline)
{
    auto* p = static_cast<D3D11GraphicsPipeline*>(pipeline.get());
    m_context->VSSetShader(p->GetVertexShader(), nullptr, 0);
    m_context->PSSetShader(p->GetPixelShader(), nullptr, 0);
    m_context->IASetInputLayout(p->GetInputLayout());
    m_context->IASetPrimitiveTopology(p->GetTopology());
    m_context->RSSetState(p->GetRasterizerState());
    float blendFactor[] = {0, 0, 0, 0};
    m_context->OMSetBlendState(p->GetBlendState(), blendFactor, 0xFF'FF'FF'FF);
    m_context->OMSetDepthStencilState(p->GetDepthStencilState(), 0);
    m_currentVBStride = p->GetVertexStride();
}

void D3D11CommandBufferEncoder::BindComputePipeline(const Ref<ComputePipeline>& pipeline)
{
    auto* p = static_cast<D3D11ComputePipeline*>(pipeline.get());
    m_context->CSSetShader(p->GetComputeShader(), nullptr, 0);
}

void D3D11CommandBufferEncoder::SetViewport(const Viewport& viewport)
{
    D3D11_VIEWPORT vp{viewport.X, viewport.Y, viewport.Width, viewport.Height, viewport.MinDepth, viewport.MaxDepth};
    m_context->RSSetViewports(1, &vp);
}

void D3D11CommandBufferEncoder::SetScissor(const Rect2D& scissor)
{
    D3D11_RECT r{scissor.OffsetX,
                 scissor.OffsetY,
                 static_cast<LONG>(scissor.OffsetX + scissor.Width),
                 static_cast<LONG>(scissor.OffsetY + scissor.Height)};
    m_context->RSSetScissorRects(1, &r);
}

void D3D11CommandBufferEncoder::BindVertexBuffer(uint32_t binding, const Ref<Buffer>& buffer, uint64_t offset)
{
#ifndef NDEBUG
    if (buffer && !(buffer->GetUsage() & BufferUsageFlags::VertexBuffer)) {
        fprintf(stderr,
                "[Luna RHI WARNING] DX11: BindVertexBuffer without VertexBuffer usage flag. "
                "Vulkan will reject this.\n");
    }
#endif
    auto* d3dBuf = static_cast<D3D11Buffer*>(buffer.get());
    ID3D11Buffer* bufs[] = {d3dBuf->GetNativeBuffer()};
    UINT strides[] = {m_currentVBStride};
    UINT offsets[] = {static_cast<UINT>(offset)};
    m_context->IASetVertexBuffers(binding, 1, bufs, strides, offsets);
}

void D3D11CommandBufferEncoder::BindIndexBuffer(const Ref<Buffer>& buffer, uint64_t offset, IndexType indexType)
{
#ifndef NDEBUG
    if (buffer && !(buffer->GetUsage() & BufferUsageFlags::IndexBuffer)) {
        fprintf(stderr,
                "[Luna RHI WARNING] DX11: BindIndexBuffer without IndexBuffer usage flag. "
                "Vulkan will reject this.\n");
    }
#endif
    auto* d3dBuf = static_cast<D3D11Buffer*>(buffer.get());
    DXGI_FORMAT fmt = (indexType == IndexType::UInt16) ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT;
    m_context->IASetIndexBuffer(d3dBuf->GetNativeBuffer(), fmt, static_cast<UINT>(offset));
}

void D3D11CommandBufferEncoder::BindDescriptorSets(const Ref<GraphicsPipeline>& pipeline,
                                                   uint32_t firstSet,
                                                   std::span<const Ref<DescriptorSet>> descriptorSets)
{
    for (auto& ds : descriptorSets) {
        auto* d3dDS = static_cast<D3D11DescriptorSet*>(ds.get());
        d3dDS->Bind(m_context.Get(), ShaderStage::AllGraphics);
    }
}

void D3D11CommandBufferEncoder::PushConstants(
    const Ref<GraphicsPipeline>& pipeline, ShaderStage stageFlags, uint32_t offset, uint32_t size, const void* data)
{
    uint32_t alignedSize = (size + 15) & ~15;
    D3D11_BUFFER_DESC desc{};
    desc.ByteWidth = alignedSize;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;

    ComPtr<ID3D11Buffer> cb;
    m_device->GetNativeDevice()->CreateBuffer(&desc, nullptr, &cb);
    if (!cb) {
        return;
    }
    m_context->UpdateSubresource(cb.Get(), 0, nullptr, data, 0, 0);

    constexpr UINT slot = 15;
    ID3D11Buffer* bufs[] = {cb.Get()};
    if (stageFlags & ShaderStage::Vertex) {
        m_context->VSSetConstantBuffers(slot, 1, bufs);
    }
    if (stageFlags & ShaderStage::Fragment) {
        m_context->PSSetConstantBuffers(slot, 1, bufs);
    }
    if (stageFlags & ShaderStage::Geometry) {
        m_context->GSSetConstantBuffers(slot, 1, bufs);
    }
}

void D3D11CommandBufferEncoder::Draw(uint32_t vertexCount,
                                     uint32_t instanceCount,
                                     uint32_t firstVertex,
                                     uint32_t firstInstance)
{
    if (instanceCount > 1) {
        m_context->DrawInstanced(vertexCount, instanceCount, firstVertex, firstInstance);
    } else {
        m_context->Draw(vertexCount, firstVertex);
    }
}

void D3D11CommandBufferEncoder::DrawIndexed(
    uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t vertexOffset, uint32_t firstInstance)
{
    if (instanceCount > 1) {
        m_context->DrawIndexedInstanced(indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
    } else {
        m_context->DrawIndexed(indexCount, firstIndex, vertexOffset);
    }
}

void D3D11CommandBufferEncoder::Dispatch(uint32_t x, uint32_t y, uint32_t z)
{
    m_context->Dispatch(x, y, z);
}

void D3D11CommandBufferEncoder::BindComputeDescriptorSets(const Ref<ComputePipeline>& pipeline,
                                                          uint32_t firstSet,
                                                          std::span<const Ref<DescriptorSet>> descriptorSets)
{
    for (auto& ds : descriptorSets) {
        auto* d3dDS = static_cast<D3D11DescriptorSet*>(ds.get());
        d3dDS->Bind(m_context.Get(), ShaderStage::Compute);
    }
}

void D3D11CommandBufferEncoder::ComputePushConstants(
    const Ref<ComputePipeline>& pipeline, ShaderStage stageFlags, uint32_t offset, uint32_t size, const void* data)
{
    uint32_t alignedSize = (size + 15) & ~15;
    D3D11_BUFFER_DESC desc{};
    desc.ByteWidth = alignedSize;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;

    ComPtr<ID3D11Buffer> cb;
    m_device->GetNativeDevice()->CreateBuffer(&desc, nullptr, &cb);
    if (!cb) {
        return;
    }
    m_context->UpdateSubresource(cb.Get(), 0, nullptr, data, 0, 0);
    ID3D11Buffer* bufs[] = {cb.Get()};
    m_context->CSSetConstantBuffers(15, 1, bufs);
}

void D3D11CommandBufferEncoder::PipelineBarrier(SyncScope srcStage,
                                                SyncScope dstStage,
                                                std::span<const CMemoryBarrier> globalBarriers,
                                                std::span<const BufferBarrier> bufferBarriers,
                                                std::span<const TextureBarrier> textureBarriers)
{
    // DX11: no explicit barriers, driver manages resource hazards
}

void D3D11CommandBufferEncoder::TransitionImage(const Ref<Texture>& texture,
                                                ImageTransition transition,
                                                const ImageSubresourceRange& range)
{
#ifndef NDEBUG
    if (texture) {
        m_transitionedTextures.insert(texture.get());
    }
#endif
}

void D3D11CommandBufferEncoder::TransitionBuffer(const Ref<Buffer>& buffer,
                                                 BufferTransition transition,
                                                 uint64_t offset,
                                                 uint64_t size)
{
    // DX11: no-op
}

void D3D11CommandBufferEncoder::ExecuteNative(const std::function<void(void*)>& func)
{
    func(m_context.Get());
}

void* D3D11CommandBufferEncoder::GetNativeHandle()
{
    return m_context.Get();
}

void D3D11CommandBufferEncoder::CopyBufferToImage(const Ref<Buffer>& srcBuffer,
                                                  const Ref<Texture>& dstImage,
                                                  ResourceState dstImageLayout,
                                                  std::span<const BufferImageCopy> regions)
{
#ifndef NDEBUG
    if (dstImage && m_transitionedTextures.find(dstImage.get()) == m_transitionedTextures.end()) {
        fprintf(stderr,
                "[Luna RHI WARNING] DX11: CopyBufferToImage without TransitionImage(). "
                "Vulkan/DX12 will fail.\n");
    }
#endif
    auto* srcBuf = static_cast<D3D11Buffer*>(srcBuffer.get());
    auto* dstTex = static_cast<D3D11Texture*>(dstImage.get());
    if (!srcBuf || !dstTex || !dstTex->GetNativeTexture()) {
        return;
    }

    void* mapped = srcBuf->Map();
    if (!mapped) {
        return;
    }

    for (auto& region : regions) {
        uint32_t subresource = D3D11CalcSubresource(
            region.ImageSubresource.MipLevel, region.ImageSubresource.BaseArrayLayer, dstImage->GetMipLevels());

        const uint8_t* srcData = static_cast<const uint8_t*>(mapped) + region.BufferOffset;
        uint32_t rowPitch = region.BufferRowLength > 0 ? region.BufferRowLength : region.ImageExtentWidth * 4;

        D3D11_BOX dstBox{};
        dstBox.left = region.ImageOffsetX;
        dstBox.top = region.ImageOffsetY;
        dstBox.front = region.ImageOffsetZ;
        dstBox.right = region.ImageOffsetX + region.ImageExtentWidth;
        dstBox.bottom = region.ImageOffsetY + region.ImageExtentHeight;
        dstBox.back = region.ImageOffsetZ + region.ImageExtentDepth;

        m_context->UpdateSubresource(dstTex->GetNativeTexture(), subresource, &dstBox, srcData, rowPitch, 0);
    }

    srcBuf->Unmap();
}

void D3D11CommandBufferEncoder::CopyTexture2D(const Ref<Texture>& src, const Ref<Texture>& dst)
{
    auto* srcTex = static_cast<D3D11Texture*>(src.get());
    auto* dstTex = static_cast<D3D11Texture*>(dst.get());
    if (src->GetWidth() == dst->GetWidth() && src->GetHeight() == dst->GetHeight()) {
        m_context->CopyResource(dstTex->GetNativeTexture(), srcTex->GetNativeTexture());
    } else {
        D3D11_BOX box = {
            0, 0, 0, std::min(src->GetWidth(), dst->GetWidth()), std::min(src->GetHeight(), dst->GetHeight()), 1};
        m_context->CopySubresourceRegion(dstTex->GetNativeTexture(), 0, 0, 0, 0, srcTex->GetNativeTexture(), 0, &box);
    }
}

void D3D11CommandBufferEncoder::CopyBuffer(
    const Ref<Buffer>& srcBuffer, const Ref<Buffer>& dstBuffer, uint64_t srcOffset, uint64_t dstOffset, uint64_t size)
{
    auto* src = static_cast<D3D11Buffer*>(srcBuffer.get());
    auto* dst = static_cast<D3D11Buffer*>(dstBuffer.get());
    D3D11_BOX box{static_cast<UINT>(srcOffset), 0, 0, static_cast<UINT>(srcOffset + size), 1, 1};
    m_context->CopySubresourceRegion(
        dst->GetNativeBuffer(), 0, static_cast<UINT>(dstOffset), 0, 0, src->GetNativeBuffer(), 0, &box);
}

void D3D11CommandBufferEncoder::ResolveTexture(const Ref<Texture>& srcTexture,
                                               const Ref<Texture>& dstTexture,
                                               const ImageSubresourceLayers& srcSub,
                                               const ImageSubresourceLayers& dstSub)
{
    auto* src = static_cast<D3D11Texture*>(srcTexture.get());
    auto* dst = static_cast<D3D11Texture*>(dstTexture.get());
    DXGI_FORMAT format = D3D11_ToDXGIFormat(src->GetFormat());
    m_context->ResolveSubresource(
        dst->GetNativeTexture(), dstSub.MipLevel, src->GetNativeTexture(), srcSub.MipLevel, format);
}

void D3D11CommandBufferEncoder::DrawIndirect(const Ref<Buffer>& argBuffer,
                                             uint64_t offset,
                                             uint32_t drawCount,
                                             uint32_t stride)
{
    auto* buf = static_cast<D3D11Buffer*>(argBuffer.get());
    for (uint32_t i = 0; i < drawCount; ++i) {
        m_context->DrawInstancedIndirect(buf->GetNativeBuffer(), static_cast<UINT>(offset + i * stride));
    }
}

void D3D11CommandBufferEncoder::DrawIndexedIndirect(const Ref<Buffer>& argBuffer,
                                                    uint64_t offset,
                                                    uint32_t drawCount,
                                                    uint32_t stride)
{
    auto* buf = static_cast<D3D11Buffer*>(argBuffer.get());
    for (uint32_t i = 0; i < drawCount; ++i) {
        m_context->DrawIndexedInstancedIndirect(buf->GetNativeBuffer(), static_cast<UINT>(offset + i * stride));
    }
}

void D3D11CommandBufferEncoder::DispatchIndirect(const Ref<Buffer>& argBuffer, uint64_t offset)
{
    auto* buf = static_cast<D3D11Buffer*>(argBuffer.get());
    m_context->DispatchIndirect(buf->GetNativeBuffer(), static_cast<UINT>(offset));
}
} // namespace luna::RHI

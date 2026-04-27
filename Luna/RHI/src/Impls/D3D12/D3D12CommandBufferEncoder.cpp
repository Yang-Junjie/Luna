#include "Impls/D3D12/D3D12AccelerationStructure.h"
#include "Impls/D3D12/D3D12Buffer.h"
#include "Impls/D3D12/D3D12CommandBufferEncoder.h"
#include "Impls/D3D12/D3D12DescriptorSet.h"
#include "Impls/D3D12/D3D12Device.h"
#include "Impls/D3D12/D3D12Pipeline.h"
#include "Impls/D3D12/D3D12PipelineLayout.h"
#include "Impls/D3D12/D3D12QueryPool.h"
#include "Impls/D3D12/D3D12RayTracingPipeline.h"
#include "Impls/D3D12/D3D12ShaderBindingTable.h"
#include "Impls/D3D12/D3D12Texture.h"
#ifdef USE_PIX
#include <pix3.h>
#endif

#include <array>
#include <stdexcept>

namespace luna::RHI {
static inline UINT CalcSubresource(UINT mipSlice, UINT arraySlice, UINT planeSlice, UINT mipLevels, UINT arraySize)
{
    return mipSlice + arraySlice * mipLevels + planeSlice * mipLevels * arraySize;
}

static void AssignDescriptorHeap(ID3D12DescriptorHeap*& target, ID3D12DescriptorHeap* candidate, const char* heapType)
{
    if (candidate == nullptr) {
        return;
    }

    if (target != nullptr && target != candidate) {
        std::string message = "D3D12 requires all bound descriptor sets to share a single ";
        message += heapType;
        message += " heap";
        throw std::runtime_error(message);
    }

    target = candidate;
}

D3D12CommandBufferEncoder::D3D12CommandBufferEncoder(const Ref<Device>& device,
                                                     CommandBufferType type,
                                                     ComPtr<ID3D12CommandAllocator> allocator)
    : m_device(device),
      m_type(type),
      m_allocator(std::move(allocator))
{
    auto d3dDevice = std::dynamic_pointer_cast<D3D12Device>(device);
    d3dDevice->GetHandle()->CreateCommandList(
        0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_allocator.Get(), nullptr, IID_PPV_ARGS(&m_commandList));
    m_commandList->Close();
}

void D3D12CommandBufferEncoder::Free()
{
    auto d3dDevice = std::dynamic_pointer_cast<D3D12Device>(m_device);
    if (d3dDevice) {
        d3dDevice->FreeCommandBuffer(shared_from_this());
    }
}

void D3D12CommandBufferEncoder::Reset()
{
    if (m_isRecording) {
        m_commandList->Close();
        m_isRecording = false;
    }
    m_currentPipeline = nullptr;
}

void D3D12CommandBufferEncoder::ReturnToPool()
{
    auto d3dDevice = std::dynamic_pointer_cast<D3D12Device>(m_device);
    if (d3dDevice) {
        d3dDevice->ReturnCommandBuffer(shared_from_this());
    }
}

void D3D12CommandBufferEncoder::Begin(const CommandBufferBeginInfo& info)
{
    if (!m_isRecording) {
        m_allocator->Reset();
        m_commandList->Reset(m_allocator.Get(), nullptr);
        m_currentPipeline = nullptr;
        m_isRecording = true;
    }
}

void D3D12CommandBufferEncoder::End()
{
    m_commandList->Close();
    m_isRecording = false;
    m_currentPipeline = nullptr;
}

void D3D12CommandBufferEncoder::BeginRendering(const RenderingInfo& info)
{
    std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> rtvHandles;
    for (auto& att : info.ColorAttachments) {
        auto* d3dTex = static_cast<D3D12Texture*>(att.Texture.get());
        d3dTex->CreateDefaultViewIfNeeded();
        auto view = std::dynamic_pointer_cast<D3D12TextureView>(d3dTex->GetDefaultView());
        if (view && view->HasRTV()) {
            rtvHandles.push_back(view->GetRTVHandle());
            if (att.LoadOp == AttachmentLoadOp::Clear) {
                m_commandList->ClearRenderTargetView(view->GetRTVHandle(), att.ClearValue.Color, 0, nullptr);
            }
        }
    }
    D3D12_CPU_DESCRIPTOR_HANDLE* pDSV = nullptr;
    D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle{};
    if (info.DepthAttachment) {
        auto* depthTex = static_cast<D3D12Texture*>(info.DepthAttachment->Texture.get());
        depthTex->CreateDefaultViewIfNeeded();
        auto view = std::dynamic_pointer_cast<D3D12TextureView>(depthTex->GetDefaultView());
        if (view && view->HasDSV()) {
            dsvHandle = view->GetDSVHandle();
            pDSV = &dsvHandle;
            if (info.DepthAttachment->LoadOp == AttachmentLoadOp::Clear) {
                D3D12_CLEAR_FLAGS clearFlags = D3D12_CLEAR_FLAG_DEPTH;
                if (depthTex->HasStencil()) {
                    clearFlags |= D3D12_CLEAR_FLAG_STENCIL;
                }
                m_commandList->ClearDepthStencilView(dsvHandle,
                                                     clearFlags,
                                                     info.DepthAttachment->ClearDepthStencil.Depth,
                                                     info.DepthAttachment->ClearDepthStencil.Stencil,
                                                     0,
                                                     nullptr);
            }
        }
    }
    m_commandList->OMSetRenderTargets(
        static_cast<UINT>(rtvHandles.size()), rtvHandles.empty() ? nullptr : rtvHandles.data(), FALSE, pDSV);
}

void D3D12CommandBufferEncoder::EndRendering()
{ /* D3D12: no-op */
}

void D3D12CommandBufferEncoder::BindGraphicsPipeline(const Ref<GraphicsPipeline>& pipeline)
{
    auto* d3dPipeline = static_cast<D3D12GraphicsPipeline*>(pipeline.get());
    if (!d3dPipeline->GetHandle()) {
        throw std::runtime_error("D3D12 graphics pipeline state is null");
    }
    m_currentPipeline = d3dPipeline;
    m_commandList->SetPipelineState(d3dPipeline->GetHandle());
    if (d3dPipeline->GetRootSignature()) {
        m_commandList->SetGraphicsRootSignature(d3dPipeline->GetRootSignature());
    }
    m_commandList->IASetPrimitiveTopology(d3dPipeline->GetTopology());
}

void D3D12CommandBufferEncoder::BindComputePipeline(const Ref<ComputePipeline>& pipeline)
{
    auto* d3dPipeline = static_cast<D3D12ComputePipeline*>(pipeline.get());
    m_commandList->SetPipelineState(d3dPipeline->GetHandle());
    m_commandList->SetComputeRootSignature(d3dPipeline->GetRootSignature());
}

void D3D12CommandBufferEncoder::SetViewport(const Viewport& viewport)
{
    D3D12_VIEWPORT vp;
    vp.TopLeftX = viewport.X;
    vp.TopLeftY = viewport.Y + viewport.Height;
    vp.Width = viewport.Width;
    vp.Height = -viewport.Height;
    vp.MinDepth = viewport.MinDepth;
    vp.MaxDepth = viewport.MaxDepth;
    m_commandList->RSSetViewports(1, &vp);
}

void D3D12CommandBufferEncoder::SetScissor(const Rect2D& scissor)
{
    D3D12_RECT r{scissor.OffsetX,
                 scissor.OffsetY,
                 static_cast<LONG>(scissor.OffsetX + scissor.Width),
                 static_cast<LONG>(scissor.OffsetY + scissor.Height)};
    m_commandList->RSSetScissorRects(1, &r);
}

void D3D12CommandBufferEncoder::BindVertexBuffer(uint32_t binding, const Ref<Buffer>& buffer, uint64_t offset)
{
    auto* d3dBuf = static_cast<D3D12Buffer*>(buffer.get());
    D3D12_VERTEX_BUFFER_VIEW vbv{};
    vbv.BufferLocation = d3dBuf->GetHandle()->GetGPUVirtualAddress() + offset;
    vbv.SizeInBytes = static_cast<UINT>(buffer->GetSize() - offset);
    vbv.StrideInBytes = m_currentPipeline ? m_currentPipeline->GetVertexStride(binding) : 0;
    m_commandList->IASetVertexBuffers(binding, 1, &vbv);
}

void D3D12CommandBufferEncoder::BindIndexBuffer(const Ref<Buffer>& buffer, uint64_t offset, IndexType indexType)
{
    auto* d3dBuf = static_cast<D3D12Buffer*>(buffer.get());
    D3D12_INDEX_BUFFER_VIEW ibv{};
    ibv.BufferLocation = d3dBuf->GetHandle()->GetGPUVirtualAddress() + offset;
    ibv.SizeInBytes = static_cast<UINT>(buffer->GetSize() - offset);
    ibv.Format = (indexType == IndexType::UInt16) ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT;
    m_commandList->IASetIndexBuffer(&ibv);
}

void D3D12CommandBufferEncoder::BindDescriptorSets(const Ref<GraphicsPipeline>& pipeline,
                                                   uint32_t firstSet,
                                                   std::span<const Ref<DescriptorSet>> descriptorSets)
{
    if (descriptorSets.empty()) {
        return;
    }

    auto d3dPipeline = std::dynamic_pointer_cast<D3D12GraphicsPipeline>(pipeline);
    bool hasPushConstants = false;
    if (auto layout = d3dPipeline->GetLayout()) {
        if (auto d3dLayout = std::dynamic_pointer_cast<D3D12PipelineLayout>(layout)) {
            hasPushConstants = !d3dLayout->GetCreateInfo().PushConstantRanges.empty();
        }
    }
    uint32_t rootParamBase = hasPushConstants ? 1 : 0;
    auto d3dDevice = std::dynamic_pointer_cast<D3D12Device>(m_device);

    ID3D12DescriptorHeap* cbvSrvUavHeap = nullptr;
    ID3D12DescriptorHeap* samplerHeap = nullptr;
    for (auto& ds : descriptorSets) {
        auto d3dSet = std::dynamic_pointer_cast<D3D12DescriptorSet>(ds);
        if (!d3dSet) {
            continue;
        }

        if (d3dSet->GetCBVSRVUAVHeap()) {
            AssignDescriptorHeap(cbvSrvUavHeap, d3dSet->GetCBVSRVUAVHeap(), "CBV/SRV/UAV");
        }

        if (d3dSet->HasSamplers() && d3dDevice && d3dSet->PrepareSamplerTable(*d3dDevice)) {
            AssignDescriptorHeap(samplerHeap, d3dSet->GetSamplerHeap(), "sampler");
        }
    }

    std::array<ID3D12DescriptorHeap*, 2> heaps{};
    UINT heapCount = 0;
    if (cbvSrvUavHeap != nullptr) {
        heaps[heapCount++] = cbvSrvUavHeap;
    }
    if (samplerHeap != nullptr) {
        heaps[heapCount++] = samplerHeap;
    }
    if (heapCount > 0) {
        m_commandList->SetDescriptorHeaps(heapCount, heaps.data());
    }

    uint32_t rootIdx = rootParamBase;
    for (size_t i = 0; i < descriptorSets.size(); i++) {
        auto d3dSet = std::dynamic_pointer_cast<D3D12DescriptorSet>(descriptorSets[i]);
        if (!d3dSet) {
            continue;
        }

        if (d3dSet->HasCBVSRVUAV()) {
            m_commandList->SetGraphicsRootDescriptorTable(rootIdx, d3dSet->GetCBVSRVUAVGPUHandle());
            rootIdx++;
        }
        if (d3dSet->HasSamplers()) {
            m_commandList->SetGraphicsRootDescriptorTable(rootIdx, d3dSet->GetSamplerGPUHandle());
            rootIdx++;
        }
    }
}

void D3D12CommandBufferEncoder::PushConstants(
    const Ref<GraphicsPipeline>& pipeline, ShaderStage stageFlags, uint32_t offset, uint32_t size, const void* data)
{
    m_commandList->SetGraphicsRoot32BitConstants(0, size / 4, data, offset / 4);
}

void D3D12CommandBufferEncoder::Draw(uint32_t vertexCount,
                                     uint32_t instanceCount,
                                     uint32_t firstVertex,
                                     uint32_t firstInstance)
{
    m_commandList->DrawInstanced(vertexCount, instanceCount, firstVertex, firstInstance);
}

void D3D12CommandBufferEncoder::DrawIndexed(
    uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t vertexOffset, uint32_t firstInstance)
{
    m_commandList->DrawIndexedInstanced(indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
}

void D3D12CommandBufferEncoder::Dispatch(uint32_t x, uint32_t y, uint32_t z)
{
    m_commandList->Dispatch(x, y, z);
}

void D3D12CommandBufferEncoder::BindComputeDescriptorSets(const Ref<ComputePipeline>& pipeline,
                                                          uint32_t firstSet,
                                                          std::span<const Ref<DescriptorSet>> descriptorSets)
{
    if (descriptorSets.empty()) {
        return;
    }

    auto d3dPipeline = std::dynamic_pointer_cast<D3D12ComputePipeline>(pipeline);
    bool hasPushConstants = false;
    if (auto layout = d3dPipeline->GetLayout()) {
        if (auto d3dLayout = std::dynamic_pointer_cast<D3D12PipelineLayout>(layout)) {
            hasPushConstants = !d3dLayout->GetCreateInfo().PushConstantRanges.empty();
        }
    }
    uint32_t rootParamBase = hasPushConstants ? 1 : 0;
    auto d3dDevice = std::dynamic_pointer_cast<D3D12Device>(m_device);

    ID3D12DescriptorHeap* cbvSrvUavHeap = nullptr;
    ID3D12DescriptorHeap* samplerHeap = nullptr;
    for (auto& ds : descriptorSets) {
        auto* d3dSet = static_cast<D3D12DescriptorSet*>(ds.get());
        if (d3dSet->HasCBVSRVUAV()) {
            AssignDescriptorHeap(cbvSrvUavHeap, d3dSet->GetCBVSRVUAVHeap(), "CBV/SRV/UAV");
        }
        if (d3dSet->HasSamplers() && d3dDevice && d3dSet->PrepareSamplerTable(*d3dDevice)) {
            AssignDescriptorHeap(samplerHeap, d3dSet->GetSamplerHeap(), "sampler");
        }
    }

    std::array<ID3D12DescriptorHeap*, 2> heaps{};
    UINT heapCount = 0;
    if (cbvSrvUavHeap != nullptr) {
        heaps[heapCount++] = cbvSrvUavHeap;
    }
    if (samplerHeap != nullptr) {
        heaps[heapCount++] = samplerHeap;
    }
    if (heapCount > 0) {
        m_commandList->SetDescriptorHeaps(heapCount, heaps.data());
    }

    uint32_t rootIdx = rootParamBase;
    for (auto& ds : descriptorSets) {
        auto* d3dSet = static_cast<D3D12DescriptorSet*>(ds.get());
        if (d3dSet->HasCBVSRVUAV()) {
            m_commandList->SetComputeRootDescriptorTable(rootIdx, d3dSet->GetCBVSRVUAVGPUHandle());
            rootIdx++;
        }
        if (d3dSet->HasSamplers()) {
            m_commandList->SetComputeRootDescriptorTable(rootIdx, d3dSet->GetSamplerGPUHandle());
            rootIdx++;
        }
    }
}

void D3D12CommandBufferEncoder::ComputePushConstants(
    const Ref<ComputePipeline>& pipeline, ShaderStage stageFlags, uint32_t offset, uint32_t size, const void* data)
{
    m_commandList->SetComputeRoot32BitConstants(0, size / 4, data, offset / 4);
}

void D3D12CommandBufferEncoder::ResolveTexture(const Ref<Texture>& srcTexture,
                                               const Ref<Texture>& dstTexture,
                                               const ImageSubresourceLayers& srcSub,
                                               const ImageSubresourceLayers& dstSub)
{
    auto* src = static_cast<D3D12Texture*>(srcTexture.get());
    auto* dst = static_cast<D3D12Texture*>(dstTexture.get());
    UINT srcSubresource = srcSub.MipLevel + srcSub.BaseArrayLayer * srcTexture->GetMipLevels();
    UINT dstSubresource = dstSub.MipLevel + dstSub.BaseArrayLayer * dstTexture->GetMipLevels();
    m_commandList->ResolveSubresource(
        dst->GetHandle(), dstSubresource, src->GetHandle(), srcSubresource, ToDXGIFormat(srcTexture->GetFormat()));
}

void D3D12CommandBufferEncoder::PipelineBarrier(SyncScope srcStage,
                                                SyncScope dstStage,
                                                std::span<const CMemoryBarrier> globalBarriers,
                                                std::span<const BufferBarrier> bufferBarriers,
                                                std::span<const TextureBarrier> textureBarriers)
{
    std::vector<D3D12_RESOURCE_BARRIER> barriers;
    barriers.reserve(textureBarriers.size() + bufferBarriers.size());

    for (auto& tb : textureBarriers) {
        if (!tb.Texture || tb.OldState == tb.NewState) {
            continue;
        }

        auto* d3dTex = static_cast<D3D12Texture*>(tb.Texture.get());
        D3D12_RESOURCE_BARRIER b{};
        b.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        b.Transition.pResource = d3dTex->GetHandle();
        b.Transition.StateBefore = ToD3D12ResourceState(tb.OldState);
        b.Transition.StateAfter = ToD3D12ResourceState(tb.NewState);
        b.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        barriers.push_back(b);
        d3dTex->SetCurrentState(tb.NewState);
    }

    if (!barriers.empty()) {
        m_commandList->ResourceBarrier(static_cast<UINT>(barriers.size()), barriers.data());
    }
}

void D3D12CommandBufferEncoder::TransitionImage(const Ref<Texture>& texture,
                                                ImageTransition transition,
                                                const ImageSubresourceRange& range)
{
    auto* d3dTex = static_cast<D3D12Texture*>(texture.get());
    ResourceState oldState = d3dTex->GetCurrentState();
    ResourceState newState = oldState;

    switch (transition) {
        case ImageTransition::UndefinedToColorAttachment:
        case ImageTransition::ShaderReadToColorAttachment:
        case ImageTransition::PresentToColorAttachment:
        case ImageTransition::TransferDstToColorAttachment:
        case ImageTransition::GeneralToColorAttachment:
            newState = ResourceState::RenderTarget;
            break;
        case ImageTransition::UndefinedToDepthAttachment:
            newState = ResourceState::DepthWrite;
            break;
        case ImageTransition::UndefinedToTransferDst:
        case ImageTransition::ShaderReadToTransferDst:
        case ImageTransition::GeneralToTransferDst:
            newState = ResourceState::CopyDest;
            break;
        case ImageTransition::UndefinedToShaderRead:
        case ImageTransition::ColorAttachmentToShaderRead:
        case ImageTransition::DepthAttachmentToShaderRead:
        case ImageTransition::TransferDstToShaderRead:
        case ImageTransition::TransferSrcToShaderRead:
        case ImageTransition::GeneralToShaderRead:
            newState = ResourceState::ShaderRead;
            break;
        case ImageTransition::ColorAttachmentToPresent:
            newState = ResourceState::Present;
            break;
        case ImageTransition::ColorAttachmentToTransferSrc:
        case ImageTransition::ShaderReadToTransferSrc:
            newState = ResourceState::CopySource;
            break;
        case ImageTransition::UndefinedToGeneral:
        case ImageTransition::ShaderReadToGeneral:
            newState = ResourceState::General;
            break;
        default:
            break;
    }

    if (oldState == newState) {
        return;
    }

    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = d3dTex->GetHandle();
    barrier.Transition.StateBefore = ToD3D12ResourceState(oldState);
    barrier.Transition.StateAfter = ToD3D12ResourceState(newState);
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    m_commandList->ResourceBarrier(1, &barrier);
    d3dTex->SetCurrentState(newState);
}

void D3D12CommandBufferEncoder::TransitionBuffer(const Ref<Buffer>& buffer,
                                                 BufferTransition transition,
                                                 uint64_t offset,
                                                 uint64_t size)
{
    // D3D12 buffer transitions are handled via resource barriers
    // Most buffer states in D3D12 don't require explicit transitions when using upload/readback heaps
}

void D3D12CommandBufferEncoder::ExecuteNative(const std::function<void(void*)>& func)
{
    func(m_commandList.Get());
}

void* D3D12CommandBufferEncoder::GetNativeHandle()
{
    return m_commandList.Get();
}

void D3D12CommandBufferEncoder::CopyTexture2D(const Ref<Texture>& src, const Ref<Texture>& dst)
{
    auto* d3dSrc = static_cast<D3D12Texture*>(src.get());
    auto* d3dDst = static_cast<D3D12Texture*>(dst.get());
    m_commandList->CopyResource(d3dDst->GetHandle(), d3dSrc->GetHandle());
}

void D3D12CommandBufferEncoder::CopyBufferToImage(const Ref<Buffer>& srcBuffer,
                                                  const Ref<Texture>& dstImage,
                                                  ImageLayout dstImageLayout,
                                                  std::span<const BufferImageCopy> regions)
{
    auto* d3dSrc = static_cast<D3D12Buffer*>(srcBuffer.get());
    auto* d3dDst = static_cast<D3D12Texture*>(dstImage.get());

    for (auto& region : regions) {
        D3D12_TEXTURE_COPY_LOCATION dst{};
        dst.pResource = d3dDst->GetHandle();
        dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        dst.SubresourceIndex = CalcSubresource(region.ImageSubresource.MipLevel,
                                               region.ImageSubresource.BaseArrayLayer,
                                               0,
                                               dstImage->GetMipLevels(),
                                               dstImage->GetArrayLayers());

        D3D12_TEXTURE_COPY_LOCATION src{};
        src.pResource = d3dSrc->GetHandle();
        src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        src.PlacedFootprint.Offset = region.BufferOffset;
        src.PlacedFootprint.Footprint.Format = ToDXGIFormat(dstImage->GetFormat());
        src.PlacedFootprint.Footprint.Width = region.ImageExtentWidth;
        src.PlacedFootprint.Footprint.Height = region.ImageExtentHeight;
        src.PlacedFootprint.Footprint.Depth = region.ImageExtentDepth;
        uint32_t bpp = D3D12GetFormatBytesPerPixel(src.PlacedFootprint.Footprint.Format);
        uint32_t naturalPitch = region.ImageExtentWidth * bpp;
        uint32_t rowPitchBytes = region.BufferRowLength > 0 ? (region.BufferRowLength * bpp) : naturalPitch;
        src.PlacedFootprint.Footprint.RowPitch = (rowPitchBytes + 255) & ~255;

        D3D12_BOX box{};
        box.left = 0;
        box.top = 0;
        box.front = 0;
        box.right = region.ImageExtentWidth;
        box.bottom = region.ImageExtentHeight;
        box.back = region.ImageExtentDepth;

        m_commandList->CopyTextureRegion(
            &dst, region.ImageOffsetX, region.ImageOffsetY, region.ImageOffsetZ, &src, &box);
    }
}

void D3D12CommandBufferEncoder::CopyImageToBuffer(const Ref<Texture>& srcImage,
                                                  ImageLayout srcImageLayout,
                                                  const Ref<Buffer>& dstBuffer,
                                                  std::span<const BufferImageCopy> regions)
{
    auto* d3dSrc = static_cast<D3D12Texture*>(srcImage.get());
    auto* d3dDst = static_cast<D3D12Buffer*>(dstBuffer.get());
    if (!d3dSrc || !d3dDst) {
        return;
    }

    for (const auto& region : regions) {
        D3D12_TEXTURE_COPY_LOCATION src{};
        src.pResource = d3dSrc->GetHandle();
        src.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        src.SubresourceIndex = CalcSubresource(region.ImageSubresource.MipLevel,
                                               region.ImageSubresource.BaseArrayLayer,
                                               0,
                                               srcImage->GetMipLevels(),
                                               srcImage->GetArrayLayers());

        D3D12_TEXTURE_COPY_LOCATION dst{};
        dst.pResource = d3dDst->GetHandle();
        dst.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        dst.PlacedFootprint.Offset = region.BufferOffset;
        dst.PlacedFootprint.Footprint.Format = ToDXGIFormat(srcImage->GetFormat());
        dst.PlacedFootprint.Footprint.Width = region.ImageExtentWidth;
        dst.PlacedFootprint.Footprint.Height = region.ImageExtentHeight;
        dst.PlacedFootprint.Footprint.Depth = region.ImageExtentDepth;
        const uint32_t bytes_per_pixel = D3D12GetFormatBytesPerPixel(dst.PlacedFootprint.Footprint.Format);
        const uint32_t natural_row_pitch = region.ImageExtentWidth * bytes_per_pixel;
        const uint32_t requested_row_pitch =
            region.BufferRowLength > 0 ? (region.BufferRowLength * bytes_per_pixel) : natural_row_pitch;
        dst.PlacedFootprint.Footprint.RowPitch = (requested_row_pitch + 255u) & ~255u;

        D3D12_BOX src_box{};
        src_box.left = region.ImageOffsetX;
        src_box.top = region.ImageOffsetY;
        src_box.front = region.ImageOffsetZ;
        src_box.right = region.ImageOffsetX + region.ImageExtentWidth;
        src_box.bottom = region.ImageOffsetY + region.ImageExtentHeight;
        src_box.back = region.ImageOffsetZ + region.ImageExtentDepth;

        m_commandList->CopyTextureRegion(&dst, 0, 0, 0, &src, &src_box);
    }
}

void D3D12CommandBufferEncoder::CopyBuffer(
    const Ref<Buffer>& srcBuffer, const Ref<Buffer>& dstBuffer, uint64_t srcOffset, uint64_t dstOffset, uint64_t size)
{
    auto* src = static_cast<D3D12Buffer*>(srcBuffer.get());
    auto* dst = static_cast<D3D12Buffer*>(dstBuffer.get());
    m_commandList->CopyBufferRegion(dst->GetHandle(), dstOffset, src->GetHandle(), srcOffset, size);
}

void D3D12CommandBufferEncoder::DrawIndirect(const Ref<Buffer>& argBuffer,
                                             uint64_t offset,
                                             uint32_t drawCount,
                                             uint32_t stride)
{
    auto d3dDevice = std::dynamic_pointer_cast<D3D12Device>(m_device);
    auto* d3dBuf = static_cast<D3D12Buffer*>(argBuffer.get());

    D3D12_INDIRECT_ARGUMENT_DESC argDesc{};
    argDesc.Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW;
    D3D12_COMMAND_SIGNATURE_DESC sigDesc{};
    sigDesc.ByteStride = stride > 0 ? stride : sizeof(D3D12_DRAW_ARGUMENTS);
    sigDesc.NumArgumentDescs = 1;
    sigDesc.pArgumentDescs = &argDesc;

    ComPtr<ID3D12CommandSignature> cmdSig;
    d3dDevice->GetHandle()->CreateCommandSignature(&sigDesc, nullptr, IID_PPV_ARGS(&cmdSig));
    if (cmdSig) {
        m_commandList->ExecuteIndirect(cmdSig.Get(), drawCount, d3dBuf->GetHandle(), offset, nullptr, 0);
    }
}

void D3D12CommandBufferEncoder::DrawIndexedIndirect(const Ref<Buffer>& argBuffer,
                                                    uint64_t offset,
                                                    uint32_t drawCount,
                                                    uint32_t stride)
{
    auto d3dDevice = std::dynamic_pointer_cast<D3D12Device>(m_device);
    auto* d3dBuf = static_cast<D3D12Buffer*>(argBuffer.get());

    D3D12_INDIRECT_ARGUMENT_DESC argDesc{};
    argDesc.Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW_INDEXED;
    D3D12_COMMAND_SIGNATURE_DESC sigDesc{};
    sigDesc.ByteStride = stride > 0 ? stride : sizeof(D3D12_DRAW_INDEXED_ARGUMENTS);
    sigDesc.NumArgumentDescs = 1;
    sigDesc.pArgumentDescs = &argDesc;

    ComPtr<ID3D12CommandSignature> cmdSig;
    d3dDevice->GetHandle()->CreateCommandSignature(&sigDesc, nullptr, IID_PPV_ARGS(&cmdSig));
    if (cmdSig) {
        m_commandList->ExecuteIndirect(cmdSig.Get(), drawCount, d3dBuf->GetHandle(), offset, nullptr, 0);
    }
}

void D3D12CommandBufferEncoder::DispatchIndirect(const Ref<Buffer>& argBuffer, uint64_t offset)
{
    auto d3dDevice = std::dynamic_pointer_cast<D3D12Device>(m_device);
    auto* d3dBuf = static_cast<D3D12Buffer*>(argBuffer.get());

    D3D12_INDIRECT_ARGUMENT_DESC argDesc{};
    argDesc.Type = D3D12_INDIRECT_ARGUMENT_TYPE_DISPATCH;
    D3D12_COMMAND_SIGNATURE_DESC sigDesc{};
    sigDesc.ByteStride = sizeof(D3D12_DISPATCH_ARGUMENTS);
    sigDesc.NumArgumentDescs = 1;
    sigDesc.pArgumentDescs = &argDesc;

    ComPtr<ID3D12CommandSignature> cmdSig;
    d3dDevice->GetHandle()->CreateCommandSignature(&sigDesc, nullptr, IID_PPV_ARGS(&cmdSig));
    if (cmdSig) {
        m_commandList->ExecuteIndirect(cmdSig.Get(), 1, d3dBuf->GetHandle(), offset, nullptr, 0);
    }
}

void D3D12CommandBufferEncoder::DrawIndirectCount(const Ref<Buffer>& argBuffer,
                                                  uint64_t offset,
                                                  const Ref<Buffer>& countBuffer,
                                                  uint64_t countOffset,
                                                  uint32_t maxDrawCount,
                                                  uint32_t stride)
{
    auto d3dDevice = std::dynamic_pointer_cast<D3D12Device>(m_device);
    auto* d3dArg = static_cast<D3D12Buffer*>(argBuffer.get());
    auto* d3dCount = static_cast<D3D12Buffer*>(countBuffer.get());

    D3D12_INDIRECT_ARGUMENT_DESC argDesc{};
    argDesc.Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW;
    D3D12_COMMAND_SIGNATURE_DESC sigDesc{};
    sigDesc.ByteStride = stride > 0 ? stride : sizeof(D3D12_DRAW_ARGUMENTS);
    sigDesc.NumArgumentDescs = 1;
    sigDesc.pArgumentDescs = &argDesc;

    ComPtr<ID3D12CommandSignature> cmdSig;
    d3dDevice->GetHandle()->CreateCommandSignature(&sigDesc, nullptr, IID_PPV_ARGS(&cmdSig));
    if (cmdSig) {
        m_commandList->ExecuteIndirect(
            cmdSig.Get(), maxDrawCount, d3dArg->GetHandle(), offset, d3dCount->GetHandle(), countOffset);
    }
}

void D3D12CommandBufferEncoder::DrawIndexedIndirectCount(const Ref<Buffer>& argBuffer,
                                                         uint64_t offset,
                                                         const Ref<Buffer>& countBuffer,
                                                         uint64_t countOffset,
                                                         uint32_t maxDrawCount,
                                                         uint32_t stride)
{
    auto d3dDevice = std::dynamic_pointer_cast<D3D12Device>(m_device);
    auto* d3dArg = static_cast<D3D12Buffer*>(argBuffer.get());
    auto* d3dCount = static_cast<D3D12Buffer*>(countBuffer.get());

    D3D12_INDIRECT_ARGUMENT_DESC argDesc{};
    argDesc.Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW_INDEXED;
    D3D12_COMMAND_SIGNATURE_DESC sigDesc{};
    sigDesc.ByteStride = stride > 0 ? stride : sizeof(D3D12_DRAW_INDEXED_ARGUMENTS);
    sigDesc.NumArgumentDescs = 1;
    sigDesc.pArgumentDescs = &argDesc;

    ComPtr<ID3D12CommandSignature> cmdSig;
    d3dDevice->GetHandle()->CreateCommandSignature(&sigDesc, nullptr, IID_PPV_ARGS(&cmdSig));
    if (cmdSig) {
        m_commandList->ExecuteIndirect(
            cmdSig.Get(), maxDrawCount, d3dArg->GetHandle(), offset, d3dCount->GetHandle(), countOffset);
    }
}

void D3D12CommandBufferEncoder::DispatchMesh(uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ)
{
    ComPtr<ID3D12GraphicsCommandList6> cmdList6;
    if (SUCCEEDED(m_commandList->QueryInterface(IID_PPV_ARGS(&cmdList6)))) {
        cmdList6->DispatchMesh(groupCountX, groupCountY, groupCountZ);
    }
}

void D3D12CommandBufferEncoder::BeginDebugLabel(const std::string& name, float r, float g, float b, float a)
{
#ifdef USE_PIX
    PIXBeginEvent(m_commandList.Get(),
                  PIX_COLOR(static_cast<BYTE>(r * 255), static_cast<BYTE>(g * 255), static_cast<BYTE>(b * 255)),
                  name.c_str());
#endif
}

void D3D12CommandBufferEncoder::EndDebugLabel()
{
#ifdef USE_PIX
    PIXEndEvent(m_commandList.Get());
#endif
}

void D3D12CommandBufferEncoder::InsertDebugLabel(const std::string& name, float r, float g, float b, float a)
{
#ifdef USE_PIX
    PIXSetMarker(m_commandList.Get(),
                 PIX_COLOR(static_cast<BYTE>(r * 255), static_cast<BYTE>(g * 255), static_cast<BYTE>(b * 255)),
                 name.c_str());
#endif
}

void D3D12CommandBufferEncoder::BeginQuery(const Ref<QueryPool>& pool, uint32_t queryIndex)
{
    auto* d3dPool = static_cast<D3D12QueryPool*>(pool.get());
    D3D12_QUERY_TYPE qtype = D3D12_QUERY_TYPE_OCCLUSION;
    if (pool->GetType() == QueryType::Timestamp) {
        qtype = D3D12_QUERY_TYPE_TIMESTAMP;
    } else if (pool->GetType() == QueryType::PipelineStatistics) {
        qtype = D3D12_QUERY_TYPE_PIPELINE_STATISTICS;
    }
    m_commandList->BeginQuery(d3dPool->GetHeap(), qtype, queryIndex);
}

void D3D12CommandBufferEncoder::EndQuery(const Ref<QueryPool>& pool, uint32_t queryIndex)
{
    auto* d3dPool = static_cast<D3D12QueryPool*>(pool.get());
    D3D12_QUERY_TYPE qtype = D3D12_QUERY_TYPE_OCCLUSION;
    if (pool->GetType() == QueryType::Timestamp) {
        qtype = D3D12_QUERY_TYPE_TIMESTAMP;
    } else if (pool->GetType() == QueryType::PipelineStatistics) {
        qtype = D3D12_QUERY_TYPE_PIPELINE_STATISTICS;
    }
    m_commandList->EndQuery(d3dPool->GetHeap(), qtype, queryIndex);
}

void D3D12CommandBufferEncoder::WriteTimestamp(const Ref<QueryPool>& pool, uint32_t queryIndex)
{
    auto* d3dPool = static_cast<D3D12QueryPool*>(pool.get());
    m_commandList->EndQuery(d3dPool->GetHeap(), D3D12_QUERY_TYPE_TIMESTAMP, queryIndex);
}

void D3D12CommandBufferEncoder::ResetQueryPool(const Ref<QueryPool>& pool, uint32_t first, uint32_t count)
{
    // D3D12: query pools don't need explicit reset
}

void D3D12CommandBufferEncoder::ResolveQueryPool(const Ref<QueryPool>& pool, uint32_t first, uint32_t count)
{
    auto* d3dPool = static_cast<D3D12QueryPool*>(pool.get());
    if (d3dPool == nullptr || count == 0) {
        return;
    }

    D3D12_QUERY_TYPE qtype = D3D12_QUERY_TYPE_OCCLUSION;
    if (pool->GetType() == QueryType::Timestamp) {
        qtype = D3D12_QUERY_TYPE_TIMESTAMP;
    } else if (pool->GetType() == QueryType::PipelineStatistics) {
        qtype = D3D12_QUERY_TYPE_PIPELINE_STATISTICS;
    }

    m_commandList->ResolveQueryData(
        d3dPool->GetHeap(), qtype, first, count, d3dPool->GetReadbackBuffer(), first * sizeof(uint64_t));
}

void D3D12CommandBufferEncoder::TraceRays(const Ref<ShaderBindingTable>& sbt, uint32_t w, uint32_t h, uint32_t d)
{
    auto* d3dSBT = static_cast<D3D12ShaderBindingTable*>(sbt.get());
    auto desc = d3dSBT->GetDispatchRaysDesc(w, h, d);

    ComPtr<ID3D12GraphicsCommandList4> cmdList4;
    m_commandList->QueryInterface(IID_PPV_ARGS(&cmdList4));
    if (cmdList4) {
        cmdList4->DispatchRays(&desc);
    }
}

void D3D12CommandBufferEncoder::BindRayTracingPipeline(const Ref<RayTracingPipeline>& pipeline)
{
    auto* d3dPipeline = static_cast<D3D12RayTracingPipeline*>(pipeline.get());
    ComPtr<ID3D12GraphicsCommandList4> cmdList4;
    m_commandList->QueryInterface(IID_PPV_ARGS(&cmdList4));
    if (cmdList4) {
        cmdList4->SetPipelineState1(d3dPipeline->GetStateObject());
    }

    auto* layout = static_cast<D3D12PipelineLayout*>(pipeline->GetLayout().get());
    m_commandList->SetComputeRootSignature(layout->GetHandle());
}

void D3D12CommandBufferEncoder::BindRayTracingDescriptorSets(const Ref<RayTracingPipeline>& pipeline,
                                                             uint32_t firstSet,
                                                             std::span<const Ref<DescriptorSet>> descriptorSets)
{
    auto d3dDevice = std::dynamic_pointer_cast<D3D12Device>(m_device);
    ID3D12DescriptorHeap* cbvSrvUavHeap = nullptr;
    ID3D12DescriptorHeap* samplerHeap = nullptr;
    for (auto& ds : descriptorSets) {
        auto* d3dSet = static_cast<D3D12DescriptorSet*>(ds.get());
        if (d3dSet->GetCBVSRVUAVHeap()) {
            AssignDescriptorHeap(cbvSrvUavHeap, d3dSet->GetCBVSRVUAVHeap(), "CBV/SRV/UAV");
        }
        if (d3dSet->HasSamplers() && d3dDevice && d3dSet->PrepareSamplerTable(*d3dDevice)) {
            AssignDescriptorHeap(samplerHeap, d3dSet->GetSamplerHeap(), "sampler");
        }
    }

    std::array<ID3D12DescriptorHeap*, 2> heaps{};
    UINT heapCount = 0;
    if (cbvSrvUavHeap != nullptr) {
        heaps[heapCount++] = cbvSrvUavHeap;
    }
    if (samplerHeap != nullptr) {
        heaps[heapCount++] = samplerHeap;
    }
    if (heapCount > 0) {
        m_commandList->SetDescriptorHeaps(heapCount, heaps.data());
    }

    uint32_t rootIdx = firstSet;
    for (auto& ds : descriptorSets) {
        auto* d3dSet = static_cast<D3D12DescriptorSet*>(ds.get());
        if (d3dSet->HasCBVSRVUAV()) {
            m_commandList->SetComputeRootDescriptorTable(rootIdx, d3dSet->GetCBVSRVUAVGPUHandle());
            rootIdx++;
        }
        if (d3dSet->HasSamplers()) {
            m_commandList->SetComputeRootDescriptorTable(rootIdx, d3dSet->GetSamplerGPUHandle());
            rootIdx++;
        }
    }
}

void D3D12CommandBufferEncoder::MemoryBarrierFast(MemoryTransition transition)
{
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
    barrier.UAV.pResource = nullptr;
    m_commandList->ResourceBarrier(1, &barrier);
}

void D3D12CommandBufferEncoder::BuildAccelerationStructure(const Ref<AccelerationStructure>& as)
{
    auto* d3dAS = static_cast<D3D12AccelerationStructure*>(as.get());
    auto desc = d3dAS->GetBuildDesc();

    ComPtr<ID3D12GraphicsCommandList4> cmdList4;
    m_commandList->QueryInterface(IID_PPV_ARGS(&cmdList4));
    if (cmdList4) {
        cmdList4->BuildRaytracingAccelerationStructure(&desc, 0, nullptr);
    }
}
} // namespace luna::RHI

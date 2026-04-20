#include "DescriptorSetLayout.h"
#include "Impls/D3D12/D3D12AccelerationStructure.h"
#include "Impls/D3D12/D3D12Buffer.h"
#include "Impls/D3D12/D3D12DescriptorSet.h"
#include "Impls/D3D12/D3D12Device.h"
#include "Impls/D3D12/D3D12Sampler.h"
#include "Impls/D3D12/D3D12Texture.h"

namespace luna::RHI {
D3D12DescriptorSet::D3D12DescriptorSet(const Ref<Device>& device,
                                       D3D12_GPU_DESCRIPTOR_HANDLE cbvGpu,
                                       D3D12_CPU_DESCRIPTOR_HANDLE cbvCpu,
                                       uint32_t cbvCount,
                                       uint32_t cbvDescSize,
                                       uint32_t sampCount,
                                       std::vector<SlotInfo> bindingMap,
                                       ID3D12DescriptorHeap* cbvHeap)
    : m_device(device),
      m_cbvSrvUavGpu(cbvGpu),
      m_cbvSrvUavCpu(cbvCpu),
      m_cbvSrvUavCount(cbvCount),
      m_cbvSrvUavDescSize(cbvDescSize),
      m_samplerCount(sampCount),
      m_samplerDescriptors(sampCount),
      m_bindingMap(std::move(bindingMap)),
      m_cbvSrvUavHeap(cbvHeap)
{}

const D3D12DescriptorSet::SlotInfo* D3D12DescriptorSet::FindSlot(uint32_t binding) const
{
    for (auto& s : m_bindingMap) {
        if (s.binding == binding) {
            return &s;
        }
    }
    return nullptr;
}

void D3D12DescriptorSet::WriteBuffer(const BufferWriteInfo& info)
{
    auto slot = FindSlot(info.Binding);
    if (!slot || slot->isSampler) {
        return;
    }

    auto d3dDevice = std::dynamic_pointer_cast<D3D12Device>(m_device);
    auto d3dBuffer = std::dynamic_pointer_cast<D3D12Buffer>(info.Buffer);
    if (!d3dDevice || !d3dBuffer) {
        return;
    }

    uint64_t resHash = reinterpret_cast<uint64_t>(d3dBuffer->GetHandle()) ^ info.Offset ^ info.Size;
    if (m_bindingHashes[info.Binding] == resHash) {
        return;
    }
    m_bindingHashes[info.Binding] = resHash;
    m_dirty = true;

    D3D12_CPU_DESCRIPTOR_HANDLE dest = m_cbvSrvUavCpu;
    dest.ptr += slot->slot * m_cbvSrvUavDescSize;

    uint64_t stride = info.Stride > 0 ? info.Stride : 1;
    uint64_t bufSize = (info.Size == UINT64_MAX) ? d3dBuffer->GetSize() : info.Size;
    uint64_t firstElement = info.Offset / stride;
    uint64_t numElements = bufSize / stride;

    if (info.Type == DescriptorType::StorageBuffer || info.Type == DescriptorType::StorageBufferDynamic) {
        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.Format = DXGI_FORMAT_UNKNOWN;
        srvDesc.Buffer.FirstElement = firstElement;
        srvDesc.Buffer.NumElements = static_cast<UINT>(numElements);
        srvDesc.Buffer.StructureByteStride = static_cast<UINT>(stride);
        d3dDevice->GetHandle()->CreateShaderResourceView(d3dBuffer->GetHandle(), &srvDesc, dest);
    } else if (info.Type == DescriptorType::UniformBuffer || info.Type == DescriptorType::UniformBufferDynamic) {
        const uint64_t resourceSize = d3dBuffer->GetHandle()->GetDesc().Width;
        const uint64_t remainingSize = resourceSize > info.Offset ? resourceSize - info.Offset : 0;
        const uint64_t viewSize = (std::min)(bufSize, remainingSize);
        if (viewSize == 0) {
            return;
        }

        D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
        cbvDesc.BufferLocation = d3dBuffer->GetHandle()->GetGPUVirtualAddress() + info.Offset;
        cbvDesc.SizeInBytes = static_cast<UINT>((viewSize + 255) & ~255);
        d3dDevice->GetHandle()->CreateConstantBufferView(&cbvDesc, dest);
    }
}

void D3D12DescriptorSet::WriteTexture(const TextureWriteInfo& info)
{
    auto slot = FindSlot(info.Binding);
    if (!slot || slot->isSampler) {
        return;
    }

    auto d3dDevice = std::dynamic_pointer_cast<D3D12Device>(m_device);
    auto d3dView = std::dynamic_pointer_cast<D3D12TextureView>(info.TextureView);
    if (!d3dDevice || !d3dView) {
        return;
    }

    D3D12_CPU_DESCRIPTOR_HANDLE srcHandle = {};
    if (info.Type == DescriptorType::StorageImage && d3dView->HasUAV()) {
        srcHandle = d3dView->GetUAVHandle();
    } else if (d3dView->HasSRV()) {
        srcHandle = d3dView->GetSRVHandle();
    } else {
        return;
    }

    uint64_t resHash = srcHandle.ptr;
    if (m_bindingHashes[info.Binding] == resHash) {
        return;
    }
    m_bindingHashes[info.Binding] = resHash;
    m_dirty = true;

    D3D12_CPU_DESCRIPTOR_HANDLE dest = m_cbvSrvUavCpu;
    dest.ptr += slot->slot * m_cbvSrvUavDescSize;

    d3dDevice->GetHandle()->CopyDescriptorsSimple(1, dest, srcHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
}

void D3D12DescriptorSet::WriteSampler(const SamplerWriteInfo& info)
{
    auto slot = FindSlot(info.Binding);
    if (!slot || !slot->isSampler) {
        return;
    }

    auto d3dDevice = std::dynamic_pointer_cast<D3D12Device>(m_device);
    auto d3dSampler = std::dynamic_pointer_cast<D3D12Sampler>(info.Sampler);
    if (!d3dDevice || !d3dSampler) {
        return;
    }

    uint64_t resHash = d3dSampler->GetCPUHandle().ptr;
    if (m_bindingHashes[info.Binding] == resHash) {
        return;
    }
    m_bindingHashes[info.Binding] = resHash;
    m_dirty = true;

    if (slot->slot >= m_samplerDescriptors.size()) {
        return;
    }

    m_samplerDescriptors[slot->slot] = d3dSampler->GetCPUHandle();
    ++m_samplerVersion;
    m_stagedSamplerFrameIndex = (std::numeric_limits<uint32_t>::max)();
    m_stagedSamplerVersion = 0;
}

void D3D12DescriptorSet::WriteAccelerationStructure(const AccelerationStructureWriteInfo& info)
{
    auto slot = FindSlot(info.Binding);
    if (!slot || slot->isSampler) {
        return;
    }

    auto d3dDevice = std::dynamic_pointer_cast<D3D12Device>(m_device);
    if (!d3dDevice) {
        return;
    }

    auto* d3dAS = static_cast<const D3D12AccelerationStructure*>(info.AccelerationStructureHandle);
    if (!d3dAS) {
        return;
    }

    m_dirty = true;

    D3D12_CPU_DESCRIPTOR_HANDLE dest = m_cbvSrvUavCpu;
    dest.ptr += slot->slot * m_cbvSrvUavDescSize;

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = DXGI_FORMAT_UNKNOWN;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_RAYTRACING_ACCELERATION_STRUCTURE;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.RaytracingAccelerationStructure.Location = d3dAS->GetResultBuffer()->GetGPUVirtualAddress();

    d3dDevice->GetHandle()->CreateShaderResourceView(nullptr, &srvDesc, dest);
}

bool D3D12DescriptorSet::PrepareSamplerTable(D3D12Device& device)
{
    if (m_samplerCount == 0) {
        return false;
    }

    const uint32_t currentFrameIndex = device.GetCurrentDeferredDescriptorFrameIndex();
    if (m_stagedSamplerFrameIndex == currentFrameIndex && m_stagedSamplerVersion == m_samplerVersion &&
        m_samplerHeap != nullptr && m_samplerGpu.ptr != 0) {
        return true;
    }

    const auto allocation = device.AllocateTransientSamplerTable(m_samplerCount);
    if (!allocation.IsValid()) {
        return false;
    }

    D3D12_CPU_DESCRIPTOR_HANDLE destination = allocation.cpu;
    const uint32_t descriptorSize = device.GetSamplerDescriptorSize();
    for (uint32_t i = 0; i < m_samplerCount; ++i) {
        const auto source = m_samplerDescriptors[i];
        if (source.ptr != 0) {
            device.GetHandle()->CopyDescriptorsSimple(1, destination, source, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
        }
        destination.ptr += descriptorSize;
    }

    m_samplerCpu = allocation.cpu;
    m_samplerGpu = allocation.gpu;
    m_samplerHeap = allocation.heap;
    m_stagedSamplerFrameIndex = currentFrameIndex;
    m_stagedSamplerVersion = m_samplerVersion;
    return true;
}
} // namespace luna::RHI

#include "DescriptorSetLayout.h"
#include "Impls/D3D12/D3D12AccelerationStructure.h"
#include "Impls/D3D12/D3D12Buffer.h"
#include "Impls/D3D12/D3D12DescriptorSet.h"
#include "Impls/D3D12/D3D12Device.h"
#include "Impls/D3D12/D3D12Sampler.h"
#include "Impls/D3D12/D3D12Texture.h"

namespace Cacao {
D3D12DescriptorSet::D3D12DescriptorSet(const Ref<Device>& device,
                                       D3D12_GPU_DESCRIPTOR_HANDLE cbvGpu,
                                       D3D12_CPU_DESCRIPTOR_HANDLE cbvCpu,
                                       uint32_t cbvCount,
                                       uint32_t cbvDescSize,
                                       D3D12_GPU_DESCRIPTOR_HANDLE sampGpu,
                                       D3D12_CPU_DESCRIPTOR_HANDLE sampCpu,
                                       uint32_t sampCount,
                                       uint32_t sampDescSize,
                                       std::vector<SlotInfo> bindingMap,
                                       ID3D12DescriptorHeap* cbvHeap,
                                       ID3D12DescriptorHeap* sampHeap)
    : m_device(device),
      m_cbvSrvUavGpu(cbvGpu),
      m_cbvSrvUavCpu(cbvCpu),
      m_cbvSrvUavCount(cbvCount),
      m_cbvSrvUavDescSize(cbvDescSize),
      m_samplerGpu(sampGpu),
      m_samplerCpu(sampCpu),
      m_samplerCount(sampCount),
      m_samplerDescSize(sampDescSize),
      m_bindingMap(std::move(bindingMap)),
      m_cbvSrvUavHeap(cbvHeap),
      m_samplerHeap(sampHeap)
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
        D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
        cbvDesc.BufferLocation = d3dBuffer->GetHandle()->GetGPUVirtualAddress() + info.Offset;
        cbvDesc.SizeInBytes = static_cast<UINT>((bufSize + 255) & ~255);
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

    D3D12_CPU_DESCRIPTOR_HANDLE dest = m_samplerCpu;
    dest.ptr += slot->slot * m_samplerDescSize;

    d3dDevice->GetHandle()->CopyDescriptorsSimple(
        1, dest, d3dSampler->GetCPUHandle(), D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
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
} // namespace Cacao

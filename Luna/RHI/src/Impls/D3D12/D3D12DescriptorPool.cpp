#include "Impls/D3D12/D3D12DescriptorPool.h"
#include "Impls/D3D12/D3D12DescriptorSet.h"
#include "Impls/D3D12/D3D12DescriptorSetLayout.h"
#include "Impls/D3D12/D3D12Device.h"

#include <stdexcept>

namespace luna::RHI {
D3D12DescriptorPool::D3D12DescriptorPool(const Ref<Device>& device, const DescriptorPoolCreateInfo& info)
    : m_device(device)
{
    auto d3dDevice = std::dynamic_pointer_cast<D3D12Device>(device);

    m_cbvSrvUavDescriptorSize =
        d3dDevice->GetHandle()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    m_samplerDescriptorSize =
        d3dDevice->GetHandle()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);

    for (const auto& poolSize : info.PoolSizes) {
        switch (poolSize.Type) {
            case DescriptorType::Sampler:
                m_samplerCapacity += poolSize.Count;
                break;
            case DescriptorType::CombinedImageSampler:
                m_cbvSrvUavCapacity += poolSize.Count;
                m_samplerCapacity += poolSize.Count;
                break;
            default:
                m_cbvSrvUavCapacity += poolSize.Count;
                break;
        }
    }

    if (m_cbvSrvUavCapacity == 0) {
        m_cbvSrvUavCapacity = 1;
    }

    D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
    heapDesc.NumDescriptors = m_cbvSrvUavCapacity;
    heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    HRESULT hr = d3dDevice->GetHandle()->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&m_cbvSrvUavHeap));
    if (FAILED(hr) || !m_cbvSrvUavHeap) {
        throw std::runtime_error("Failed to create D3D12 CBV/SRV/UAV descriptor heap for descriptor pool");
    }

    if (m_samplerCapacity > 0) {
        heapDesc.NumDescriptors = m_samplerCapacity;
        heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
        heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        hr = d3dDevice->GetHandle()->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&m_samplerHeap));
        if (FAILED(hr) || !m_samplerHeap) {
            throw std::runtime_error("Failed to create D3D12 sampler descriptor heap for descriptor pool");
        }
    }
}

D3D12_CPU_DESCRIPTOR_HANDLE D3D12DescriptorPool::AllocateCBVSRVUAV()
{
    auto handle = m_cbvSrvUavHeap->GetCPUDescriptorHandleForHeapStart();
    handle.ptr += m_cbvSrvUavOffset * m_cbvSrvUavDescriptorSize;
    m_cbvSrvUavOffset++;
    return handle;
}

D3D12_CPU_DESCRIPTOR_HANDLE D3D12DescriptorPool::AllocateSampler()
{
    auto handle = m_samplerHeap->GetCPUDescriptorHandleForHeapStart();
    handle.ptr += m_samplerOffset * m_samplerDescriptorSize;
    m_samplerOffset++;
    return handle;
}

D3D12_GPU_DESCRIPTOR_HANDLE D3D12DescriptorPool::GetGPUHandleForCBVSRVUAV(uint32_t offset) const
{
    auto handle = m_cbvSrvUavHeap->GetGPUDescriptorHandleForHeapStart();
    handle.ptr += offset * m_cbvSrvUavDescriptorSize;
    return handle;
}

D3D12_GPU_DESCRIPTOR_HANDLE D3D12DescriptorPool::GetGPUHandleForSampler(uint32_t offset) const
{
    auto handle = m_samplerHeap->GetGPUDescriptorHandleForHeapStart();
    handle.ptr += offset * m_samplerDescriptorSize;
    return handle;
}

Ref<DescriptorSet> D3D12DescriptorPool::AllocateDescriptorSet(const Ref<DescriptorSetLayout>& layout)
{
    auto d3dLayout = std::dynamic_pointer_cast<D3D12DescriptorSetLayout>(layout);
    if (!d3dLayout) {
        throw std::runtime_error("D3D12DescriptorPool::AllocateDescriptorSet received a non-D3D12 layout");
    }
    const auto& bindings = d3dLayout->GetBindings();

    uint32_t cbvSrvUavCount = 0;
    uint32_t samplerCount = 0;
    std::vector<D3D12DescriptorSet::SlotInfo> bindingMap;

    for (const auto& b : bindings) {
        if (b.Type == DescriptorType::Sampler) {
            bindingMap.push_back({b.Binding, true, samplerCount});
            samplerCount += b.Count;
        } else {
            bindingMap.push_back({b.Binding, false, cbvSrvUavCount});
            cbvSrvUavCount += b.Count;
        }
    }
    if (cbvSrvUavCount == 0) {
        cbvSrvUavCount = 1;
    }
    if (m_cbvSrvUavOffset + cbvSrvUavCount > m_cbvSrvUavCapacity || !m_cbvSrvUavHeap) {
        throw std::runtime_error("D3D12 descriptor pool exhausted CBV/SRV/UAV heap capacity");
    }

    uint32_t cbvHeapOffset = m_cbvSrvUavOffset;
    auto cbvGpu = GetGPUHandleForCBVSRVUAV(cbvHeapOffset);
    auto cbvCpu = m_cbvSrvUavHeap->GetCPUDescriptorHandleForHeapStart();
    cbvCpu.ptr += cbvHeapOffset * m_cbvSrvUavDescriptorSize;
    m_cbvSrvUavOffset += cbvSrvUavCount;

    D3D12_GPU_DESCRIPTOR_HANDLE sampGpu = {};
    D3D12_CPU_DESCRIPTOR_HANDLE sampCpu = {};
    uint32_t sampHeapOffset = m_samplerOffset;
    if (samplerCount > 0) {
        if (m_samplerOffset + samplerCount > m_samplerCapacity || !m_samplerHeap) {
            throw std::runtime_error("D3D12 descriptor pool exhausted sampler heap capacity");
        }
        sampGpu = GetGPUHandleForSampler(sampHeapOffset);
        sampCpu = m_samplerHeap->GetCPUDescriptorHandleForHeapStart();
        sampCpu.ptr += sampHeapOffset * m_samplerDescriptorSize;
        m_samplerOffset += samplerCount;
    }

    return std::make_shared<D3D12DescriptorSet>(m_device,
                                                cbvGpu,
                                                cbvCpu,
                                                cbvSrvUavCount,
                                                m_cbvSrvUavDescriptorSize,
                                                sampGpu,
                                                sampCpu,
                                                samplerCount,
                                                m_samplerDescriptorSize,
                                                std::move(bindingMap),
                                                m_cbvSrvUavHeap.Get(),
                                                m_samplerHeap.Get());
}

void D3D12DescriptorPool::Reset()
{
    m_cbvSrvUavOffset = 0;
    m_samplerOffset = 0;
}
} // namespace luna::RHI

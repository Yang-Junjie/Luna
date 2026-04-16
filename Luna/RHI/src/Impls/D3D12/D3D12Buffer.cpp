#include "Impls/D3D12/D3D12Buffer.h"
#include "Impls/D3D12/D3D12Device.h"

namespace Cacao {
D3D12Buffer::D3D12Buffer(const Ref<Device>& device, const BufferCreateInfo& info)
    : m_device(device),
      m_createInfo(info)
{
    auto d3dDevice = std::dynamic_pointer_cast<D3D12Device>(device);

    D3D12_RESOURCE_DESC desc = {};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    desc.Width = info.Size;
    desc.Height = 1;
    desc.DepthOrArraySize = 1;
    desc.MipLevels = 1;
    desc.Format = DXGI_FORMAT_UNKNOWN;
    desc.SampleDesc.Count = 1;
    desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    desc.Flags = D3D12_RESOURCE_FLAG_NONE;

    D3D12MA::ALLOCATION_DESC allocDesc = {};
    allocDesc.HeapType = ToD3D12HeapType(info.MemoryUsage);

    D3D12_RESOURCE_STATES initialState = D3D12_RESOURCE_STATE_COMMON;
    if (info.MemoryUsage == BufferMemoryUsage::CpuToGpu) {
        initialState = D3D12_RESOURCE_STATE_GENERIC_READ;
    } else if (info.MemoryUsage == BufferMemoryUsage::GpuToCpu) {
        initialState = D3D12_RESOURCE_STATE_COPY_DEST;
    }

    d3dDevice->GetAllocator()->CreateResource(
        &allocDesc, &desc, initialState, nullptr, &m_allocation, IID_PPV_ARGS(&m_resource));
}

D3D12Buffer::~D3D12Buffer()
{
    if (m_mappedData) {
        Unmap();
    }
    m_resource.Reset();
    if (m_allocation) {
        m_allocation->Release();
        m_allocation = nullptr;
    }
}

Ref<D3D12Buffer> D3D12Buffer::Create(const Ref<Device>& device, const BufferCreateInfo& info)
{
    return std::make_shared<D3D12Buffer>(device, info);
}

uint64_t D3D12Buffer::GetSize() const
{
    return m_createInfo.Size;
}

BufferUsageFlags D3D12Buffer::GetUsage() const
{
    return m_createInfo.Usage;
}

BufferMemoryUsage D3D12Buffer::GetMemoryUsage() const
{
    return m_createInfo.MemoryUsage;
}

void* D3D12Buffer::Map()
{
    if (!m_mappedData) {
        D3D12_RANGE readRange = {0, 0};
        m_resource->Map(0, &readRange, &m_mappedData);
    }
    return m_mappedData;
}

void D3D12Buffer::Unmap()
{
    if (m_mappedData) {
        m_resource->Unmap(0, nullptr);
        m_mappedData = nullptr;
    }
}

void D3D12Buffer::Flush(uint64_t offset, uint64_t size)
{
    // D3D12 upload heaps are coherent, no explicit flush needed
}

uint64_t D3D12Buffer::GetDeviceAddress() const
{
    return m_resource ? m_resource->GetGPUVirtualAddress() : 0;
}
} // namespace Cacao

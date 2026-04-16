#include "Impls/D3D12/D3D12Device.h"
#include "Impls/D3D12/D3D12StagingBuffer.h"

namespace Cacao {
D3D12StagingBufferPool::D3D12StagingBufferPool(const Ref<Device>& device,
                                               uint64_t blockSize,
                                               uint32_t maxFramesInFlight)
    : m_device(device),
      m_blockSize(blockSize)
{
    auto d3dDevice = std::dynamic_pointer_cast<D3D12Device>(device);

    D3D12_RESOURCE_DESC desc = {};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    desc.Width = blockSize;
    desc.Height = 1;
    desc.DepthOrArraySize = 1;
    desc.MipLevels = 1;
    desc.Format = DXGI_FORMAT_UNKNOWN;
    desc.SampleDesc.Count = 1;
    desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    D3D12MA::ALLOCATION_DESC allocDesc = {};
    allocDesc.HeapType = D3D12_HEAP_TYPE_UPLOAD;

    d3dDevice->GetAllocator()->CreateResource(
        &allocDesc, &desc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, &m_allocation, IID_PPV_ARGS(&m_uploadBuffer));

    D3D12_RANGE readRange = {0, 0};
    m_uploadBuffer->Map(0, &readRange, &m_mappedData);
}

D3D12StagingBufferPool::~D3D12StagingBufferPool()
{
    if (m_mappedData) {
        m_uploadBuffer->Unmap(0, nullptr);
    }
    m_uploadBuffer.Reset();
    if (m_allocation) {
        m_allocation->Release();
        m_allocation = nullptr;
    }
}

StagingAllocation D3D12StagingBufferPool::Allocate(uint64_t size, uint64_t alignment)
{
    uint64_t alignedOffset = (m_offset + alignment - 1) & ~(alignment - 1);
    if (alignedOffset + size > m_blockSize) {
        return {};
    }

    StagingAllocation alloc;
    alloc.mappedPtr = static_cast<uint8_t*>(m_mappedData) + alignedOffset;
    alloc.offset = alignedOffset;
    alloc.size = size;
    m_offset = alignedOffset + size;
    m_totalAllocated += size;
    return alloc;
}

void D3D12StagingBufferPool::Reset()
{
    m_offset = 0;
    m_totalAllocated = 0;
}

void D3D12StagingBufferPool::AdvanceFrame()
{
    Reset();
}
} // namespace Cacao

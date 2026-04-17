#include "Impls/D3D11/D3D11Device.h"
#include "Impls/D3D11/D3D11StagingBuffer.h"

namespace luna::RHI {
D3D11StagingBufferPool::D3D11StagingBufferPool(const Ref<D3D11Device>& device, uint64_t initialSize)
    : m_device(device),
      m_totalSize(initialSize)
{}

StagingAllocation D3D11StagingBufferPool::Allocate(uint64_t size, uint64_t alignment)
{
    for (auto& entry : m_pool) {
        if (!entry.inUse && entry.size >= size) {
            entry.inUse = true;
            D3D11_MAPPED_SUBRESOURCE mapped;
            m_device->GetImmediateContext()->Map(entry.buffer.Get(), 0, D3D11_MAP_WRITE, 0, &mapped);
            m_device->GetImmediateContext()->Unmap(entry.buffer.Get(), 0);
            m_usedSize += size;
            return {nullptr, 0, mapped.pData, size};
        }
    }

    D3D11_BUFFER_DESC desc = {};
    desc.ByteWidth = static_cast<UINT>(size);
    desc.Usage = D3D11_USAGE_STAGING;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE | D3D11_CPU_ACCESS_READ;

    PoolEntry entry;
    entry.size = size;
    entry.inUse = true;
    m_device->GetNativeDevice()->CreateBuffer(&desc, nullptr, &entry.buffer);

    D3D11_MAPPED_SUBRESOURCE mapped;
    m_device->GetImmediateContext()->Map(entry.buffer.Get(), 0, D3D11_MAP_WRITE, 0, &mapped);
    m_device->GetImmediateContext()->Unmap(entry.buffer.Get(), 0);

    m_totalSize += size;
    m_usedSize += size;
    m_pool.push_back(std::move(entry));
    return {nullptr, 0, mapped.pData, size};
}

void D3D11StagingBufferPool::Reset()
{
    for (auto& entry : m_pool) {
        entry.inUse = false;
    }
    m_usedSize = 0;
}
} // namespace luna::RHI

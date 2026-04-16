#include "Impls/WebGPU/WGPUBuffer.h"

#include <cstring>

namespace Cacao {
WGPUBufferImpl::WGPUBufferImpl(::WGPUDevice device, const BufferCreateInfo& info)
    : m_wgpuDevice(device),
      m_createInfo(info)
{
    WGPUBufferDescriptor desc = {};
    desc.size = info.Size;
    desc.usage = ToWGPUBufferUsage(info.Usage, info.MemoryUsage);
    desc.mappedAtCreation =
        (info.MemoryUsage == BufferMemoryUsage::CpuToGpu || info.MemoryUsage == BufferMemoryUsage::CpuOnly);
    if (!info.Name.empty()) {
        desc.label = info.Name.c_str();
    }

    m_buffer = wgpuDeviceCreateBuffer(device, &desc);

    if (desc.mappedAtCreation) {
        m_mappedPtr = wgpuBufferGetMappedRange(m_buffer, 0, info.Size);
        m_persistentlyMapped = true;

        if (info.InitialData && m_mappedPtr) {
            memcpy(m_mappedPtr, info.InitialData, info.Size);
            wgpuBufferUnmap(m_buffer);
            m_mappedPtr = nullptr;
            m_persistentlyMapped = false;
        }
    }
}

WGPUBufferImpl::~WGPUBufferImpl()
{
    if (m_mappedPtr) {
        wgpuBufferUnmap(m_buffer);
        m_mappedPtr = nullptr;
    }
    if (m_buffer) {
        wgpuBufferRelease(m_buffer);
        m_buffer = nullptr;
    }
}

uint64_t WGPUBufferImpl::GetSize() const
{
    return m_createInfo.Size;
}

BufferUsageFlags WGPUBufferImpl::GetUsage() const
{
    return m_createInfo.Usage;
}

BufferMemoryUsage WGPUBufferImpl::GetMemoryUsage() const
{
    return m_createInfo.MemoryUsage;
}

void* WGPUBufferImpl::Map()
{
    if (m_mappedPtr) {
        return m_mappedPtr;
    }

    // For CpuToGpu buffers that were unmapped, we need to re-map synchronously.
    // Dawn supports mappedAtCreation but async map for subsequent maps.
    // We create a new mapped-at-creation buffer and copy on Unmap.
    // Simpler approach: use the already-mapped pointer if available.
    // For robust implementation, we do synchronous map via wgpuBufferMapAsync + spin.

    // Dawn's mappedAtCreation gives us the pointer at creation. After Unmap, we can't
    // easily re-map a MapWrite buffer synchronously. The pattern used in Cacao (Map/Write/Unmap
    // at init, then never again) means this path is rarely hit.
    // For now, return nullptr for re-map attempts on already-unmapped buffers.
    return nullptr;
}

void WGPUBufferImpl::Unmap()
{
    if (m_mappedPtr) {
        wgpuBufferUnmap(m_buffer);
        m_mappedPtr = nullptr;
        m_persistentlyMapped = false;
    }
}

void WGPUBufferImpl::Flush(uint64_t offset, uint64_t size)
{
    // WebGPU guarantees coherency on Unmap; no explicit flush needed.
    // If buffer is still mapped, unmap will flush.
}

uint64_t WGPUBufferImpl::GetDeviceAddress() const
{
    return 0; // WebGPU does not expose buffer device addresses
}
} // namespace Cacao

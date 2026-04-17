#include "Builders.h"
#include "Impls/D3D12/D3D12Buffer.h"
#include "Impls/D3D12/D3D12Device.h"
#include "Impls/D3D12/D3D12ShaderBindingTable.h"

namespace luna::RHI {
static constexpr uint32_t Align(uint32_t size, uint32_t alignment)
{
    return (size + alignment - 1) & ~(alignment - 1);
}

D3D12ShaderBindingTable::D3D12ShaderBindingTable(const Ref<Device>& device,
                                                 ID3D12StateObject* rtPSO,
                                                 uint32_t rayGenCount,
                                                 uint32_t missCount,
                                                 uint32_t hitGroupCount,
                                                 uint32_t callableCount)
{
    ComPtr<ID3D12StateObjectProperties> props;
    rtPSO->QueryInterface(IID_PPV_ARGS(&props));

    m_entrySize = Align(D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES, D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT);
    uint32_t totalEntries = rayGenCount + missCount + hitGroupCount + callableCount;

    m_rayGenOffset = 0;
    m_missOffset = Align(rayGenCount * m_entrySize, D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT);
    m_hitGroupOffset = Align(static_cast<uint32_t>(m_missOffset) + missCount * m_entrySize,
                             D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT);
    m_callableOffset = Align(static_cast<uint32_t>(m_hitGroupOffset) + hitGroupCount * m_entrySize,
                             D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT);

    uint64_t totalSize = m_callableOffset + callableCount * m_entrySize;
    if (totalSize == 0) {
        totalSize = m_hitGroupOffset + hitGroupCount * m_entrySize;
    }

    m_buffer = device->CreateBuffer(BufferBuilder()
                                        .SetSize(totalSize)
                                        .SetUsage(BufferUsageFlags::StorageBuffer)
                                        .SetMemoryUsage(BufferMemoryUsage::CpuToGpu)
                                        .Build());

    uint8_t* data = static_cast<uint8_t*>(m_buffer->Map());
    memset(data, 0, totalSize);

    auto copyIdentifier = [&](uint64_t offset, const wchar_t* name) {
        void* id = props->GetShaderIdentifier(name);
        if (id) {
            memcpy(data + offset, id, D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);
        }
    };

    copyIdentifier(m_rayGenOffset, L"RayGen");
    if (missCount > 0) {
        copyIdentifier(m_missOffset, L"Miss");
    }
    if (hitGroupCount > 0) {
        copyIdentifier(m_hitGroupOffset, L"HitGroup");
    }

    m_buffer->Unmap();
    m_buffer->Flush();
}

D3D12_GPU_VIRTUAL_ADDRESS D3D12ShaderBindingTable::GetGPUAddress() const
{
    auto* d3dBuf = static_cast<D3D12Buffer*>(m_buffer.get());
    return d3dBuf->GetHandle()->GetGPUVirtualAddress();
}

D3D12_DISPATCH_RAYS_DESC
    D3D12ShaderBindingTable::GetDispatchRaysDesc(uint32_t width, uint32_t height, uint32_t depth) const
{
    auto base = GetGPUAddress();
    D3D12_DISPATCH_RAYS_DESC desc = {};
    desc.RayGenerationShaderRecord.StartAddress = base + m_rayGenOffset;
    desc.RayGenerationShaderRecord.SizeInBytes = m_entrySize;
    desc.MissShaderTable.StartAddress = base + m_missOffset;
    desc.MissShaderTable.SizeInBytes = m_entrySize;
    desc.MissShaderTable.StrideInBytes = m_entrySize;
    desc.HitGroupTable.StartAddress = base + m_hitGroupOffset;
    desc.HitGroupTable.SizeInBytes = m_entrySize;
    desc.HitGroupTable.StrideInBytes = m_entrySize;
    desc.Width = width;
    desc.Height = height;
    desc.Depth = depth;
    return desc;
}
} // namespace luna::RHI

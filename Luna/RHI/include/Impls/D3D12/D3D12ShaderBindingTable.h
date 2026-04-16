#ifndef CACAO_D3D12SBT_H
#define CACAO_D3D12SBT_H
#include "D3D12Common.h"
#include "RayTracing.h"

namespace Cacao {
class CACAO_API D3D12ShaderBindingTable final : public ShaderBindingTable {
public:
    D3D12ShaderBindingTable(const Ref<Device>& device,
                            ID3D12StateObject* rtPSO,
                            uint32_t rayGenCount,
                            uint32_t missCount,
                            uint32_t hitGroupCount,
                            uint32_t callableCount);

    Ref<Buffer> GetBuffer() const override
    {
        return m_buffer;
    }

    uint64_t GetRayGenOffset() const override
    {
        return m_rayGenOffset;
    }

    uint64_t GetMissOffset() const override
    {
        return m_missOffset;
    }

    uint64_t GetHitGroupOffset() const override
    {
        return m_hitGroupOffset;
    }

    uint64_t GetCallableOffset() const override
    {
        return m_callableOffset;
    }

    uint32_t GetEntrySize() const override
    {
        return m_entrySize;
    }

    D3D12_GPU_VIRTUAL_ADDRESS GetGPUAddress() const;
    D3D12_DISPATCH_RAYS_DESC GetDispatchRaysDesc(uint32_t width, uint32_t height, uint32_t depth) const;

private:
    Ref<Buffer> m_buffer;
    uint32_t m_entrySize = 0;
    uint64_t m_rayGenOffset = 0;
    uint64_t m_missOffset = 0;
    uint64_t m_hitGroupOffset = 0;
    uint64_t m_callableOffset = 0;
};
} // namespace Cacao

#endif

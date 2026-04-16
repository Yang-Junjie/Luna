#ifndef CACAO_VKSBT_H
#define CACAO_VKSBT_H
#include "Device.h"
#include "RayTracing.h"

#include <vulkan/vulkan.hpp>

namespace Cacao {
class VKDevice;

class CACAO_API VKShaderBindingTable final : public ShaderBindingTable {
public:
    VKShaderBindingTable(const Ref<Device>& device,
                         VkPipeline rtPipeline,
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

    VkStridedDeviceAddressRegionKHR GetRaygenRegion() const;
    VkStridedDeviceAddressRegionKHR GetMissRegion() const;
    VkStridedDeviceAddressRegionKHR GetHitRegion() const;

private:
    uint32_t m_missCount = 0;
    uint32_t m_hitGroupCount = 0;
    Ref<Buffer> m_buffer;
    uint32_t m_entrySize = 0;
    uint64_t m_rayGenOffset = 0;
    uint64_t m_missOffset = 0;
    uint64_t m_hitGroupOffset = 0;
    uint64_t m_callableOffset = 0;
};
} // namespace Cacao

#endif

#include "Impls/Vulkan/VKShaderBindingTable.h"
#include "Impls/Vulkan/VKDevice.h"
#include "Impls/Vulkan/VKBuffer.h"
#include "Impls/Vulkan/VKAdapter.h"
#include "Builders.h"
#include <cstring>

namespace Cacao
{
    static uint32_t AlignUp(uint32_t val, uint32_t alignment) {
        return (val + alignment - 1) & ~(alignment - 1);
    }

    VKShaderBindingTable::VKShaderBindingTable(
        const Ref<Device>& device, VkPipeline rtPipeline,
        uint32_t rayGenCount, uint32_t missCount,
        uint32_t hitGroupCount, uint32_t callableCount)
    {
        auto vkDevice = std::dynamic_pointer_cast<VKDevice>(device);
        auto& dev = vkDevice->GetHandle();

        VkPhysicalDeviceRayTracingPipelinePropertiesKHR rtProps = {};
        rtProps.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR;
        VkPhysicalDeviceProperties2 props2 = {};
        props2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
        props2.pNext = &rtProps;

        auto adapter = device->GetParentAdapter();
        auto vkAdapter = std::dynamic_pointer_cast<class VKAdapter>(adapter);
        // Get RT properties would require physical device - for now use common defaults
        uint32_t handleSize = 32; // VK default shaderGroupHandleSize
        uint32_t handleAlignment = 64; // shaderGroupHandleAlignment
        uint32_t baseAlignment = 64; // shaderGroupBaseAlignment

        m_entrySize = AlignUp(handleSize, handleAlignment);
        uint32_t totalGroups = rayGenCount + missCount + hitGroupCount + callableCount;

        std::vector<uint8_t> handles(totalGroups * handleSize);
        auto vkGetHandles = reinterpret_cast<PFN_vkGetRayTracingShaderGroupHandlesKHR>(
            dev.getProcAddr("vkGetRayTracingShaderGroupHandlesKHR"));
        if (vkGetHandles)
            vkGetHandles(static_cast<VkDevice>(dev), rtPipeline, 0, totalGroups, handles.size(), handles.data());

        m_rayGenOffset = 0;
        m_missOffset = AlignUp(rayGenCount * m_entrySize, baseAlignment);
        m_hitGroupOffset = AlignUp(static_cast<uint32_t>(m_missOffset) + missCount * m_entrySize, baseAlignment);
        m_callableOffset = AlignUp(static_cast<uint32_t>(m_hitGroupOffset) + hitGroupCount * m_entrySize, baseAlignment);

        uint64_t totalSize = m_callableOffset + callableCount * m_entrySize;
        if (totalSize == 0) totalSize = m_hitGroupOffset + hitGroupCount * m_entrySize;

        m_buffer = device->CreateBuffer(
            BufferBuilder()
            .SetSize(totalSize)
            .SetUsage(BufferUsageFlags::StorageBuffer | BufferUsageFlags::TransferSrc)
            .SetMemoryUsage(BufferMemoryUsage::CpuToGpu)
            .Build());

        uint8_t* data = static_cast<uint8_t*>(m_buffer->Map());
        memset(data, 0, totalSize);

        uint32_t groupIdx = 0;
        for (uint32_t i = 0; i < rayGenCount; i++)
            memcpy(data + m_rayGenOffset + i * m_entrySize, handles.data() + (groupIdx++) * handleSize, handleSize);
        for (uint32_t i = 0; i < missCount; i++)
            memcpy(data + m_missOffset + i * m_entrySize, handles.data() + (groupIdx++) * handleSize, handleSize);
        for (uint32_t i = 0; i < hitGroupCount; i++)
            memcpy(data + m_hitGroupOffset + i * m_entrySize, handles.data() + (groupIdx++) * handleSize, handleSize);

        m_buffer->Unmap();
        m_buffer->Flush();

        m_missCount = missCount;
        m_hitGroupCount = hitGroupCount;
    }

    VkStridedDeviceAddressRegionKHR VKShaderBindingTable::GetRaygenRegion() const
    {
        VkStridedDeviceAddressRegionKHR region = {};
        region.deviceAddress = m_buffer->GetDeviceAddress() + m_rayGenOffset;
        region.stride = m_entrySize;
        region.size = m_entrySize;
        return region;
    }

    VkStridedDeviceAddressRegionKHR VKShaderBindingTable::GetMissRegion() const
    {
        VkStridedDeviceAddressRegionKHR region = {};
        region.deviceAddress = m_buffer->GetDeviceAddress() + m_missOffset;
        region.stride = m_entrySize;
        region.size = m_missCount * m_entrySize;
        return region;
    }

    VkStridedDeviceAddressRegionKHR VKShaderBindingTable::GetHitRegion() const
    {
        VkStridedDeviceAddressRegionKHR region = {};
        region.deviceAddress = m_buffer->GetDeviceAddress() + m_hitGroupOffset;
        region.stride = m_entrySize;
        region.size = m_hitGroupCount * m_entrySize;
        return region;
    }
}

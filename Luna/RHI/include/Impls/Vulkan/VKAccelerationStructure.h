#ifndef LUNA_RHI_VKACCELERATIONSTRUCTURE_H
#define LUNA_RHI_VKACCELERATIONSTRUCTURE_H
#include "Device.h"
#include "RayTracing.h"
#include "vk_mem_alloc.h"

#include <vulkan/vulkan.hpp>

namespace luna::RHI {
class VKDevice;

class LUNA_RHI_API VKAccelerationStructure final : public AccelerationStructure {
public:
    VKAccelerationStructure(const Ref<Device>& device, const AccelerationStructureCreateInfo& info);
    ~VKAccelerationStructure() override;

    AccelerationStructureType GetType() const override
    {
        return m_type;
    }

    uint64_t GetDeviceAddress() const override;

    uint64_t GetScratchSize() const override
    {
        return m_scratchSize;
    }

    VkAccelerationStructureKHR GetHandle() const
    {
        return m_accelStructure;
    }

    VkBuffer GetResultBuffer() const
    {
        return m_resultBuffer;
    }

    VkAccelerationStructureBuildGeometryInfoKHR GetBuildInfo() const;

    const std::vector<VkAccelerationStructureBuildRangeInfoKHR>& GetRangeInfos() const
    {
        return m_ranges;
    }

private:
    std::vector<VkAccelerationStructureGeometryKHR> m_geometries;
    std::vector<VkAccelerationStructureBuildRangeInfoKHR> m_ranges;
    VkBuildAccelerationStructureFlagsKHR m_buildFlags = 0;
    AccelerationStructureType m_type;
    VkAccelerationStructureKHR m_accelStructure = VK_NULL_HANDLE;
    VkBuffer m_resultBuffer = VK_NULL_HANDLE;
    VmaAllocation m_resultAllocation = VK_NULL_HANDLE;
    VkBuffer m_scratchBuffer = VK_NULL_HANDLE;
    VmaAllocation m_scratchAllocation = VK_NULL_HANDLE;
    VkBuffer m_instanceBuffer = VK_NULL_HANDLE;
    VmaAllocation m_instanceAllocation = VK_NULL_HANDLE;
    uint64_t m_scratchSize = 0;
    Ref<Device> m_device;
};
} // namespace luna::RHI

#endif

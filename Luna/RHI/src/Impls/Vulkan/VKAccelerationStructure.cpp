#include "Impls/Vulkan/VKAccelerationStructure.h"
#include "Impls/Vulkan/VKBuffer.h"
#include "Impls/Vulkan/VKDevice.h"

namespace Cacao {
VKAccelerationStructure::VKAccelerationStructure(const Ref<Device>& device, const AccelerationStructureCreateInfo& info)
    : m_type(info.Type),
      m_device(device)
{
    auto vkDevice = std::dynamic_pointer_cast<VKDevice>(device);
    auto& dev = vkDevice->GetHandle();
    auto allocator = vkDevice->GetVmaAllocator();

    auto asType = (info.Type == AccelerationStructureType::BottomLevel)
                      ? VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR
                      : VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;

    std::vector<VkAccelerationStructureGeometryKHR> geometries;
    std::vector<VkAccelerationStructureBuildRangeInfoKHR> ranges;
    std::vector<uint32_t> maxPrimCounts;

    if (info.Type == AccelerationStructureType::BottomLevel) {
        for (auto& g : info.Geometries) {
            VkAccelerationStructureGeometryKHR geom = {};
            geom.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
            geom.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
            geom.flags = g.Opaque ? VK_GEOMETRY_OPAQUE_BIT_KHR : 0;

            auto& tri = geom.geometry.triangles;
            tri.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
            tri.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
            tri.vertexStride = g.VertexStride;
            tri.maxVertex = g.VertexCount - 1;
            tri.vertexData.deviceAddress = g.VertexBuffer->GetDeviceAddress() + g.VertexOffset;

            if (g.IndexBuffer) {
                tri.indexType = (g.IndexFormat == IndexType::UInt16) ? VK_INDEX_TYPE_UINT16 : VK_INDEX_TYPE_UINT32;
                tri.indexData.deviceAddress = g.IndexBuffer->GetDeviceAddress() + g.IndexOffset;
            } else {
                tri.indexType = VK_INDEX_TYPE_NONE_KHR;
            }

            if (g.TransformBuffer) {
                tri.transformData.deviceAddress = g.TransformBuffer->GetDeviceAddress() + g.TransformOffset;
            }

            geometries.push_back(geom);

            VkAccelerationStructureBuildRangeInfoKHR range = {};
            range.primitiveCount = g.IndexCount > 0 ? g.IndexCount / 3 : g.VertexCount / 3;
            ranges.push_back(range);
            maxPrimCounts.push_back(range.primitiveCount);
        }
    } else {
        VkAccelerationStructureGeometryKHR geom = {};
        geom.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
        geom.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
        geom.geometry.instances.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
        geom.geometry.instances.arrayOfPointers = VK_FALSE;
        geometries.push_back(geom);

        VkAccelerationStructureBuildRangeInfoKHR range = {};
        range.primitiveCount = static_cast<uint32_t>(info.Instances.size());
        ranges.push_back(range);
        maxPrimCounts.push_back(range.primitiveCount);
    }

    VkAccelerationStructureBuildGeometryInfoKHR buildInfo = {};
    buildInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
    buildInfo.type = asType;
    buildInfo.flags = info.PreferFastTrace ? VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR
                                           : VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_BUILD_BIT_KHR;
    if (info.AllowUpdate) {
        buildInfo.flags |= VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR;
    }
    buildInfo.geometryCount = static_cast<uint32_t>(geometries.size());
    buildInfo.pGeometries = geometries.data();

    VkAccelerationStructureBuildSizesInfoKHR sizeInfo = {};
    sizeInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;

    auto vkGetASBuildSizes = reinterpret_cast<PFN_vkGetAccelerationStructureBuildSizesKHR>(
        dev.getProcAddr("vkGetAccelerationStructureBuildSizesKHR"));
    if (vkGetASBuildSizes) {
        vkGetASBuildSizes(static_cast<VkDevice>(dev),
                          VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
                          &buildInfo,
                          maxPrimCounts.data(),
                          &sizeInfo);
    }

    m_scratchSize = sizeInfo.buildScratchSize;

    VkBufferCreateInfo bufCI = {};
    bufCI.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufCI.usage = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
    bufCI.size = sizeInfo.accelerationStructureSize;

    VmaAllocationCreateInfo allocCI = {};
    allocCI.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    vmaCreateBuffer(allocator, &bufCI, &allocCI, &m_resultBuffer, &m_resultAllocation, nullptr);

    bufCI.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
    bufCI.size = m_scratchSize;
    vmaCreateBuffer(allocator, &bufCI, &allocCI, &m_scratchBuffer, &m_scratchAllocation, nullptr);

    VkAccelerationStructureCreateInfoKHR asCI = {};
    asCI.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
    asCI.buffer = m_resultBuffer;
    asCI.size = sizeInfo.accelerationStructureSize;
    asCI.type = asType;

    auto vkCreateAS =
        reinterpret_cast<PFN_vkCreateAccelerationStructureKHR>(dev.getProcAddr("vkCreateAccelerationStructureKHR"));
    if (vkCreateAS) {
        vkCreateAS(static_cast<VkDevice>(dev), &asCI, nullptr, &m_accelStructure);
    }

    m_geometries = geometries;
    m_ranges = ranges;
    m_buildFlags = buildInfo.flags;

    if (info.Type == AccelerationStructureType::TopLevel && !info.Instances.empty()) {
        uint64_t instSize = info.Instances.size() * sizeof(VkAccelerationStructureInstanceKHR);
        VkBufferCreateInfo instBufCI = {};
        instBufCI.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        instBufCI.size = instSize;
        instBufCI.usage = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR |
                          VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
        VmaAllocationCreateInfo instAllocCI = {};
        instAllocCI.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
        vmaCreateBuffer(allocator, &instBufCI, &instAllocCI, &m_instanceBuffer, &m_instanceAllocation, nullptr);

        VkAccelerationStructureInstanceKHR* dst = nullptr;
        vmaMapMemory(allocator, m_instanceAllocation, reinterpret_cast<void**>(&dst));
        for (size_t i = 0; i < info.Instances.size(); i++) {
            auto& src = info.Instances[i];
            memcpy(&dst[i].transform, src.Transform, sizeof(VkTransformMatrixKHR));
            dst[i].instanceCustomIndex = src.InstanceID & 0xFF'FF'FF;
            dst[i].mask = static_cast<uint8_t>(src.Mask);
            dst[i].instanceShaderBindingTableRecordOffset = src.ShaderBindingTableOffset & 0xFF'FF'FF;
            dst[i].flags = static_cast<VkGeometryInstanceFlagsKHR>(src.Flags);
            dst[i].accelerationStructureReference = src.AccelerationStructureAddress;
        }
        vmaUnmapMemory(allocator, m_instanceAllocation);

        VkBufferDeviceAddressInfo instAddrInfo = {};
        instAddrInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
        instAddrInfo.buffer = m_instanceBuffer;
        m_geometries[0].geometry.instances.data.deviceAddress =
            vkGetBufferDeviceAddress(static_cast<VkDevice>(dev), &instAddrInfo);
    }
}

VKAccelerationStructure::~VKAccelerationStructure()
{
    auto vkDevice = std::dynamic_pointer_cast<VKDevice>(m_device);
    if (!vkDevice) {
        return;
    }
    auto& dev = vkDevice->GetHandle();
    auto allocator = vkDevice->GetVmaAllocator();

    if (m_accelStructure) {
        auto vkDestroyAS = reinterpret_cast<PFN_vkDestroyAccelerationStructureKHR>(
            dev.getProcAddr("vkDestroyAccelerationStructureKHR"));
        if (vkDestroyAS) {
            vkDestroyAS(static_cast<VkDevice>(dev), m_accelStructure, nullptr);
        }
    }
    if (m_resultBuffer) {
        vmaDestroyBuffer(allocator, m_resultBuffer, m_resultAllocation);
    }
    if (m_scratchBuffer) {
        vmaDestroyBuffer(allocator, m_scratchBuffer, m_scratchAllocation);
    }
    if (m_instanceBuffer) {
        vmaDestroyBuffer(allocator, m_instanceBuffer, m_instanceAllocation);
    }
}

uint64_t VKAccelerationStructure::GetDeviceAddress() const
{
    auto vkDevice = std::dynamic_pointer_cast<VKDevice>(m_device);
    if (!vkDevice || !m_accelStructure) {
        return 0;
    }

    VkAccelerationStructureDeviceAddressInfoKHR addrInfo = {};
    addrInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
    addrInfo.accelerationStructure = m_accelStructure;

    auto vkGetASAddr = reinterpret_cast<PFN_vkGetAccelerationStructureDeviceAddressKHR>(
        vkDevice->GetHandle().getProcAddr("vkGetAccelerationStructureDeviceAddressKHR"));
    return vkGetASAddr ? vkGetASAddr(static_cast<VkDevice>(vkDevice->GetHandle()), &addrInfo) : 0;
}

VkAccelerationStructureBuildGeometryInfoKHR VKAccelerationStructure::GetBuildInfo() const
{
    VkAccelerationStructureBuildGeometryInfoKHR info = {};
    info.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
    info.type = (m_type == AccelerationStructureType::BottomLevel) ? VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR
                                                                   : VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
    info.flags = m_buildFlags;
    info.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
    info.dstAccelerationStructure = m_accelStructure;
    info.geometryCount = static_cast<uint32_t>(m_geometries.size());
    info.pGeometries = m_geometries.data();

    VkBufferDeviceAddressInfo scratchAddrInfo = {};
    scratchAddrInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
    scratchAddrInfo.buffer = m_scratchBuffer;
    auto vkDevice = std::dynamic_pointer_cast<VKDevice>(m_device);
    if (vkDevice) {
        info.scratchData.deviceAddress =
            vkGetBufferDeviceAddress(static_cast<VkDevice>(vkDevice->GetHandle()), &scratchAddrInfo);
    }

    return info;
}

} // namespace Cacao

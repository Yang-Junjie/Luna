#include "Impls/WebGPU/WGPUAdapter.h"
#include "Impls/WebGPU/WGPUDevice.h"
#include "Impls/WebGPU/WGPUInstance.h"

#include <iostream>

namespace Cacao {
WGPUAdapter::WGPUAdapter(Ref<WGPUInstance> instance, ::WGPUAdapter adapter)
    : m_instance(instance),
      m_adapter(adapter)
{}

WGPUAdapter::~WGPUAdapter()
{
    if (m_adapter) {
        wgpuAdapterRelease(m_adapter);
    }
}

AdapterProperties WGPUAdapter::GetProperties() const
{
    WGPUAdapterProperties props = {};
    wgpuAdapterGetProperties(m_adapter, &props);

    AdapterProperties result;
    result.deviceID = props.deviceID;
    result.vendorID = props.vendorID;
    result.name = props.name ? props.name : "WebGPU Device";
    result.dedicatedVideoMemory = 0;

    switch (props.adapterType) {
        case WGPUAdapterType_DiscreteGPU:
            result.type = AdapterType::Discrete;
            break;
        case WGPUAdapterType_IntegratedGPU:
            result.type = AdapterType::Integrated;
            break;
        case WGPUAdapterType_CPU:
            result.type = AdapterType::Software;
            break;
        default:
            result.type = AdapterType::Unknown;
            break;
    }
    return result;
}

AdapterType WGPUAdapter::GetAdapterType() const
{
    return GetProperties().type;
}

bool WGPUAdapter::IsFeatureSupported(DeviceFeature feature) const
{
    // WebGPU feature support is limited compared to Vulkan/DX12
    switch (feature) {
        case DeviceFeature::SamplerAnisotropy:
        case DeviceFeature::TextureCompressionBC:
            return true;
        default:
            return false;
    }
}

DeviceLimits WGPUAdapter::QueryLimits() const
{
    WGPUSupportedLimits limits = {};
    wgpuAdapterGetLimits(m_adapter, &limits);

    DeviceLimits result;
    result.maxTextureSize2D = limits.limits.maxTextureDimension2D;
    result.maxTextureSize3D = limits.limits.maxTextureDimension3D;
    result.maxComputeWorkGroupSizeX = limits.limits.maxComputeWorkgroupSizeX;
    result.maxComputeWorkGroupSizeY = limits.limits.maxComputeWorkgroupSizeY;
    result.maxComputeWorkGroupSizeZ = limits.limits.maxComputeWorkgroupSizeZ;
    result.maxComputeSharedMemorySize = limits.limits.maxComputeWorkgroupStorageSize;
    result.maxBoundDescriptorSets = limits.limits.maxBindGroups;
    result.maxUniformBufferSize = static_cast<uint32_t>(limits.limits.maxUniformBufferBindingSize);
    result.maxStorageBufferSize = static_cast<uint32_t>(limits.limits.maxStorageBufferBindingSize);
    result.maxSamplerAnisotropy = 16;
    result.maxBufferSize = limits.limits.maxBufferSize;
    return result;
}

Ref<Device> WGPUAdapter::CreateDevice(const DeviceCreateInfo& info)
{
    auto self = std::dynamic_pointer_cast<Adapter>(shared_from_this());
    return std::make_shared<WGPUDevice>(self, info);
}

uint32_t WGPUAdapter::FindQueueFamilyIndex(QueueType type) const
{
    return 0;
}
} // namespace Cacao

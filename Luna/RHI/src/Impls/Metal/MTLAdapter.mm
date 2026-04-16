#ifdef __APPLE__

#include "Impls/Metal/MTLAdapter.h"
#include "Impls/Metal/MTLDevice.h"
#include "Impls/Metal/MTLInstance.h"
#import <Metal/Metal.h>

namespace Cacao
{
    MTLAdapter::MTLAdapter(Ref<MTLInstance> instance, id<MTLDevice> device)
        : m_instance(instance), m_device(device) {}

    AdapterProperties MTLAdapter::GetProperties() const
    {
        AdapterProperties props;
        props.deviceID = 0;
        props.vendorID = 0;
        props.name = [[m_device name] UTF8String];
        props.dedicatedVideoMemory = [m_device recommendedMaxWorkingSetSize];
        props.type = [m_device isLowPower] ? AdapterType::Integrated : AdapterType::Discrete;
        return props;
    }

    AdapterType MTLAdapter::GetAdapterType() const
    {
        return [m_device isLowPower] ? AdapterType::Integrated : AdapterType::Discrete;
    }

    bool MTLAdapter::IsFeatureSupported(DeviceFeature feature) const
    {
        switch (feature)
        {
        case DeviceFeature::SamplerAnisotropy:
        case DeviceFeature::TextureCompressionBC:
        case DeviceFeature::IndependentBlending:
            return true;
        case DeviceFeature::RayTracingPipeline:
        case DeviceFeature::AccelerationStructure:
            if (@available(macOS 11.0, *))
                return [m_device supportsRaytracing];
            return false;
        default:
            return false;
        }
    }

    DeviceLimits MTLAdapter::QueryLimits() const
    {
        DeviceLimits limits;
        limits.maxTextureSize2D = 16384;
        limits.maxTextureSize3D = 2048;
        limits.maxComputeWorkGroupSizeX = 1024;
        limits.maxComputeWorkGroupSizeY = 1024;
        limits.maxComputeWorkGroupSizeZ = 1024;
        limits.maxBufferSize = [m_device maxBufferLength];
        limits.maxBoundDescriptorSets = 30;
        limits.maxSamplerAnisotropy = 16;
        return limits;
    }

    Ref<Device> MTLAdapter::CreateDevice(const DeviceCreateInfo& info)
    {
        auto self = std::dynamic_pointer_cast<Adapter>(shared_from_this());
        return std::make_shared<MTLDevice>(self, info);
    }

    uint32_t MTLAdapter::FindQueueFamilyIndex(QueueType type) const { return 0; }
}

#endif // __APPLE__

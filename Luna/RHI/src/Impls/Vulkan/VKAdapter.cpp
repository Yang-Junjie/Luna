#include "Impls/Vulkan/VKDevice.h"
#include "Impls/Vulkan/VKInstance.h"
#include "Instance.h"

#include <Impls/Vulkan/VKAdapter.h>

namespace Cacao {
uint32_t VKAdapter::GetTotalGPUMemory() const
{
    vk::PhysicalDeviceMemoryProperties memoryProperties = m_physicalDevice.getMemoryProperties();
    uint32_t totalMemory = 0;
    for (uint32_t i = 0; i < memoryProperties.memoryHeapCount; i++) {
        if (memoryProperties.memoryHeaps[i].flags & vk::MemoryHeapFlagBits::eDeviceLocal) {
            totalMemory += static_cast<uint32_t>(memoryProperties.memoryHeaps[i].size / (1'024 * 1'024));
        }
    }
    return totalMemory;
}

vk::PhysicalDevice VKAdapter::GetPhysicalDevice() const
{
    return m_physicalDevice;
}

VKAdapter::VKAdapter(const Ref<Instance>& inst, const vk::PhysicalDevice& physicalDevice)
    : m_physicalDevice(physicalDevice)
{
    if (!m_physicalDevice) {
        throw std::runtime_error("Invalid physical device provided to Adapter");
    }
    m_instance = std::dynamic_pointer_cast<VKInstance>(inst);
    vk::PhysicalDeviceProperties m_physicalDeviceProperties = m_physicalDevice.getProperties();
    m_properties.name = m_physicalDeviceProperties.deviceName.data();
    m_properties.vendorID = m_physicalDeviceProperties.vendorID;
    m_properties.deviceID = m_physicalDeviceProperties.deviceID;
    uint32_t totalMemoryMB = GetTotalGPUMemory();
    m_properties.dedicatedVideoMemory = static_cast<uint64_t>(totalMemoryMB) * 1'024 * 1'024;
    switch (m_physicalDeviceProperties.deviceType) {
        case vk::PhysicalDeviceType::eDiscreteGpu:
            m_adapterType = AdapterType::Discrete;
            break;
        case vk::PhysicalDeviceType::eIntegratedGpu:
            m_adapterType = AdapterType::Integrated;
            break;
        case vk::PhysicalDeviceType::eCpu:
            m_adapterType = AdapterType::Software;
            break;
        default:
            m_adapterType = AdapterType::Unknown;
    }
}

Ref<VKAdapter> VKAdapter::Create(const Ref<Instance>& inst, const vk::PhysicalDevice& physicalDevice)
{
    return CreateRef<VKAdapter>(inst, physicalDevice);
}

AdapterProperties VKAdapter::GetProperties() const
{
    return m_properties;
}

AdapterType VKAdapter::GetAdapterType() const
{
    return m_adapterType;
}

bool VKAdapter::IsFeatureSupported(DeviceFeature feature) const
{
    vk::PhysicalDeviceFeatures m_features = m_physicalDevice.getFeatures();
    switch (feature) {
        case DeviceFeature::RayTracing: {
            vk::PhysicalDeviceRayTracingPipelineFeaturesKHR rayTracingFeatures{};
            vk::PhysicalDeviceAccelerationStructureFeaturesKHR accelerationStructureFeatures{};
            vk::PhysicalDeviceFeatures2 features2{};
            features2.setPNext(&rayTracingFeatures);
            rayTracingFeatures.setPNext(&accelerationStructureFeatures);
            m_physicalDevice.getFeatures2(&features2);
            return rayTracingFeatures.rayTracingPipeline && accelerationStructureFeatures.accelerationStructure;
        }
        case DeviceFeature::MeshShader: {
            vk::PhysicalDeviceMeshShaderFeaturesNV meshShaderFeatures{};
            vk::PhysicalDeviceFeatures2 features2{};
            features2.setPNext(&meshShaderFeatures);
            m_physicalDevice.getFeatures2(&features2);
            return meshShaderFeatures.meshShader && meshShaderFeatures.taskShader;
        }
        case DeviceFeature::VariableRateShading: {
            vk::PhysicalDeviceFragmentShadingRateFeaturesKHR fragmentShadingRateFeatureKHR{};
            vk::PhysicalDeviceFeatures2 features2{};
            features2.setPNext(&fragmentShadingRateFeatureKHR);
            m_physicalDevice.getFeatures2(&features2);
            return fragmentShadingRateFeatureKHR.pipelineFragmentShadingRate;
        }
        case DeviceFeature::BindlessDescriptors: {
            vk::PhysicalDeviceDescriptorIndexingFeatures descriptorIndexingFeatures{};
            vk::PhysicalDeviceFeatures2 features2{};
            features2.setPNext(&descriptorIndexingFeatures);
            m_physicalDevice.getFeatures2(&features2);
            return descriptorIndexingFeatures.descriptorBindingPartiallyBound &&
                   descriptorIndexingFeatures.runtimeDescriptorArray;
        }
        case DeviceFeature::BufferDeviceAddress: {
            vk::PhysicalDeviceBufferDeviceAddressFeatures bufferDeviceAddressFeatures{};
            vk::PhysicalDeviceFeatures2 features2{};
            features2.setPNext(&bufferDeviceAddressFeatures);
            m_physicalDevice.getFeatures2(&features2);
            return bufferDeviceAddressFeatures.bufferDeviceAddress;
        }
        case DeviceFeature::DrawIndirectCount: {
            return m_features.drawIndirectFirstInstance;
        }
        case DeviceFeature::ShaderFloat16: {
            vk::PhysicalDeviceShaderFloat16Int8Features float16Feature{};
            vk::PhysicalDeviceFeatures2 features2{};
            features2.setPNext(&float16Feature);
            m_physicalDevice.getFeatures2(&features2);
            return float16Feature.shaderFloat16;
        }
        case DeviceFeature::ShaderInt64:
            return m_features.shaderInt64;
        case DeviceFeature::GeometryShader:
            return m_features.geometryShader;
        case DeviceFeature::TessellationShader:
            return m_features.tessellationShader;
        case DeviceFeature::MultiViewport:
            return m_features.multiViewport;
        case DeviceFeature::IndependentBlending:
            return m_features.independentBlend;
        case DeviceFeature::PipelineStatistics:
            return m_features.pipelineStatisticsQuery;
        default:
            return false;
    }
}

Ref<Device> VKAdapter::CreateDevice(const DeviceCreateInfo& info)
{
    return VKDevice::Create(shared_from_this(), info);
}

uint32_t VKAdapter::FindQueueFamilyIndex(QueueType type) const
{
    auto queueFamilies = m_physicalDevice.getQueueFamilyProperties();
    for (uint32_t i = 0; i < queueFamilies.size(); i++) {
        const auto& queueFamily = queueFamilies[i];
        switch (type) {
            case QueueType::Graphics:
                if (queueFamily.queueFlags & vk::QueueFlagBits::eGraphics) {
                    return i;
                }
                break;
            case QueueType::Compute:
                if (queueFamily.queueFlags & vk::QueueFlagBits::eCompute) {
                    return i;
                }
                break;
            case QueueType::Transfer:
                if (queueFamily.queueFlags & vk::QueueFlagBits::eTransfer) {
                    return i;
                }
                break;
            default:
                break;
        }
    }
    return UINT32_MAX;
}

DeviceLimits VKAdapter::QueryLimits() const
{
    auto props = m_physicalDevice.getProperties();
    auto& vkLimits = props.limits;
    DeviceLimits limits;
    limits.maxTextureSize2D = vkLimits.maxImageDimension2D;
    limits.maxTextureSize3D = vkLimits.maxImageDimension3D;
    limits.maxTextureSizeCube = vkLimits.maxImageDimensionCube;
    limits.maxTextureArrayLayers = vkLimits.maxImageArrayLayers;
    limits.maxColorAttachments = vkLimits.maxColorAttachments;
    limits.maxViewports = vkLimits.maxViewports;
    limits.maxComputeWorkGroupCountX = vkLimits.maxComputeWorkGroupCount[0];
    limits.maxComputeWorkGroupCountY = vkLimits.maxComputeWorkGroupCount[1];
    limits.maxComputeWorkGroupCountZ = vkLimits.maxComputeWorkGroupCount[2];
    limits.maxComputeWorkGroupSizeX = vkLimits.maxComputeWorkGroupSize[0];
    limits.maxComputeWorkGroupSizeY = vkLimits.maxComputeWorkGroupSize[1];
    limits.maxComputeWorkGroupSizeZ = vkLimits.maxComputeWorkGroupSize[2];
    limits.maxComputeSharedMemorySize = vkLimits.maxComputeSharedMemorySize;
    limits.maxBoundDescriptorSets = vkLimits.maxBoundDescriptorSets;
    limits.maxPushConstantsSize = vkLimits.maxPushConstantsSize;
    limits.maxUniformBufferSize = static_cast<uint32_t>(vkLimits.maxUniformBufferRange);
    limits.maxStorageBufferSize = static_cast<uint32_t>(vkLimits.maxStorageBufferRange);
    limits.maxSamplerAnisotropy = static_cast<uint32_t>(vkLimits.maxSamplerAnisotropy);
    limits.maxLineWidth = vkLimits.lineWidthRange[1];

    auto queueFamilies = m_physicalDevice.getQueueFamilyProperties();
    for (auto& qf : queueFamilies) {
        if ((qf.queueFlags & vk::QueueFlagBits::eCompute) && !(qf.queueFlags & vk::QueueFlagBits::eGraphics)) {
            limits.supportsAsyncCompute = true;
        }
        if ((qf.queueFlags & vk::QueueFlagBits::eTransfer) && !(qf.queueFlags & vk::QueueFlagBits::eGraphics) &&
            !(qf.queueFlags & vk::QueueFlagBits::eCompute)) {
            limits.supportsTransferQueue = true;
        }
    }
    limits.supportsPipelineCacheSerialization = true;
    limits.supportsStorageBufferWriteInGraphics = true;
    return limits;
}
} // namespace Cacao

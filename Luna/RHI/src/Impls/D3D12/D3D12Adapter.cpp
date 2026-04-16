#include "Impls/D3D12/D3D12Adapter.h"
#include "Impls/D3D12/D3D12Instance.h"
#include "Impls/D3D12/D3D12Device.h"

namespace Cacao
{
    D3D12Adapter::D3D12Adapter(const Ref<Instance>& inst, ComPtr<IDXGIAdapter4> adapter)
        : m_adapter(std::move(adapter))
        , m_instance(std::dynamic_pointer_cast<D3D12Instance>(inst))
    {
        DXGI_ADAPTER_DESC3 desc;
        m_adapter->GetDesc3(&desc);

        char name[128];
        WideCharToMultiByte(CP_UTF8, 0, desc.Description, -1, name, sizeof(name), nullptr, nullptr);

        m_properties.name = name;
        m_properties.vendorID = desc.VendorId;
        m_properties.deviceID = desc.DeviceId;
        m_properties.dedicatedVideoMemory = desc.DedicatedVideoMemory;
        m_properties.type = (desc.Flags & DXGI_ADAPTER_FLAG3_SOFTWARE) ? AdapterType::Software : AdapterType::Discrete;

        m_adapterType = m_properties.type;
    }

    Ref<D3D12Adapter> D3D12Adapter::Create(const Ref<Instance>& inst, ComPtr<IDXGIAdapter4> adapter)
    {
        return std::make_shared<D3D12Adapter>(inst, std::move(adapter));
    }

    AdapterProperties D3D12Adapter::GetProperties() const { return m_properties; }
    AdapterType D3D12Adapter::GetAdapterType() const { return m_adapterType; }

    bool D3D12Adapter::IsFeatureSupported(DeviceFeature feature) const
    {
        switch (feature)
        {
        case DeviceFeature::TessellationShader:
        case DeviceFeature::GeometryShader:
        case DeviceFeature::SubgroupOperations:
        case DeviceFeature::MultiDrawIndirect:
        case DeviceFeature::FillModeNonSolid:
            return true;
        case DeviceFeature::RayTracing:
        {
            ComPtr<ID3D12Device5> tempDevice;
            if (SUCCEEDED(D3D12CreateDevice(m_adapter.Get(), D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&tempDevice))))
            {
                D3D12_FEATURE_DATA_D3D12_OPTIONS5 opts5 = {};
                if (SUCCEEDED(tempDevice->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5, &opts5, sizeof(opts5))))
                    return opts5.RaytracingTier >= D3D12_RAYTRACING_TIER_1_0;
            }
            return false;
        }
        default:
            return true;
        }
    }

    Ref<Device> D3D12Adapter::CreateDevice(const DeviceCreateInfo& info)
    {
        return D3D12Device::Create(shared_from_this(), info);
    }

    DeviceLimits D3D12Adapter::QueryLimits() const
    {
        DeviceLimits limits;
        ComPtr<ID3D12Device> tempDevice;
        if (FAILED(D3D12CreateDevice(m_adapter.Get(), D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&tempDevice))))
            return limits;

        limits.maxTextureSize2D = D3D12_REQ_TEXTURE2D_U_OR_V_DIMENSION;
        limits.maxTextureSize3D = D3D12_REQ_TEXTURE3D_U_V_OR_W_DIMENSION;
        limits.maxTextureSizeCube = D3D12_REQ_TEXTURECUBE_DIMENSION;
        limits.maxTextureArrayLayers = D3D12_REQ_TEXTURE2D_ARRAY_AXIS_DIMENSION;
        limits.maxColorAttachments = 8;
        limits.maxViewports = D3D12_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE;
        limits.maxComputeWorkGroupCountX = D3D12_CS_DISPATCH_MAX_THREAD_GROUPS_PER_DIMENSION;
        limits.maxComputeWorkGroupCountY = D3D12_CS_DISPATCH_MAX_THREAD_GROUPS_PER_DIMENSION;
        limits.maxComputeWorkGroupCountZ = D3D12_CS_DISPATCH_MAX_THREAD_GROUPS_PER_DIMENSION;
        limits.maxComputeWorkGroupSizeX = D3D12_CS_THREAD_GROUP_MAX_X;
        limits.maxComputeWorkGroupSizeY = D3D12_CS_THREAD_GROUP_MAX_Y;
        limits.maxComputeWorkGroupSizeZ = D3D12_CS_THREAD_GROUP_MAX_Z;
        limits.maxComputeSharedMemorySize = D3D12_CS_THREAD_LOCAL_TEMP_REGISTER_POOL * 4;
        limits.maxBoundDescriptorSets = 64;
        limits.maxPushConstantsSize = 256;
        limits.maxUniformBufferSize = D3D12_REQ_CONSTANT_BUFFER_ELEMENT_COUNT * 16;
        limits.maxStorageBufferSize = 128 * 1024 * 1024;
        limits.maxSamplerAnisotropy = D3D12_MAX_MAXANISOTROPY;
        limits.maxMSAASamples = 8;
        limits.supportsAsyncCompute = true;
        limits.supportsTransferQueue = true;
        limits.supportsPipelineCacheSerialization = true;
        limits.supportsStorageBufferWriteInGraphics = true;
        return limits;
    }

    uint32_t D3D12Adapter::FindQueueFamilyIndex(QueueType type) const
    {
        return 0;
    }
}

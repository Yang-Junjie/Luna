#include "Impls/D3D11/D3D11Adapter.h"
#include "Impls/D3D11/D3D11Device.h"

namespace Cacao
{
    D3D11Adapter::D3D11Adapter(Ref<D3D11Instance> instance, ComPtr<IDXGIAdapter1> adapter)
        : m_instance(std::move(instance)), m_adapter(std::move(adapter))
    {
        m_adapter->GetDesc1(&m_desc);
    }

    AdapterProperties D3D11Adapter::GetProperties() const
    {
        int len = WideCharToMultiByte(CP_UTF8, 0, m_desc.Description, -1, nullptr, 0, nullptr, nullptr);
        std::string name(len - 1, '\0');
        WideCharToMultiByte(CP_UTF8, 0, m_desc.Description, -1, name.data(), len, nullptr, nullptr);

        return AdapterProperties{
            .deviceID = m_desc.DeviceId,
            .vendorID = m_desc.VendorId,
            .name = name,
            .type = GetAdapterType(),
            .dedicatedVideoMemory = m_desc.DedicatedVideoMemory
        };
    }

    AdapterType D3D11Adapter::GetAdapterType() const
    {
        if (m_desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) return AdapterType::Software;
        if (m_desc.DedicatedVideoMemory > 0) return AdapterType::Discrete;
        return AdapterType::Integrated;
    }

    bool D3D11Adapter::IsFeatureSupported(DeviceFeature feature) const
    {
        switch (feature)
        {
        case DeviceFeature::SamplerAnisotropy:
        case DeviceFeature::IndependentBlending:
        case DeviceFeature::TextureCompressionBC:
            return true;
        case DeviceFeature::GeometryShader:
        case DeviceFeature::TessellationShader:
            return true;
        default:
            return false;
        }
    }

    Ref<Device> D3D11Adapter::CreateDevice(const DeviceCreateInfo& createInfo)
    {
        return CreateRef<D3D11Device>(
            std::static_pointer_cast<D3D11Adapter>(shared_from_this()));
    }

    DeviceLimits D3D11Adapter::QueryLimits() const
    {
        DeviceLimits limits;
        limits.maxTextureSize2D = D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION;
        limits.maxTextureSize3D = D3D11_REQ_TEXTURE3D_U_V_OR_W_DIMENSION;
        limits.maxTextureSizeCube = D3D11_REQ_TEXTURECUBE_DIMENSION;
        limits.maxTextureArrayLayers = D3D11_REQ_TEXTURE2D_ARRAY_AXIS_DIMENSION;
        limits.maxColorAttachments = 8;
        limits.maxViewports = D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE;
        limits.maxComputeWorkGroupCountX = D3D11_CS_DISPATCH_MAX_THREAD_GROUPS_PER_DIMENSION;
        limits.maxComputeWorkGroupCountY = D3D11_CS_DISPATCH_MAX_THREAD_GROUPS_PER_DIMENSION;
        limits.maxComputeWorkGroupCountZ = D3D11_CS_DISPATCH_MAX_THREAD_GROUPS_PER_DIMENSION;
        limits.maxComputeWorkGroupSizeX = D3D11_CS_THREAD_GROUP_MAX_X;
        limits.maxComputeWorkGroupSizeY = D3D11_CS_THREAD_GROUP_MAX_Y;
        limits.maxComputeWorkGroupSizeZ = D3D11_CS_THREAD_GROUP_MAX_Z;
        limits.maxComputeSharedMemorySize = D3D11_CS_THREAD_LOCAL_TEMP_REGISTER_POOL * 4;
        limits.maxBoundDescriptorSets = 4;
        limits.maxPushConstantsSize = 256;
        limits.maxUniformBufferSize = D3D11_REQ_CONSTANT_BUFFER_ELEMENT_COUNT * 16;
        limits.maxStorageBufferSize = 128 * 1024 * 1024;
        limits.maxSamplerAnisotropy = D3D11_MAX_MAXANISOTROPY;
        limits.maxMSAASamples = 8;
        limits.supportsAsyncCompute = false;
        limits.supportsTransferQueue = false;
        limits.supportsPipelineCacheSerialization = false;
        limits.supportsStorageBufferWriteInGraphics = false;
        return limits;
    }
}

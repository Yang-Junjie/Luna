#include "Impls/D3D12/D3D12Device.h"
#include "Impls/D3D12/D3D12Sampler.h"

namespace luna::RHI {
static D3D12_FILTER ToD3D12Filter(Filter min, Filter mag, SamplerMipmapMode mip, bool aniso)
{
    if (aniso) {
        return D3D12_FILTER_ANISOTROPIC;
    }
    bool minLinear = (min == Filter::Linear);
    bool magLinear = (mag == Filter::Linear);
    bool mipLinear = (mip == SamplerMipmapMode::Linear);
    if (minLinear && magLinear && mipLinear) {
        return D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    }
    if (minLinear && magLinear) {
        return D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT;
    }
    if (!minLinear && !magLinear && !mipLinear) {
        return D3D12_FILTER_MIN_MAG_MIP_POINT;
    }
    return D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT;
}

static D3D12_TEXTURE_ADDRESS_MODE ToD3D12AddressMode(SamplerAddressMode mode)
{
    switch (mode) {
        case SamplerAddressMode::Repeat:
            return D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        case SamplerAddressMode::MirroredRepeat:
            return D3D12_TEXTURE_ADDRESS_MODE_MIRROR;
        case SamplerAddressMode::ClampToEdge:
            return D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        case SamplerAddressMode::ClampToBorder:
            return D3D12_TEXTURE_ADDRESS_MODE_BORDER;
        case SamplerAddressMode::MirrorClampToEdge:
            return D3D12_TEXTURE_ADDRESS_MODE_MIRROR_ONCE;
        default:
            return D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    }
}

D3D12Sampler::D3D12Sampler(const Ref<Device>& device, const SamplerCreateInfo& createInfo)
    : m_device(device),
      m_createInfo(createInfo)
{
    auto d3dDevice = std::dynamic_pointer_cast<D3D12Device>(device);

    D3D12_SAMPLER_DESC desc = {};
    desc.Filter = ToD3D12Filter(createInfo.MinFilter,
                                createInfo.MagFilter,
                                createInfo.MipmapMode,
                                createInfo.AnisotropyEnable && createInfo.MaxAnisotropy > 1.0f);
    desc.AddressU = ToD3D12AddressMode(createInfo.AddressModeU);
    desc.AddressV = ToD3D12AddressMode(createInfo.AddressModeV);
    desc.AddressW = ToD3D12AddressMode(createInfo.AddressModeW);
    desc.MaxAnisotropy = static_cast<UINT>(createInfo.MaxAnisotropy);
    desc.MinLOD = createInfo.MinLod;
    desc.MaxLOD = createInfo.MaxLod;
    desc.MipLODBias = createInfo.MipLodBias;

    D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
    heapDesc.NumDescriptors = 1;
    heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
    heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    d3dDevice->GetHandle()->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&m_stagingHeap));
    m_cpuHandle = m_stagingHeap->GetCPUDescriptorHandleForHeapStart();
    d3dDevice->GetHandle()->CreateSampler(&desc, m_cpuHandle);
}

Ref<D3D12Sampler> D3D12Sampler::Create(const Ref<Device>& device, const SamplerCreateInfo& createInfo)
{
    return std::make_shared<D3D12Sampler>(device, createInfo);
}

const SamplerCreateInfo& D3D12Sampler::GetInfo() const
{
    return m_createInfo;
}
} // namespace luna::RHI

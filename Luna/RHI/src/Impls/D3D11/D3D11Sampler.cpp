#include "Impls/D3D11/D3D11Device.h"
#include "Impls/D3D11/D3D11Sampler.h"

namespace Cacao {
D3D11Sampler::D3D11Sampler(Ref<D3D11Device> device, const SamplerCreateInfo& createInfo)
    : m_info(createInfo)
{
    D3D11_SAMPLER_DESC desc{};

    bool isLinearMin = (createInfo.MinFilter == Filter::Linear);
    bool isLinearMag = (createInfo.MagFilter == Filter::Linear);
    bool isLinearMip = (createInfo.MipmapMode == SamplerMipmapMode::Linear);

    if (createInfo.CompareEnable) {
        desc.Filter =
            isLinearMin ? D3D11_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR : D3D11_FILTER_COMPARISON_MIN_MAG_MIP_POINT;
    } else if (createInfo.AnisotropyEnable && createInfo.MaxAnisotropy > 1.0f) {
        desc.Filter = D3D11_FILTER_ANISOTROPIC;
    } else if (isLinearMin && isLinearMag && isLinearMip) {
        desc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    } else if (!isLinearMin && !isLinearMag && !isLinearMip) {
        desc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
    } else {
        desc.Filter = D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT;
    }

    desc.AddressU = D3D11_ToAddressMode(createInfo.AddressModeU);
    desc.AddressV = D3D11_ToAddressMode(createInfo.AddressModeV);
    desc.AddressW = D3D11_ToAddressMode(createInfo.AddressModeW);
    desc.MipLODBias = createInfo.MipLodBias;
    desc.MaxAnisotropy = static_cast<UINT>(createInfo.MaxAnisotropy);
    desc.ComparisonFunc = createInfo.CompareEnable ? D3D11_ToCompareFunc(createInfo.CompareOp) : D3D11_COMPARISON_NEVER;
    desc.MinLOD = createInfo.MinLod;
    desc.MaxLOD = createInfo.MaxLod;

    switch (createInfo.BorderColor) {
        case BorderColor::FloatTransparentBlack:
        case BorderColor::IntTransparentBlack:
            desc.BorderColor[0] = desc.BorderColor[1] = desc.BorderColor[2] = desc.BorderColor[3] = 0.0f;
            break;
        case BorderColor::FloatOpaqueBlack:
        case BorderColor::IntOpaqueBlack:
            desc.BorderColor[0] = desc.BorderColor[1] = desc.BorderColor[2] = 0.0f;
            desc.BorderColor[3] = 1.0f;
            break;
        case BorderColor::FloatOpaqueWhite:
        case BorderColor::IntOpaqueWhite:
            desc.BorderColor[0] = desc.BorderColor[1] = desc.BorderColor[2] = desc.BorderColor[3] = 1.0f;
            break;
    }

    device->GetNativeDevice()->CreateSamplerState(&desc, &m_sampler);
}
} // namespace Cacao

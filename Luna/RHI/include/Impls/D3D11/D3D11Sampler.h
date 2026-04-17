#ifndef LUNA_RHI_D3D11SAMPLER_H
#define LUNA_RHI_D3D11SAMPLER_H
#include "D3D11Common.h"

#include <Sampler.h>

namespace luna::RHI {
class D3D11Device;

inline D3D11_TEXTURE_ADDRESS_MODE D3D11_ToAddressMode(SamplerAddressMode mode)
{
    switch (mode) {
        case SamplerAddressMode::Repeat:
            return D3D11_TEXTURE_ADDRESS_WRAP;
        case SamplerAddressMode::MirroredRepeat:
            return D3D11_TEXTURE_ADDRESS_MIRROR;
        case SamplerAddressMode::ClampToEdge:
            return D3D11_TEXTURE_ADDRESS_CLAMP;
        case SamplerAddressMode::ClampToBorder:
            return D3D11_TEXTURE_ADDRESS_BORDER;
        case SamplerAddressMode::MirrorClampToEdge:
            return D3D11_TEXTURE_ADDRESS_MIRROR_ONCE;
        default:
            return D3D11_TEXTURE_ADDRESS_WRAP;
    }
}

inline D3D11_COMPARISON_FUNC D3D11_ToCompareFunc(CompareOp op)
{
    switch (op) {
        case CompareOp::Never:
            return D3D11_COMPARISON_NEVER;
        case CompareOp::Less:
            return D3D11_COMPARISON_LESS;
        case CompareOp::Equal:
            return D3D11_COMPARISON_EQUAL;
        case CompareOp::LessOrEqual:
            return D3D11_COMPARISON_LESS_EQUAL;
        case CompareOp::Greater:
            return D3D11_COMPARISON_GREATER;
        case CompareOp::NotEqual:
            return D3D11_COMPARISON_NOT_EQUAL;
        case CompareOp::GreaterOrEqual:
            return D3D11_COMPARISON_GREATER_EQUAL;
        case CompareOp::Always:
            return D3D11_COMPARISON_ALWAYS;
        default:
            return D3D11_COMPARISON_LESS_EQUAL;
    }
}

class LUNA_RHI_API D3D11Sampler : public Sampler {
public:
    D3D11Sampler(Ref<D3D11Device> device, const SamplerCreateInfo& createInfo);

    const SamplerCreateInfo& GetInfo() const override
    {
        return m_info;
    }

    ID3D11SamplerState* GetNativeSampler() const
    {
        return m_sampler.Get();
    }

private:
    SamplerCreateInfo m_info;
    ComPtr<ID3D11SamplerState> m_sampler;
};
} // namespace luna::RHI
#endif

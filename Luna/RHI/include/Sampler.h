#ifndef LUNA_RHI_SAMPLER_H
#define LUNA_RHI_SAMPLER_H
#include "Core.h"
#include "PipelineDefs.h"

namespace luna::RHI {
enum class Filter {
    Nearest,
    Linear
};
enum class SamplerMipmapMode {
    Nearest,
    Linear
};
enum class SamplerAddressMode {
    Repeat,
    MirroredRepeat,
    ClampToEdge,
    ClampToBorder,
    MirrorClampToEdge
};
enum class BorderColor {
    FloatTransparentBlack,
    IntTransparentBlack,
    FloatOpaqueBlack,
    IntOpaqueBlack,
    FloatOpaqueWhite,
    IntOpaqueWhite
};

struct SamplerCreateInfo {
    Filter MagFilter = Filter::Linear;
    Filter MinFilter = Filter::Linear;
    SamplerMipmapMode MipmapMode = SamplerMipmapMode::Linear;
    SamplerAddressMode AddressModeU = SamplerAddressMode::Repeat;
    SamplerAddressMode AddressModeV = SamplerAddressMode::Repeat;
    SamplerAddressMode AddressModeW = SamplerAddressMode::Repeat;
    float MipLodBias = 0.0f;
    float MinLod = 0.0f;
    float MaxLod = 1000.0f;
    bool AnisotropyEnable = true;
    float MaxAnisotropy = 16.0f;
    bool CompareEnable = false;
    CompareOp CompareOp = CompareOp::LessOrEqual;
    BorderColor BorderColor = BorderColor::FloatOpaqueBlack;
    bool UnnormalizedCoordinates = false;
    std::string Name;
};

class LUNA_RHI_API Sampler : public std::enable_shared_from_this<Sampler> {
public:
    virtual ~Sampler() = default;
    virtual const SamplerCreateInfo& GetInfo() const = 0;
};
} // namespace luna::RHI
#endif

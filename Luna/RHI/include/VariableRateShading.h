#ifndef LUNA_RHI_VARIABLE_RATE_SHADING_H
#define LUNA_RHI_VARIABLE_RATE_SHADING_H
#include "Core.h"
#include "Texture.h"

#include <memory>

namespace luna::RHI {
enum class ShadingRate : uint8_t {
    Rate1x1 = 0,
    Rate1x2 = 1,
    Rate2x1 = 4,
    Rate2x2 = 5,
    Rate2x4 = 6,
    Rate4x2 = 9,
    Rate4x4 = 10
};

enum class ShadingRateCombiner : uint8_t {
    Passthrough,
    Override,
    Min,
    Max,
    Sum
};

struct ShadingRateImageCreateInfo {
    uint32_t Width = 0;
    uint32_t Height = 0;
    uint32_t TileSize = 16;
};

class LUNA_RHI_API ShadingRateImage {
public:
    virtual ~ShadingRateImage() = default;
    virtual Ref<Texture> GetTexture() const = 0;
    virtual uint32_t GetTileSize() const = 0;
    virtual void SetRate(uint32_t tileX, uint32_t tileY, ShadingRate rate) = 0;
    virtual void Upload() = 0;
};
} // namespace luna::RHI

#endif

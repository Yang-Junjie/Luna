#pragma once

#include "DescriptorEnums.h"

#include <string_view>

namespace luna {

struct BufferDesc {
    uint64_t size = 0;
    BufferUsage usage = BufferUsage::None;
    MemoryUsage memoryUsage = MemoryUsage::Default;
    std::string_view debugName;
};

struct ImageDesc {
    uint32_t width = 1;
    uint32_t height = 1;
    uint32_t depth = 1;
    uint32_t mipLevels = 1;
    uint32_t arrayLayers = 1;
    ImageType type = ImageType::Image2D;
    PixelFormat format = PixelFormat::Undefined;
    ImageUsage usage = ImageUsage::None;
    std::string_view debugName;
};

struct ImageViewDesc {
    ImageHandle image{};
    ImageViewType type = ImageViewType::Image2D;
    ImageAspect aspect = ImageAspect::Color;
    PixelFormat format = PixelFormat::Undefined;
    uint32_t baseMipLevel = 0;
    uint32_t mipCount = 1;
    uint32_t baseArrayLayer = 0;
    uint32_t layerCount = 1;
    std::string_view debugName;
};

struct SamplerDesc {
    FilterMode magFilter = FilterMode::Linear;
    FilterMode minFilter = FilterMode::Linear;
    SamplerMipmapMode mipmapMode = SamplerMipmapMode::Linear;
    SamplerAddressMode addressModeU = SamplerAddressMode::Repeat;
    SamplerAddressMode addressModeV = SamplerAddressMode::Repeat;
    SamplerAddressMode addressModeW = SamplerAddressMode::Repeat;
    float mipLodBias = 0.0f;
    float minLod = 0.0f;
    float maxLod = 1000.0f;
    bool anisotropyEnable = false;
    float maxAnisotropy = 1.0f;
    bool compareEnable = false;
    CompareOp compareOp = CompareOp::LessOrEqual;
    SamplerBorderColor borderColor = SamplerBorderColor::FloatTransparentBlack;
    std::string_view debugName;
};

} // namespace luna


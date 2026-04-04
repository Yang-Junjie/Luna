#pragma once

#include "RHI/Descriptors.h"

#include <array>
#include <algorithm>
#include <cstdint>
#include <string>

namespace image_lab {

enum class Page : uint8_t {
    FormatProbe = 0,
    MRTPreview,
    MipPreview,
    Array3DPreview
};

enum class ArrayPreviewMode : uint8_t {
    Array = 0,
    Volume
};

inline constexpr std::array<luna::PixelFormat, 6> kFormatProbeFormats = {
    luna::PixelFormat::RG16Float,
    luna::PixelFormat::RGBA16Float,
    luna::PixelFormat::R32Float,
    luna::PixelFormat::R11G11B10Float,
    luna::PixelFormat::RGBA8Unorm,
    luna::PixelFormat::D32Float,
};

inline constexpr std::array<luna::PixelFormat, 5> kMrtColorFormats = {
    luna::PixelFormat::RGBA8Unorm,
    luna::PixelFormat::RGBA8Srgb,
    luna::PixelFormat::RG16Float,
    luna::PixelFormat::RGBA16Float,
    luna::PixelFormat::R11G11B10Float,
};

inline uint32_t calculate_theoretical_mip_count(uint32_t width, uint32_t height, uint32_t depth = 1)
{
    uint32_t maxDimension = std::max(width, height);
    maxDimension = std::max(maxDimension, depth);

    uint32_t mipCount = 0;
    do {
        ++mipCount;
        maxDimension >>= 1;
    } while (maxDimension > 0);

    return mipCount;
}

struct FormatProbeState {
    luna::PixelFormat selectedFormat = luna::PixelFormat::RGBA16Float;
    bool probeRequested = true;
    bool accepted = false;
    std::string backendMapping;
    std::string details;
};

struct MrtPreviewState {
    int attachmentCount = 4;
    std::array<luna::PixelFormat, 4> formats = {
        luna::PixelFormat::RGBA8Unorm,
        luna::PixelFormat::RG16Float,
        luna::PixelFormat::R11G11B10Float,
        luna::PixelFormat::RGBA16Float,
    };
    int previewAttachment = 0;
    bool showFourUpView = false;
    bool rebuildRequested = true;
    std::string status;
};

struct MipPreviewState {
    uint32_t width = 256;
    uint32_t height = 256;
    uint32_t mipLevels = calculate_theoretical_mip_count(256, 256);
    float lod = 0.0f;
    bool createRequested = true;
    std::string status;
};

struct Array3DPreviewState {
    luna::ImageType type = luna::ImageType::Image2DArray;
    ArrayPreviewMode previewMode = ArrayPreviewMode::Array;
    uint32_t width = 128;
    uint32_t height = 128;
    uint32_t depth = 16;
    uint32_t mipLevels = 1;
    uint32_t arrayLayers = 4;
    int layer = 0;
    float slice = 0.5f;
    bool createRequested = true;
    std::string status;
};

struct State {
    Page page = Page::FormatProbe;
    FormatProbeState formatProbe;
    MrtPreviewState mrt;
    MipPreviewState mip;
    Array3DPreviewState array3d;
};

} // namespace image_lab

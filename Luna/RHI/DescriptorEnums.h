#pragma once

#include "Types.h"

#include <cstdint>
#include <string_view>

namespace luna {

enum class BufferUsage : uint32_t {
    None = 0,
    TransferSrc = 1u << 0,
    TransferDst = 1u << 1,
    Vertex = 1u << 2,
    Index = 1u << 3,
    Uniform = 1u << 4,
    Storage = 1u << 5,
    Indirect = 1u << 6
};

inline BufferUsage operator|(BufferUsage lhs, BufferUsage rhs)
{
    return static_cast<BufferUsage>(static_cast<uint32_t>(lhs) | static_cast<uint32_t>(rhs));
}

enum class ImageUsage : uint32_t {
    None = 0,
    TransferSrc = 1u << 0,
    TransferDst = 1u << 1,
    Sampled = 1u << 2,
    ColorAttachment = 1u << 3,
    DepthStencilAttachment = 1u << 4,
    Storage = 1u << 5
};

inline ImageUsage operator|(ImageUsage lhs, ImageUsage rhs)
{
    return static_cast<ImageUsage>(static_cast<uint32_t>(lhs) | static_cast<uint32_t>(rhs));
}

enum class MemoryUsage : uint32_t {
    Default = 0,
    Upload,
    Readback
};

enum class PixelFormat : uint32_t {
    Undefined = 0,
    BGRA8Unorm,
    RGBA8Unorm,
    RGBA8Srgb,
    RG16Float,
    RGBA16Float,
    R32Float,
    R11G11B10Float,
    D32Float
};

constexpr std::string_view to_string(PixelFormat format) noexcept
{
    switch (format) {
        case PixelFormat::BGRA8Unorm:
            return "BGRA8Unorm";
        case PixelFormat::RGBA8Unorm:
            return "RGBA8Unorm";
        case PixelFormat::RGBA8Srgb:
            return "RGBA8Srgb";
        case PixelFormat::RG16Float:
            return "RG16Float";
        case PixelFormat::RGBA16Float:
            return "RGBA16Float";
        case PixelFormat::R32Float:
            return "R32Float";
        case PixelFormat::R11G11B10Float:
            return "R11G11B10Float";
        case PixelFormat::D32Float:
            return "D32Float";
        case PixelFormat::Undefined:
        default:
            return "Undefined";
    }
}

constexpr bool is_depth_format(PixelFormat format) noexcept
{
    return format == PixelFormat::D32Float;
}

constexpr uint32_t pixel_format_bits_per_pixel(PixelFormat format) noexcept
{
    switch (format) {
        case PixelFormat::BGRA8Unorm:
        case PixelFormat::RGBA8Unorm:
        case PixelFormat::RGBA8Srgb:
        case PixelFormat::RG16Float:
        case PixelFormat::R32Float:
        case PixelFormat::R11G11B10Float:
        case PixelFormat::D32Float:
            return 32;
        case PixelFormat::RGBA16Float:
            return 64;
        case PixelFormat::Undefined:
        default:
            return 0;
    }
}

constexpr uint32_t pixel_format_bytes_per_pixel(PixelFormat format) noexcept
{
    return pixel_format_bits_per_pixel(format) / 8;
}

enum class ImageType : uint32_t {
    Image2D = 0,
    Image2DArray,
    Image3D,
    Cube
};

constexpr std::string_view to_string(ImageType type) noexcept
{
    switch (type) {
        case ImageType::Image2D:
            return "2D";
        case ImageType::Image2DArray:
            return "2D Array";
        case ImageType::Image3D:
            return "3D";
        case ImageType::Cube:
            return "Cube";
        default:
            return "Unknown";
    }
}

enum class ImageAspect : uint32_t {
    Color = 0,
    Depth
};

constexpr std::string_view to_string(ImageAspect aspect) noexcept
{
    switch (aspect) {
        case ImageAspect::Color:
            return "Color";
        case ImageAspect::Depth:
            return "Depth";
        default:
            return "Unknown";
    }
}

enum class ImageViewType : uint32_t {
    Image2D = 0,
    Image2DArray,
    Image3D,
    Cube
};

constexpr std::string_view to_string(ImageViewType type) noexcept
{
    switch (type) {
        case ImageViewType::Image2D:
            return "2D";
        case ImageViewType::Image2DArray:
            return "2D Array";
        case ImageViewType::Image3D:
            return "3D";
        case ImageViewType::Cube:
            return "Cube";
        default:
            return "Unknown";
    }
}

enum class VertexFormat : uint32_t {
    Undefined = 0,
    Float2,
    Float3,
    Float4,
    UByte4Norm
};

enum class PrimitiveTopology : uint32_t {
    TriangleList = 0,
    TriangleStrip,
    LineList
};

enum class PolygonMode : uint32_t {
    Fill = 0,
    Line
};

enum class CullMode : uint32_t {
    None = 0,
    Front,
    Back
};

enum class FrontFace : uint32_t {
    CounterClockwise = 0,
    Clockwise
};

enum class CompareOp : uint32_t {
    Never = 0,
    Less,
    Equal,
    LessOrEqual,
    Greater,
    Always
};

constexpr std::string_view to_string(CompareOp compareOp) noexcept
{
    switch (compareOp) {
        case CompareOp::Never:
            return "Never";
        case CompareOp::Less:
            return "Less";
        case CompareOp::Equal:
            return "Equal";
        case CompareOp::LessOrEqual:
            return "LessOrEqual";
        case CompareOp::Greater:
            return "Greater";
        case CompareOp::Always:
            return "Always";
        default:
            return "Unknown";
    }
}

enum class FilterMode : uint32_t {
    Nearest = 0,
    Linear
};

constexpr std::string_view to_string(FilterMode mode) noexcept
{
    switch (mode) {
        case FilterMode::Nearest:
            return "Nearest";
        case FilterMode::Linear:
            return "Linear";
        default:
            return "Unknown";
    }
}

enum class SamplerMipmapMode : uint32_t {
    Nearest = 0,
    Linear
};

constexpr std::string_view to_string(SamplerMipmapMode mode) noexcept
{
    switch (mode) {
        case SamplerMipmapMode::Nearest:
            return "Nearest";
        case SamplerMipmapMode::Linear:
            return "Linear";
        default:
            return "Unknown";
    }
}

enum class SamplerAddressMode : uint32_t {
    Repeat = 0,
    MirroredRepeat,
    ClampToEdge,
    ClampToBorder
};

constexpr std::string_view to_string(SamplerAddressMode mode) noexcept
{
    switch (mode) {
        case SamplerAddressMode::Repeat:
            return "Repeat";
        case SamplerAddressMode::MirroredRepeat:
            return "MirroredRepeat";
        case SamplerAddressMode::ClampToEdge:
            return "ClampToEdge";
        case SamplerAddressMode::ClampToBorder:
            return "ClampToBorder";
        default:
            return "Unknown";
    }
}

enum class SamplerBorderColor : uint32_t {
    FloatTransparentBlack = 0,
    FloatOpaqueBlack,
    FloatOpaqueWhite
};

constexpr std::string_view to_string(SamplerBorderColor color) noexcept
{
    switch (color) {
        case SamplerBorderColor::FloatTransparentBlack:
            return "FloatTransparentBlack";
        case SamplerBorderColor::FloatOpaqueBlack:
            return "FloatOpaqueBlack";
        case SamplerBorderColor::FloatOpaqueWhite:
            return "FloatOpaqueWhite";
        default:
            return "Unknown";
    }
}

} // namespace luna


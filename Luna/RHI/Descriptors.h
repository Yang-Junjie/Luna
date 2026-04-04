#pragma once

#include "Types.h"

#include <cstdint>
#include <string_view>
#include <vector>

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
    Image3D
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
    Image3D
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

enum class FilterMode : uint32_t {
    Nearest = 0,
    Linear
};

enum class SamplerMipmapMode : uint32_t {
    Nearest = 0,
    Linear
};

enum class SamplerAddressMode : uint32_t {
    Repeat = 0,
    MirroredRepeat,
    ClampToEdge
};

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
    float minLod = 0.0f;
    float maxLod = 1000.0f;
    std::string_view debugName;
};

struct ShaderDesc {
    ShaderType stage = ShaderType::None;
    std::string_view filePath;
    std::string_view entryPoint = "main";
};

struct VertexAttributeDesc {
    uint32_t location = 0;
    uint32_t offset = 0;
    VertexFormat format = VertexFormat::Undefined;
};

struct VertexBufferLayoutDesc {
    uint32_t stride = 0;
    std::vector<VertexAttributeDesc> attributes;
};

struct ColorAttachmentDesc {
    PixelFormat format = PixelFormat::Undefined;
    bool blendEnabled = false;
};

struct DepthStencilDesc {
    PixelFormat format = PixelFormat::Undefined;
    bool depthTestEnabled = false;
    bool depthWriteEnabled = false;
    CompareOp depthCompareOp = CompareOp::LessOrEqual;
};

struct GraphicsPipelineDesc {
    std::string_view debugName;
    ShaderDesc vertexShader;
    ShaderDesc fragmentShader;
    std::vector<ResourceLayoutHandle> resourceLayouts;
    VertexBufferLayoutDesc vertexLayout;
    PrimitiveTopology topology = PrimitiveTopology::TriangleList;
    PolygonMode polygonMode = PolygonMode::Fill;
    CullMode cullMode = CullMode::Back;
    FrontFace frontFace = FrontFace::CounterClockwise;
    uint32_t pushConstantSize = 0;
    ShaderType pushConstantVisibility = ShaderType::AllGraphics;
    std::vector<ColorAttachmentDesc> colorAttachments;
    DepthStencilDesc depthStencil;
};

struct ComputePipelineDesc {
    std::string_view debugName;
    ShaderDesc computeShader;
    std::vector<ResourceLayoutHandle> resourceLayouts;
    uint32_t pushConstantSize = 0;
    ShaderType pushConstantVisibility = ShaderType::Compute;
};

struct SwapchainDesc {
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t bufferCount = 2;
    PixelFormat format = PixelFormat::BGRA8Unorm;
    bool vsync = true;
};

} // namespace luna

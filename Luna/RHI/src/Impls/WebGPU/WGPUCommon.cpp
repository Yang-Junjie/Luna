#include "Impls/WebGPU/WGPUCommon.h"

namespace Cacao {
WGPUTextureFormat ToWGPUFormat(Format format)
{
    switch (format) {
        case Format::R8_UNORM:
            return WGPUTextureFormat_R8Unorm;
        case Format::R8_SNORM:
            return WGPUTextureFormat_R8Snorm;
        case Format::R8_UINT:
            return WGPUTextureFormat_R8Uint;
        case Format::R8_SINT:
            return WGPUTextureFormat_R8Sint;
        case Format::RG8_UNORM:
            return WGPUTextureFormat_RG8Unorm;
        case Format::RG8_SNORM:
            return WGPUTextureFormat_RG8Snorm;
        case Format::RG8_UINT:
            return WGPUTextureFormat_RG8Uint;
        case Format::RG8_SINT:
            return WGPUTextureFormat_RG8Sint;
        case Format::RGBA8_UNORM:
            return WGPUTextureFormat_RGBA8Unorm;
        case Format::RGBA8_SNORM:
            return WGPUTextureFormat_RGBA8Snorm;
        case Format::RGBA8_UINT:
            return WGPUTextureFormat_RGBA8Uint;
        case Format::RGBA8_SINT:
            return WGPUTextureFormat_RGBA8Sint;
        case Format::RGBA8_SRGB:
            return WGPUTextureFormat_RGBA8UnormSrgb;
        case Format::BGRA8_UNORM:
            return WGPUTextureFormat_BGRA8Unorm;
        case Format::BGRA8_SRGB:
            return WGPUTextureFormat_BGRA8UnormSrgb;
        case Format::R16_UINT:
            return WGPUTextureFormat_R16Uint;
        case Format::R16_SINT:
            return WGPUTextureFormat_R16Sint;
        case Format::R16_FLOAT:
            return WGPUTextureFormat_R16Float;
        case Format::RG16_UINT:
            return WGPUTextureFormat_RG16Uint;
        case Format::RG16_SINT:
            return WGPUTextureFormat_RG16Sint;
        case Format::RG16_FLOAT:
            return WGPUTextureFormat_RG16Float;
        case Format::RGBA16_UINT:
            return WGPUTextureFormat_RGBA16Uint;
        case Format::RGBA16_SINT:
            return WGPUTextureFormat_RGBA16Sint;
        case Format::RGBA16_FLOAT:
            return WGPUTextureFormat_RGBA16Float;
        case Format::R32_UINT:
            return WGPUTextureFormat_R32Uint;
        case Format::R32_SINT:
            return WGPUTextureFormat_R32Sint;
        case Format::R32_FLOAT:
            return WGPUTextureFormat_R32Float;
        case Format::RG32_UINT:
            return WGPUTextureFormat_RG32Uint;
        case Format::RG32_SINT:
            return WGPUTextureFormat_RG32Sint;
        case Format::RG32_FLOAT:
            return WGPUTextureFormat_RG32Float;
        case Format::RGBA32_UINT:
            return WGPUTextureFormat_RGBA32Uint;
        case Format::RGBA32_SINT:
            return WGPUTextureFormat_RGBA32Sint;
        case Format::RGBA32_FLOAT:
            return WGPUTextureFormat_RGBA32Float;
        case Format::RGB10A2_UNORM:
            return WGPUTextureFormat_RGB10A2Unorm;
        case Format::RG11B10_FLOAT:
            return WGPUTextureFormat_RG11B10Ufloat;
        case Format::D16_UNORM:
            return WGPUTextureFormat_Depth16Unorm;
        case Format::D24_UNORM_S8_UINT:
            return WGPUTextureFormat_Depth24PlusStencil8;
        case Format::D32_FLOAT:
            return WGPUTextureFormat_Depth32Float;
        case Format::D32_FLOAT_S8_UINT:
            return WGPUTextureFormat_Depth32FloatStencil8;
        case Format::S8_UINT:
            return WGPUTextureFormat_Stencil8;
        case Format::BC1_RGB_UNORM:
            return WGPUTextureFormat_BC1RGBAUnorm;
        case Format::BC1_RGB_SRGB:
            return WGPUTextureFormat_BC1RGBAUnormSrgb;
        case Format::BC2_UNORM:
            return WGPUTextureFormat_BC2RGBAUnorm;
        case Format::BC2_SRGB:
            return WGPUTextureFormat_BC2RGBAUnormSrgb;
        case Format::BC3_UNORM:
            return WGPUTextureFormat_BC3RGBAUnorm;
        case Format::BC3_SRGB:
            return WGPUTextureFormat_BC3RGBAUnormSrgb;
        case Format::BC4_UNORM:
            return WGPUTextureFormat_BC4RUnorm;
        case Format::BC4_SNORM:
            return WGPUTextureFormat_BC4RSnorm;
        case Format::BC5_UNORM:
            return WGPUTextureFormat_BC5RGUnorm;
        case Format::BC5_SNORM:
            return WGPUTextureFormat_BC5RGSnorm;
        case Format::BC6H_UFLOAT:
            return WGPUTextureFormat_BC6HRGBUfloat;
        case Format::BC6H_SFLOAT:
            return WGPUTextureFormat_BC6HRGBFloat;
        case Format::BC7_UNORM:
            return WGPUTextureFormat_BC7RGBAUnorm;
        case Format::BC7_SRGB:
            return WGPUTextureFormat_BC7RGBAUnormSrgb;
        default:
            return WGPUTextureFormat_Undefined;
    }
}

Format FromWGPUFormat(WGPUTextureFormat format)
{
    switch (format) {
        case WGPUTextureFormat_RGBA8Unorm:
            return Format::RGBA8_UNORM;
        case WGPUTextureFormat_BGRA8Unorm:
            return Format::BGRA8_UNORM;
        case WGPUTextureFormat_RGBA8UnormSrgb:
            return Format::RGBA8_SRGB;
        case WGPUTextureFormat_BGRA8UnormSrgb:
            return Format::BGRA8_SRGB;
        case WGPUTextureFormat_Depth32Float:
            return Format::D32F;
        case WGPUTextureFormat_Depth24PlusStencil8:
            return Format::D24S8;
        case WGPUTextureFormat_R32Float:
            return Format::R32_FLOAT;
        default:
            return Format::UNDEFINED;
    }
}

WGPUVertexFormat ToWGPUVertexFormat(Format format)
{
    switch (format) {
        case Format::R32_FLOAT:
            return WGPUVertexFormat_Float32;
        case Format::RG32_FLOAT:
            return WGPUVertexFormat_Float32x2;
        case Format::RGB32_FLOAT:
            return WGPUVertexFormat_Float32x3;
        case Format::RGBA32_FLOAT:
            return WGPUVertexFormat_Float32x4;
        case Format::R32_UINT:
            return WGPUVertexFormat_Uint32;
        case Format::RG32_UINT:
            return WGPUVertexFormat_Uint32x2;
        case Format::RGB32_UINT:
            return WGPUVertexFormat_Uint32x3;
        case Format::RGBA32_UINT:
            return WGPUVertexFormat_Uint32x4;
        case Format::R32_SINT:
            return WGPUVertexFormat_Sint32;
        case Format::RG32_SINT:
            return WGPUVertexFormat_Sint32x2;
        case Format::RGB32_SINT:
            return WGPUVertexFormat_Sint32x3;
        case Format::RGBA32_SINT:
            return WGPUVertexFormat_Sint32x4;
        case Format::R16_FLOAT:
            return WGPUVertexFormat_Float16x2;
        case Format::RG16_FLOAT:
            return WGPUVertexFormat_Float16x2;
        case Format::RGBA16_FLOAT:
            return WGPUVertexFormat_Float16x4;
        case Format::R8_UNORM:
            return WGPUVertexFormat_Unorm8x2;
        case Format::RGBA8_UNORM:
            return WGPUVertexFormat_Unorm8x4;
        case Format::R8_SNORM:
            return WGPUVertexFormat_Snorm8x2;
        case Format::RGBA8_SNORM:
            return WGPUVertexFormat_Snorm8x4;
        default:
            return WGPUVertexFormat_Float32x4;
    }
}

WGPUPrimitiveTopology ToWGPUTopology(PrimitiveTopology topology)
{
    switch (topology) {
        case PrimitiveTopology::PointList:
            return WGPUPrimitiveTopology_PointList;
        case PrimitiveTopology::LineList:
            return WGPUPrimitiveTopology_LineList;
        case PrimitiveTopology::LineStrip:
            return WGPUPrimitiveTopology_LineStrip;
        case PrimitiveTopology::TriangleList:
            return WGPUPrimitiveTopology_TriangleList;
        case PrimitiveTopology::TriangleStrip:
            return WGPUPrimitiveTopology_TriangleStrip;
        default:
            return WGPUPrimitiveTopology_TriangleList;
    }
}

WGPUCullMode ToWGPUCullMode(CullMode mode)
{
    switch (mode) {
        case CullMode::None:
            return WGPUCullMode_None;
        case CullMode::Front:
            return WGPUCullMode_Front;
        case CullMode::Back:
            return WGPUCullMode_Back;
        default:
            return WGPUCullMode_None;
    }
}

WGPUFrontFace ToWGPUFrontFace(FrontFace face)
{
    switch (face) {
        case FrontFace::CounterClockwise:
            return WGPUFrontFace_CCW;
        case FrontFace::Clockwise:
            return WGPUFrontFace_CW;
        default:
            return WGPUFrontFace_CCW;
    }
}

WGPUFilterMode ToWGPUFilterMode(Filter filter)
{
    switch (filter) {
        case Filter::Nearest:
            return WGPUFilterMode_Nearest;
        case Filter::Linear:
            return WGPUFilterMode_Linear;
        default:
            return WGPUFilterMode_Nearest;
    }
}

WGPUAddressMode ToWGPUAddressMode(SamplerAddressMode mode)
{
    switch (mode) {
        case SamplerAddressMode::Repeat:
            return WGPUAddressMode_Repeat;
        case SamplerAddressMode::MirroredRepeat:
            return WGPUAddressMode_MirrorRepeat;
        case SamplerAddressMode::ClampToEdge:
            return WGPUAddressMode_ClampToEdge;
        default:
            return WGPUAddressMode_ClampToEdge;
    }
}

WGPUCompareFunction ToWGPUCompareFunction(CompareOp op)
{
    switch (op) {
        case CompareOp::Never:
            return WGPUCompareFunction_Never;
        case CompareOp::Less:
            return WGPUCompareFunction_Less;
        case CompareOp::Equal:
            return WGPUCompareFunction_Equal;
        case CompareOp::LessOrEqual:
            return WGPUCompareFunction_LessEqual;
        case CompareOp::Greater:
            return WGPUCompareFunction_Greater;
        case CompareOp::NotEqual:
            return WGPUCompareFunction_NotEqual;
        case CompareOp::GreaterOrEqual:
            return WGPUCompareFunction_GreaterEqual;
        case CompareOp::Always:
            return WGPUCompareFunction_Always;
        default:
            return WGPUCompareFunction_Always;
    }
}

WGPUBlendFactor ToWGPUBlendFactor(BlendFactor factor)
{
    switch (factor) {
        case BlendFactor::Zero:
            return WGPUBlendFactor_Zero;
        case BlendFactor::One:
            return WGPUBlendFactor_One;
        case BlendFactor::SrcColor:
            return WGPUBlendFactor_Src;
        case BlendFactor::OneMinusSrcColor:
            return WGPUBlendFactor_OneMinusSrc;
        case BlendFactor::DstColor:
            return WGPUBlendFactor_Dst;
        case BlendFactor::OneMinusDstColor:
            return WGPUBlendFactor_OneMinusDst;
        case BlendFactor::SrcAlpha:
            return WGPUBlendFactor_SrcAlpha;
        case BlendFactor::OneMinusSrcAlpha:
            return WGPUBlendFactor_OneMinusSrcAlpha;
        case BlendFactor::DstAlpha:
            return WGPUBlendFactor_DstAlpha;
        case BlendFactor::OneMinusDstAlpha:
            return WGPUBlendFactor_OneMinusDstAlpha;
        case BlendFactor::ConstantColor:
            return WGPUBlendFactor_Constant;
        case BlendFactor::OneMinusConstantColor:
            return WGPUBlendFactor_OneMinusConstant;
        case BlendFactor::SrcAlphaSaturate:
            return WGPUBlendFactor_SrcAlphaSaturated;
        default:
            return WGPUBlendFactor_One;
    }
}

WGPUBlendOperation ToWGPUBlendOp(BlendOp op)
{
    switch (op) {
        case BlendOp::Add:
            return WGPUBlendOperation_Add;
        case BlendOp::Subtract:
            return WGPUBlendOperation_Subtract;
        case BlendOp::ReverseSubtract:
            return WGPUBlendOperation_ReverseSubtract;
        case BlendOp::Min:
            return WGPUBlendOperation_Min;
        case BlendOp::Max:
            return WGPUBlendOperation_Max;
        default:
            return WGPUBlendOperation_Add;
    }
}

WGPUStencilOperation ToWGPUStencilOp(StencilOp op)
{
    switch (op) {
        case StencilOp::Keep:
            return WGPUStencilOperation_Keep;
        case StencilOp::Zero:
            return WGPUStencilOperation_Zero;
        case StencilOp::Replace:
            return WGPUStencilOperation_Replace;
        case StencilOp::IncrementAndClamp:
            return WGPUStencilOperation_IncrementClamp;
        case StencilOp::DecrementAndClamp:
            return WGPUStencilOperation_DecrementClamp;
        case StencilOp::Invert:
            return WGPUStencilOperation_Invert;
        case StencilOp::IncrementWrap:
            return WGPUStencilOperation_IncrementWrap;
        case StencilOp::DecrementWrap:
            return WGPUStencilOperation_DecrementWrap;
        default:
            return WGPUStencilOperation_Keep;
    }
}

WGPUColorWriteMaskFlags ToWGPUColorWriteMask(ColorComponentFlags flags)
{
    WGPUColorWriteMaskFlags mask = WGPUColorWriteMask_None;
    if (flags & ColorComponentFlags::R) {
        mask |= WGPUColorWriteMask_Red;
    }
    if (flags & ColorComponentFlags::G) {
        mask |= WGPUColorWriteMask_Green;
    }
    if (flags & ColorComponentFlags::B) {
        mask |= WGPUColorWriteMask_Blue;
    }
    if (flags & ColorComponentFlags::A) {
        mask |= WGPUColorWriteMask_Alpha;
    }
    return mask;
}

WGPUBufferUsageFlags ToWGPUBufferUsage(BufferUsageFlags usage)
{
    WGPUBufferUsageFlags flags = WGPUBufferUsage_None;
    if (usage & BufferUsageFlags::TransferSrc) {
        flags |= WGPUBufferUsage_CopySrc;
    }
    if (usage & BufferUsageFlags::TransferDst) {
        flags |= WGPUBufferUsage_CopyDst;
    }
    if (usage & BufferUsageFlags::UniformBuffer) {
        flags |= WGPUBufferUsage_Uniform;
    }
    if (usage & BufferUsageFlags::StorageBuffer) {
        flags |= WGPUBufferUsage_Storage;
    }
    if (usage & BufferUsageFlags::IndexBuffer) {
        flags |= WGPUBufferUsage_Index;
    }
    if (usage & BufferUsageFlags::VertexBuffer) {
        flags |= WGPUBufferUsage_Vertex;
    }
    if (usage & BufferUsageFlags::IndirectBuffer) {
        flags |= WGPUBufferUsage_Indirect;
    }
    return flags;
}

WGPUTextureUsageFlags ToWGPUTextureUsage(TextureUsageFlags usage)
{
    WGPUTextureUsageFlags flags = WGPUTextureUsage_None;
    if (usage & TextureUsageFlags::TransferSrc) {
        flags |= WGPUTextureUsage_CopySrc;
    }
    if (usage & TextureUsageFlags::TransferDst) {
        flags |= WGPUTextureUsage_CopyDst;
    }
    if (usage & TextureUsageFlags::Sampled) {
        flags |= WGPUTextureUsage_TextureBinding;
    }
    if (usage & TextureUsageFlags::Storage) {
        flags |= WGPUTextureUsage_StorageBinding;
    }
    if (usage & TextureUsageFlags::ColorAttachment) {
        flags |= WGPUTextureUsage_RenderAttachment;
    }
    if (usage & TextureUsageFlags::DepthStencilAttachment) {
        flags |= WGPUTextureUsage_RenderAttachment;
    }
    return flags;
}
} // namespace Cacao

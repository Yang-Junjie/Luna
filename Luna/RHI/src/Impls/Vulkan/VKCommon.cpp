#include "Buffer.h"
#include "Impls/Vulkan/VKCommon.h"

namespace Cacao {
vk::BufferUsageFlags VKConverter::Convert(BufferUsageFlags usage)
{
    vk::BufferUsageFlags usageFlags;
    if (usage & BufferUsageFlags::VertexBuffer) {
        usageFlags |= vk::BufferUsageFlagBits::eVertexBuffer;
    }
    if (usage & BufferUsageFlags::IndexBuffer) {
        usageFlags |= vk::BufferUsageFlagBits::eIndexBuffer;
    }
    if (usage & BufferUsageFlags::UniformBuffer) {
        usageFlags |= vk::BufferUsageFlagBits::eUniformBuffer;
    }
    if (usage & BufferUsageFlags::StorageBuffer) {
        usageFlags |= vk::BufferUsageFlagBits::eStorageBuffer;
    }
    if (usage & BufferUsageFlags::TransferSrc) {
        usageFlags |= vk::BufferUsageFlagBits::eTransferSrc;
    }
    if (usage & BufferUsageFlags::TransferDst) {
        usageFlags |= vk::BufferUsageFlagBits::eTransferDst;
    }
    if (usage & BufferUsageFlags::IndirectBuffer) {
        usageFlags |= vk::BufferUsageFlagBits::eIndirectBuffer;
    }
    if (usage & BufferUsageFlags::ShaderDeviceAddress) {
        usageFlags |= vk::BufferUsageFlagBits::eShaderDeviceAddress;
    }
    if (usage & BufferUsageFlags::AccelerationStructure) {
        usageFlags |= vk::BufferUsageFlagBits::eAccelerationStructureStorageKHR;
    }
    return usageFlags;
}

VmaMemoryUsage VKConverter::Convert(BufferMemoryUsage usage)
{
    switch (usage) {
        case BufferMemoryUsage::GpuOnly:
            return VMA_MEMORY_USAGE_GPU_ONLY;
        case BufferMemoryUsage::CpuOnly:
            return VMA_MEMORY_USAGE_CPU_ONLY;
        case BufferMemoryUsage::CpuToGpu:
            return VMA_MEMORY_USAGE_CPU_TO_GPU;
        case BufferMemoryUsage::GpuToCpu:
            return VMA_MEMORY_USAGE_GPU_TO_CPU;
        default:
            return VMA_MEMORY_USAGE_GPU_ONLY;
    }
}

vk::Format VKConverter::Convert(Format format)
{
    switch (format) {
        case Format::R8_UNORM:
            return vk::Format::eR8Unorm;
        case Format::R8_SNORM:
            return vk::Format::eR8Snorm;
        case Format::R8_UINT:
            return vk::Format::eR8Uint;
        case Format::R8_SINT:
            return vk::Format::eR8Sint;
        case Format::RG8_UNORM:
            return vk::Format::eR8G8Unorm;
        case Format::RG8_SNORM:
            return vk::Format::eR8G8Snorm;
        case Format::RG8_UINT:
            return vk::Format::eR8G8Uint;
        case Format::RG8_SINT:
            return vk::Format::eR8G8Sint;
        case Format::RGBA8_UNORM:
            return vk::Format::eR8G8B8A8Unorm;
        case Format::RGBA8_SNORM:
            return vk::Format::eR8G8B8A8Snorm;
        case Format::RGBA8_UINT:
            return vk::Format::eR8G8B8A8Uint;
        case Format::RGBA8_SINT:
            return vk::Format::eR8G8B8A8Sint;
        case Format::RGBA8_SRGB:
            return vk::Format::eR8G8B8A8Srgb;
        case Format::BGRA8_UNORM:
            return vk::Format::eB8G8R8A8Unorm;
        case Format::BGRA8_SRGB:
            return vk::Format::eB8G8R8A8Srgb;
        case Format::R16_UNORM:
            return vk::Format::eR16Unorm;
        case Format::R16_SNORM:
            return vk::Format::eR16Snorm;
        case Format::R16_UINT:
            return vk::Format::eR16Uint;
        case Format::R16_SINT:
            return vk::Format::eR16Sint;
        case Format::R16_FLOAT:
            return vk::Format::eR16Sfloat;
        case Format::RG16_UNORM:
            return vk::Format::eR16G16Unorm;
        case Format::RG16_SNORM:
            return vk::Format::eR16G16Snorm;
        case Format::RG16_UINT:
            return vk::Format::eR16G16Uint;
        case Format::RG16_SINT:
            return vk::Format::eR16G16Sint;
        case Format::RG16_FLOAT:
            return vk::Format::eR16G16Sfloat;
        case Format::RGBA16_UNORM:
            return vk::Format::eR16G16B16A16Unorm;
        case Format::RGBA16_SNORM:
            return vk::Format::eR16G16B16A16Snorm;
        case Format::RGBA16_UINT:
            return vk::Format::eR16G16B16A16Uint;
        case Format::RGBA16_SINT:
            return vk::Format::eR16G16B16A16Sint;
        case Format::RGBA16_FLOAT:
            return vk::Format::eR16G16B16A16Sfloat;
        case Format::R32_UINT:
            return vk::Format::eR32Uint;
        case Format::R32_SINT:
            return vk::Format::eR32Sint;
        case Format::R32_FLOAT:
            return vk::Format::eR32Sfloat;
        case Format::RG32_UINT:
            return vk::Format::eR32G32Uint;
        case Format::RG32_SINT:
            return vk::Format::eR32G32Sint;
        case Format::RG32_FLOAT:
            return vk::Format::eR32G32Sfloat;
        case Format::RGB32_UINT:
            return vk::Format::eR32G32B32Uint;
        case Format::RGB32_SINT:
            return vk::Format::eR32G32B32Sint;
        case Format::RGB32_FLOAT:
            return vk::Format::eR32G32B32Sfloat;
        case Format::RGBA32_UINT:
            return vk::Format::eR32G32B32A32Uint;
        case Format::RGBA32_SINT:
            return vk::Format::eR32G32B32A32Sint;
        case Format::RGBA32_FLOAT:
            return vk::Format::eR32G32B32A32Sfloat;
        case Format::RGB10A2_UNORM:
            return vk::Format::eA2B10G10R10UnormPack32;
        case Format::RGB10A2_UINT:
            return vk::Format::eA2B10G10R10UintPack32;
        case Format::RG11B10_FLOAT:
            return vk::Format::eB10G11R11UfloatPack32;
        case Format::RGB9E5_FLOAT:
            return vk::Format::eE5B9G9R9UfloatPack32;
        case Format::D16_UNORM:
            return vk::Format::eD16Unorm;
        case Format::D24_UNORM_S8_UINT:
            return vk::Format::eD24UnormS8Uint;
        case Format::D32_FLOAT:
            return vk::Format::eD32Sfloat;
        case Format::D32_FLOAT_S8_UINT:
            return vk::Format::eD32SfloatS8Uint;
        case Format::S8_UINT:
            return vk::Format::eS8Uint;
        case Format::BC1_RGB_UNORM:
            return vk::Format::eBc1RgbUnormBlock;
        case Format::BC1_RGB_SRGB:
            return vk::Format::eBc1RgbSrgbBlock;
        case Format::BC1_RGBA_UNORM:
            return vk::Format::eBc1RgbaUnormBlock;
        case Format::BC1_RGBA_SRGB:
            return vk::Format::eBc1RgbaSrgbBlock;
        case Format::BC2_UNORM:
            return vk::Format::eBc2UnormBlock;
        case Format::BC2_SRGB:
            return vk::Format::eBc2SrgbBlock;
        case Format::BC3_UNORM:
            return vk::Format::eBc3UnormBlock;
        case Format::BC3_SRGB:
            return vk::Format::eBc3SrgbBlock;
        case Format::BC4_UNORM:
            return vk::Format::eBc4UnormBlock;
        case Format::BC4_SNORM:
            return vk::Format::eBc4SnormBlock;
        case Format::BC5_UNORM:
            return vk::Format::eBc5UnormBlock;
        case Format::BC5_SNORM:
            return vk::Format::eBc5SnormBlock;
        case Format::BC6H_UFLOAT:
            return vk::Format::eBc6HUfloatBlock;
        case Format::BC6H_SFLOAT:
            return vk::Format::eBc6HSfloatBlock;
        case Format::BC7_UNORM:
            return vk::Format::eBc7UnormBlock;
        case Format::BC7_SRGB:
            return vk::Format::eBc7SrgbBlock;
        default:
            throw std::runtime_error("Unsupported texture format in VKConvert");
    }
}

vk::ShaderStageFlagBits VKConverter::ConvertShaderStageBits(ShaderStage stage)
{
    switch (stage) {
        case ShaderStage::Vertex:
            return vk::ShaderStageFlagBits::eVertex;
        case ShaderStage::Fragment:
            return vk::ShaderStageFlagBits::eFragment;
        case ShaderStage::Compute:
            return vk::ShaderStageFlagBits::eCompute;
        case ShaderStage::Geometry:
            return vk::ShaderStageFlagBits::eGeometry;
        case ShaderStage::TessellationControl:
            return vk::ShaderStageFlagBits::eTessellationControl;
        case ShaderStage::TessellationEvaluation:
            return vk::ShaderStageFlagBits::eTessellationEvaluation;
        default:
            throw std::runtime_error("Unknown ShaderStage");
    }
}

vk::ShaderStageFlags VKConverter::ConvertShaderStageFlags(ShaderStage stage)
{
    vk::ShaderStageFlags flags;
    if (stage == ShaderStage::None) {
        return flags;
    }
    auto stageBits = static_cast<uint32_t>(stage);
    if (stageBits & static_cast<uint32_t>(ShaderStage::Vertex)) {
        flags |= vk::ShaderStageFlagBits::eVertex;
    }
    if (stageBits & static_cast<uint32_t>(ShaderStage::Fragment)) {
        flags |= vk::ShaderStageFlagBits::eFragment;
    }
    if (stageBits & static_cast<uint32_t>(ShaderStage::Compute)) {
        flags |= vk::ShaderStageFlagBits::eCompute;
    }
    if (stageBits & static_cast<uint32_t>(ShaderStage::Geometry)) {
        flags |= vk::ShaderStageFlagBits::eGeometry;
    }
    if (stageBits & static_cast<uint32_t>(ShaderStage::TessellationControl)) {
        flags |= vk::ShaderStageFlagBits::eTessellationControl;
    }
    if (stageBits & static_cast<uint32_t>(ShaderStage::TessellationEvaluation)) {
        flags |= vk::ShaderStageFlagBits::eTessellationEvaluation;
    }
#ifdef VK_ENABLE_BETA_EXTENSIONS
    if (stageBits & static_cast<uint32_t>(ShaderStage::Mesh)) {
        flags |= vk::ShaderStageFlagBits::eMeshEXT;
    }
    if (stageBits & static_cast<uint32_t>(ShaderStage::Task)) {
        flags |= vk::ShaderStageFlagBits::eTaskEXT;
    }
#endif
#ifdef VK_KHR_ray_tracing_pipeline
    if (stageBits & static_cast<uint32_t>(ShaderStage::RayGen)) {
        flags |= vk::ShaderStageFlagBits::eRaygenKHR;
    }
    if (stageBits & static_cast<uint32_t>(ShaderStage::RayAnyHit)) {
        flags |= vk::ShaderStageFlagBits::eAnyHitKHR;
    }
    if (stageBits & static_cast<uint32_t>(ShaderStage::RayClosestHit)) {
        flags |= vk::ShaderStageFlagBits::eClosestHitKHR;
    }
    if (stageBits & static_cast<uint32_t>(ShaderStage::RayMiss)) {
        flags |= vk::ShaderStageFlagBits::eMissKHR;
    }
    if (stageBits & static_cast<uint32_t>(ShaderStage::RayIntersection)) {
        flags |= vk::ShaderStageFlagBits::eIntersectionKHR;
    }
    if (stageBits & static_cast<uint32_t>(ShaderStage::Callable)) {
        flags |= vk::ShaderStageFlagBits::eCallableKHR;
    }
#endif
    return flags;
}

vk::PrimitiveTopology VKConverter::Convert(PrimitiveTopology topology)
{
    switch (topology) {
        case PrimitiveTopology::PointList:
            return vk::PrimitiveTopology::ePointList;
        case PrimitiveTopology::LineList:
            return vk::PrimitiveTopology::eLineList;
        case PrimitiveTopology::LineStrip:
            return vk::PrimitiveTopology::eLineStrip;
        case PrimitiveTopology::TriangleList:
            return vk::PrimitiveTopology::eTriangleList;
        case PrimitiveTopology::TriangleStrip:
            return vk::PrimitiveTopology::eTriangleStrip;
        case PrimitiveTopology::TriangleFan:
            return vk::PrimitiveTopology::eTriangleFan;
        case PrimitiveTopology::PatchList:
            return vk::PrimitiveTopology::ePatchList;
        default:
            throw std::runtime_error("Unknown PrimitiveTopology");
    }
}

vk::CullModeFlags VKConverter::Convert(CullMode cullMode)
{
    switch (cullMode) {
        case CullMode::None:
            return vk::CullModeFlagBits::eNone;
        case CullMode::Front:
            return vk::CullModeFlagBits::eFront;
        case CullMode::Back:
            return vk::CullModeFlagBits::eBack;
        case CullMode::FrontAndBack:
            return vk::CullModeFlagBits::eFrontAndBack;
        default:
            throw std::runtime_error("Unknown CullMode");
    }
}

vk::FrontFace VKConverter::Convert(FrontFace frontFace)
{
    switch (frontFace) {
        case FrontFace::CounterClockwise:
            return vk::FrontFace::eCounterClockwise;
        case FrontFace::Clockwise:
            return vk::FrontFace::eClockwise;
        default:
            throw std::runtime_error("Unknown FrontFace");
    }
}

vk::PolygonMode VKConverter::Convert(PolygonMode polygonMode)
{
    switch (polygonMode) {
        case PolygonMode::Fill:
            return vk::PolygonMode::eFill;
        case PolygonMode::Line:
            return vk::PolygonMode::eLine;
        case PolygonMode::Point:
            return vk::PolygonMode::ePoint;
        default:
            throw std::runtime_error("Unknown PolygonMode");
    }
}

vk::LogicOp VKConverter::Convert(LogicOp logicOp)
{
    switch (logicOp) {
        case LogicOp::Clear:
            return vk::LogicOp::eClear;
        case LogicOp::And:
            return vk::LogicOp::eAnd;
        case LogicOp::AndReverse:
            return vk::LogicOp::eAndReverse;
        case LogicOp::Copy:
            return vk::LogicOp::eCopy;
        case LogicOp::AndInverted:
            return vk::LogicOp::eAndInverted;
        case LogicOp::NoOp:
            return vk::LogicOp::eNoOp;
        case LogicOp::Xor:
            return vk::LogicOp::eXor;
        case LogicOp::Or:
            return vk::LogicOp::eOr;
        case LogicOp::Nor:
            return vk::LogicOp::eNor;
        case LogicOp::Equiv:
            return vk::LogicOp::eEquivalent;
        case LogicOp::Invert:
            return vk::LogicOp::eInvert;
        case LogicOp::OrReverse:
            return vk::LogicOp::eOrReverse;
        case LogicOp::CopyInverted:
            return vk::LogicOp::eCopyInverted;
        case LogicOp::OrInverted:
            return vk::LogicOp::eOrInverted;
        case LogicOp::Nand:
            return vk::LogicOp::eNand;
        case LogicOp::Set:
            return vk::LogicOp::eSet;
        default:
            throw std::runtime_error("Unknown LogicOp");
    }
}

vk::BlendFactor VKConverter::Convert(BlendFactor blendFactor)
{
    switch (blendFactor) {
        case BlendFactor::Zero:
            return vk::BlendFactor::eZero;
        case BlendFactor::One:
            return vk::BlendFactor::eOne;
        case BlendFactor::SrcColor:
            return vk::BlendFactor::eSrcColor;
        case BlendFactor::OneMinusSrcColor:
            return vk::BlendFactor::eOneMinusSrcColor;
        case BlendFactor::DstColor:
            return vk::BlendFactor::eDstColor;
        case BlendFactor::OneMinusDstColor:
            return vk::BlendFactor::eOneMinusDstColor;
        case BlendFactor::SrcAlpha:
            return vk::BlendFactor::eSrcAlpha;
        case BlendFactor::OneMinusSrcAlpha:
            return vk::BlendFactor::eOneMinusSrcAlpha;
        case BlendFactor::DstAlpha:
            return vk::BlendFactor::eDstAlpha;
        case BlendFactor::OneMinusDstAlpha:
            return vk::BlendFactor::eOneMinusDstAlpha;
        case BlendFactor::ConstantColor:
            return vk::BlendFactor::eConstantColor;
        case BlendFactor::OneMinusConstantColor:
            return vk::BlendFactor::eOneMinusConstantColor;
        case BlendFactor::SrcAlphaSaturate:
            return vk::BlendFactor::eSrcAlphaSaturate;
        default:
            throw std::runtime_error("Unknown BlendFactor");
    }
}

vk::BlendOp VKConverter::Convert(BlendOp blendOp)
{
    switch (blendOp) {
        case BlendOp::Add:
            return vk::BlendOp::eAdd;
        case BlendOp::Subtract:
            return vk::BlendOp::eSubtract;
        case BlendOp::ReverseSubtract:
            return vk::BlendOp::eReverseSubtract;
        case BlendOp::Min:
            return vk::BlendOp::eMin;
        case BlendOp::Max:
            return vk::BlendOp::eMax;
        default:
            throw std::runtime_error("Unknown BlendOp");
    }
}

vk::ColorComponentFlags VKConverter::Convert(ColorComponentFlags flags)
{
    vk::ColorComponentFlags vkFlags;
    if (flags & ColorComponentFlags::R) {
        vkFlags |= vk::ColorComponentFlagBits::eR;
    }
    if (flags & ColorComponentFlags::G) {
        vkFlags |= vk::ColorComponentFlagBits::eG;
    }
    if (flags & ColorComponentFlags::B) {
        vkFlags |= vk::ColorComponentFlagBits::eB;
    }
    if (flags & ColorComponentFlags::A) {
        vkFlags |= vk::ColorComponentFlagBits::eA;
    }
    return vkFlags;
}

vk::CompareOp VKConverter::Convert(CompareOp compareOp)
{
    switch (compareOp) {
        case CompareOp::Never:
            return vk::CompareOp::eNever;
        case CompareOp::Less:
            return vk::CompareOp::eLess;
        case CompareOp::Equal:
            return vk::CompareOp::eEqual;
        case CompareOp::LessOrEqual:
            return vk::CompareOp::eLessOrEqual;
        case CompareOp::Greater:
            return vk::CompareOp::eGreater;
        case CompareOp::NotEqual:
            return vk::CompareOp::eNotEqual;
        case CompareOp::GreaterOrEqual:
            return vk::CompareOp::eGreaterOrEqual;
        case CompareOp::Always:
            return vk::CompareOp::eAlways;
        default:
            throw std::runtime_error("Unknown CompareOp");
    }
}

vk::StencilOp VKConverter::Convert(StencilOp stencilOp)
{
    switch (stencilOp) {
        case StencilOp::Keep:
            return vk::StencilOp::eKeep;
        case StencilOp::Zero:
            return vk::StencilOp::eZero;
        case StencilOp::Replace:
            return vk::StencilOp::eReplace;
        case StencilOp::IncrementAndClamp:
            return vk::StencilOp::eIncrementAndClamp;
        case StencilOp::DecrementAndClamp:
            return vk::StencilOp::eDecrementAndClamp;
        case StencilOp::Invert:
            return vk::StencilOp::eInvert;
        case StencilOp::IncrementWrap:
            return vk::StencilOp::eIncrementAndWrap;
        case StencilOp::DecrementWrap:
            return vk::StencilOp::eDecrementAndWrap;
        default:
            throw std::runtime_error("Unknown StencilOp");
    }
}

vk::SampleCountFlagBits VKConverter::ConvertSampleCount(uint32_t sampleCount)
{
    switch (sampleCount) {
        case 1:
            return vk::SampleCountFlagBits::e1;
        case 2:
            return vk::SampleCountFlagBits::e2;
        case 4:
            return vk::SampleCountFlagBits::e4;
        case 8:
            return vk::SampleCountFlagBits::e8;
        case 16:
            return vk::SampleCountFlagBits::e16;
        case 32:
            return vk::SampleCountFlagBits::e32;
        case 64:
            return vk::SampleCountFlagBits::e64;
        default:
            throw std::runtime_error("Unsupported sample count");
    }
}

vk::PipelineStageFlags VKConverter::ConvertSyncScope(SyncScope flags)
{
    vk::PipelineStageFlags vkFlags;
    uint32_t bits = static_cast<uint32_t>(flags);
    if (bits == 0) {
        return vk::PipelineStageFlagBits::eTopOfPipe;
    }
    if (bits & static_cast<uint32_t>(SyncScope::VertexStage)) {
        vkFlags |= vk::PipelineStageFlagBits::eVertexShader | vk::PipelineStageFlagBits::eVertexInput;
    }
    if (bits & static_cast<uint32_t>(SyncScope::FragmentStage)) {
        vkFlags |= vk::PipelineStageFlagBits::eFragmentShader | vk::PipelineStageFlagBits::eColorAttachmentOutput;
    }
    if (bits & static_cast<uint32_t>(SyncScope::ComputeStage)) {
        vkFlags |= vk::PipelineStageFlagBits::eComputeShader;
    }
    if (bits & static_cast<uint32_t>(SyncScope::TransferStage)) {
        vkFlags |= vk::PipelineStageFlagBits::eTransfer;
    }
    if (bits & static_cast<uint32_t>(SyncScope::HostStage)) {
        vkFlags |= vk::PipelineStageFlagBits::eHost;
    }
    if (bits & static_cast<uint32_t>(SyncScope::AllGraphics)) {
        vkFlags |= vk::PipelineStageFlagBits::eAllGraphics;
    }
    if (bits & static_cast<uint32_t>(SyncScope::AllCommands)) {
        vkFlags |= vk::PipelineStageFlagBits::eAllCommands;
    }
    return vkFlags;
}

vk::AccessFlags VKConverter::ConvertResourceStateToAccess(ResourceState state)
{
    auto mapping = VKResourceStateConvert::Convert(state);
    return static_cast<vk::AccessFlags>(mapping.access);
}

vk::ImageLayout VKConverter::ConvertResourceStateToLayout(ResourceState state)
{
    auto mapping = VKResourceStateConvert::Convert(state);
    return static_cast<vk::ImageLayout>(mapping.layout);
}

vk::ImageAspectFlags VKConverter::Convert(ImageAspectFlags flags)
{
    vk::ImageAspectFlags vkFlags;
    if (flags == ImageAspectFlags::None) {
        return vkFlags;
    }
    if (static_cast<uint32_t>(flags) & static_cast<uint32_t>(ImageAspectFlags::Color)) {
        vkFlags |= vk::ImageAspectFlagBits::eColor;
    }
    if (static_cast<uint32_t>(flags) & static_cast<uint32_t>(ImageAspectFlags::Depth)) {
        vkFlags |= vk::ImageAspectFlagBits::eDepth;
    }
    if (static_cast<uint32_t>(flags) & static_cast<uint32_t>(ImageAspectFlags::Stencil)) {
        vkFlags |= vk::ImageAspectFlagBits::eStencil;
    }
    if (static_cast<uint32_t>(flags) & static_cast<uint32_t>(ImageAspectFlags::Metadata)) {
        vkFlags |= vk::ImageAspectFlagBits::eMetadata;
    }
    if (static_cast<uint32_t>(flags) & static_cast<uint32_t>(ImageAspectFlags::Plane0)) {
        vkFlags |= vk::ImageAspectFlagBits::ePlane0;
    }
    if (static_cast<uint32_t>(flags) & static_cast<uint32_t>(ImageAspectFlags::Plane1)) {
        vkFlags |= vk::ImageAspectFlagBits::ePlane1;
    }
    if (static_cast<uint32_t>(flags) & static_cast<uint32_t>(ImageAspectFlags::Plane2)) {
        vkFlags |= vk::ImageAspectFlagBits::ePlane2;
    }
    return vkFlags;
}

vk::Filter VKConverter::Convert(Filter filter)
{
    switch (filter) {
        case Filter::Nearest:
            return vk::Filter::eNearest;
        case Filter::Linear:
            return vk::Filter::eLinear;
        default:
            return vk::Filter::eLinear;
    }
}

vk::SamplerAddressMode VKConverter::Convert(SamplerAddressMode addressMode)
{
    switch (addressMode) {
        case SamplerAddressMode::Repeat:
            return vk::SamplerAddressMode::eRepeat;
        case SamplerAddressMode::MirroredRepeat:
            return vk::SamplerAddressMode::eMirroredRepeat;
        case SamplerAddressMode::ClampToEdge:
            return vk::SamplerAddressMode::eClampToEdge;
        case SamplerAddressMode::ClampToBorder:
            return vk::SamplerAddressMode::eClampToBorder;
        case SamplerAddressMode::MirrorClampToEdge:
            return vk::SamplerAddressMode::eMirrorClampToEdge;
        default:
            return vk::SamplerAddressMode::eRepeat;
    }
}

vk::SamplerMipmapMode VKConverter::Convert(SamplerMipmapMode mipmapMode)
{
    switch (mipmapMode) {
        case SamplerMipmapMode::Nearest:
            return vk::SamplerMipmapMode::eNearest;
        case SamplerMipmapMode::Linear:
            return vk::SamplerMipmapMode::eLinear;
        default:
            return vk::SamplerMipmapMode::eLinear;
    }
}

vk::BorderColor VKConverter::Convert(BorderColor borderColor)
{
    switch (borderColor) {
        case BorderColor::FloatTransparentBlack:
            return vk::BorderColor::eFloatTransparentBlack;
        case BorderColor::IntTransparentBlack:
            return vk::BorderColor::eIntTransparentBlack;
        case BorderColor::FloatOpaqueBlack:
            return vk::BorderColor::eFloatOpaqueBlack;
        case BorderColor::IntOpaqueBlack:
            return vk::BorderColor::eIntOpaqueBlack;
        case BorderColor::FloatOpaqueWhite:
            return vk::BorderColor::eFloatOpaqueWhite;
        case BorderColor::IntOpaqueWhite:
            return vk::BorderColor::eIntOpaqueWhite;
        default:
            return vk::BorderColor::eFloatOpaqueBlack;
    }
}

vk::DescriptorType VKConverter::Convert(DescriptorType type)
{
    switch (type) {
        case DescriptorType::Sampler:
            return vk::DescriptorType::eSampler;
        case DescriptorType::CombinedImageSampler:
            return vk::DescriptorType::eCombinedImageSampler;
        case DescriptorType::SampledImage:
            return vk::DescriptorType::eSampledImage;
        case DescriptorType::StorageImage:
            return vk::DescriptorType::eStorageImage;
        case DescriptorType::UniformBuffer:
            return vk::DescriptorType::eUniformBuffer;
        case DescriptorType::StorageBuffer:
            return vk::DescriptorType::eStorageBuffer;
        case DescriptorType::UniformBufferDynamic:
            return vk::DescriptorType::eUniformBufferDynamic;
        case DescriptorType::StorageBufferDynamic:
            return vk::DescriptorType::eStorageBufferDynamic;
        case DescriptorType::InputAttachment:
            return vk::DescriptorType::eInputAttachment;
        case DescriptorType::AccelerationStructure:
            return vk::DescriptorType::eAccelerationStructureKHR;
        default:
            return vk::DescriptorType::eSampler;
    }
}

vk::IndexType VKConverter::Convert(IndexType indexType)
{
    switch (indexType) {
        case IndexType::UInt16:
            return vk::IndexType::eUint16;
        case IndexType::UInt32:
            return vk::IndexType::eUint32;
        default:
            throw std::runtime_error("Unsupported index type");
    }
}
} // namespace Cacao

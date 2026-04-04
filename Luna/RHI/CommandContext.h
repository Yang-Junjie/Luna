#pragma once

#include "Descriptors.h"

#include <cstdint>
#include <span>

namespace luna {

struct ClearColorValue {
    float r = 0.0f;
    float g = 0.0f;
    float b = 0.0f;
    float a = 1.0f;
};

struct DrawArguments {
    uint32_t vertexCount = 0;
    uint32_t instanceCount = 1;
    uint32_t firstVertex = 0;
    uint32_t firstInstance = 0;
};

enum class IndexFormat : uint32_t {
    UInt16 = 0,
    UInt32
};

struct IndexedDrawArguments {
    uint32_t indexCount = 0;
    uint32_t instanceCount = 1;
    uint32_t firstIndex = 0;
    int32_t vertexOffset = 0;
    uint32_t firstInstance = 0;
};

enum class PipelineStage : uint32_t {
    None = 0,
    Top = 1u << 0,
    DrawIndirect = 1u << 1,
    VertexInput = 1u << 2,
    VertexShader = 1u << 3,
    FragmentShader = 1u << 4,
    ComputeShader = 1u << 5,
    ColorAttachmentOutput = 1u << 6,
    Transfer = 1u << 7,
    Host = 1u << 8,
    Bottom = 1u << 9,
    AllGraphics = (1u << 3) | (1u << 4) | (1u << 6),
    AllCommands = 0xFFFF'FFFFu
};

inline PipelineStage operator|(PipelineStage lhs, PipelineStage rhs)
{
    return static_cast<PipelineStage>(static_cast<uint32_t>(lhs) | static_cast<uint32_t>(rhs));
}

enum class ResourceAccess : uint32_t {
    None = 0,
    IndirectCommandRead = 1u << 0,
    VertexBufferRead = 1u << 1,
    IndexBufferRead = 1u << 2,
    UniformRead = 1u << 3,
    ShaderRead = 1u << 4,
    ShaderWrite = 1u << 5,
    ColorAttachmentRead = 1u << 6,
    ColorAttachmentWrite = 1u << 7,
    DepthStencilRead = 1u << 8,
    DepthStencilWrite = 1u << 9,
    TransferRead = 1u << 10,
    TransferWrite = 1u << 11,
    HostRead = 1u << 12,
    HostWrite = 1u << 13,
    MemoryRead = 1u << 14,
    MemoryWrite = 1u << 15
};

inline ResourceAccess operator|(ResourceAccess lhs, ResourceAccess rhs)
{
    return static_cast<ResourceAccess>(static_cast<uint32_t>(lhs) | static_cast<uint32_t>(rhs));
}

enum class ImageLayout : uint32_t {
    Undefined = 0,
    General,
    ColorAttachment,
    DepthStencilAttachment,
    TransferSrc,
    TransferDst,
    ShaderReadOnly,
    Present
};

struct ImageCopyInfo {
    ImageHandle source{};
    ImageHandle destination{};
    uint32_t sourceWidth = 0;
    uint32_t sourceHeight = 0;
    uint32_t destinationWidth = 0;
    uint32_t destinationHeight = 0;
};

struct ImageBarrierInfo {
    ImageHandle image{};
    ImageLayout oldLayout = ImageLayout::Undefined;
    ImageLayout newLayout = ImageLayout::Undefined;
    PipelineStage srcStage = PipelineStage::None;
    PipelineStage dstStage = PipelineStage::None;
    ResourceAccess srcAccess = ResourceAccess::None;
    ResourceAccess dstAccess = ResourceAccess::None;
    ImageAspect aspect = ImageAspect::Color;
    uint32_t baseMipLevel = 0;
    uint32_t mipCount = 1;
    uint32_t baseArrayLayer = 0;
    uint32_t layerCount = 1;
};

struct BufferBarrierInfo {
    BufferHandle buffer{};
    PipelineStage srcStage = PipelineStage::None;
    PipelineStage dstStage = PipelineStage::None;
    ResourceAccess srcAccess = ResourceAccess::None;
    ResourceAccess dstAccess = ResourceAccess::None;
    uint64_t offset = 0;
    uint64_t size = 0;
};

struct BufferCopyInfo {
    BufferHandle source{};
    BufferHandle destination{};
    uint64_t sourceOffset = 0;
    uint64_t destinationOffset = 0;
    uint64_t size = 0;
};

struct BufferImageCopyInfo {
    BufferHandle buffer{};
    ImageHandle image{};
    uint64_t bufferOffset = 0;
    uint32_t bufferRowLength = 0;
    uint32_t bufferImageHeight = 0;
    ImageAspect aspect = ImageAspect::Color;
    uint32_t mipLevel = 0;
    uint32_t baseArrayLayer = 0;
    uint32_t layerCount = 1;
    uint32_t imageOffsetX = 0;
    uint32_t imageOffsetY = 0;
    uint32_t imageOffsetZ = 0;
    uint32_t imageExtentWidth = 0;
    uint32_t imageExtentHeight = 0;
    uint32_t imageExtentDepth = 1;
};

struct ColorAttachmentInfo {
    ImageHandle image{};
    PixelFormat format = PixelFormat::Undefined;
    ClearColorValue clearColor{};
};

struct DepthAttachmentInfo {
    ImageHandle image{};
    PixelFormat format = PixelFormat::Undefined;
    float clearDepth = 1.0f;
};

struct RenderingInfo {
    uint32_t width = 0;
    uint32_t height = 0;
    std::vector<ColorAttachmentInfo> colorAttachments;
    DepthAttachmentInfo depthAttachment{};
};

class IRHICommandContext {
public:
    virtual ~IRHICommandContext() = default;

    virtual RHIResult beginRendering(const RenderingInfo& renderingInfo) = 0;
    virtual RHIResult endRendering() = 0;
    virtual RHIResult clearColor(const ClearColorValue& color) = 0;
    virtual RHIResult imageBarrier(const ImageBarrierInfo& barrierInfo) = 0;
    virtual RHIResult bufferBarrier(const BufferBarrierInfo& barrierInfo) = 0;
    virtual RHIResult transitionImage(ImageHandle image, ImageLayout newLayout) = 0;
    virtual RHIResult copyBuffer(const BufferCopyInfo& copyInfo) = 0;
    virtual RHIResult copyImage(const ImageCopyInfo& copyInfo) = 0;
    virtual RHIResult copyBufferToImage(const BufferImageCopyInfo& copyInfo) = 0;
    virtual RHIResult copyImageToBuffer(const BufferImageCopyInfo& copyInfo) = 0;
    virtual RHIResult bindGraphicsPipeline(PipelineHandle pipeline) = 0;
    virtual RHIResult bindComputePipeline(PipelineHandle pipeline) = 0;
    virtual RHIResult bindVertexBuffer(BufferHandle buffer, uint64_t offset = 0) = 0;
    virtual RHIResult bindIndexBuffer(BufferHandle buffer, IndexFormat indexFormat, uint64_t offset = 0) = 0;
    virtual RHIResult bindResourceSet(ResourceSetHandle resourceSet, std::span<const uint32_t> dynamicOffsets = {}) = 0;
    virtual RHIResult pushConstants(const void* data,
                                    uint32_t size,
                                    uint32_t offset = 0,
                                    ShaderType visibility = ShaderType::AllGraphics) = 0;
    virtual RHIResult draw(const DrawArguments& arguments) = 0;
    virtual RHIResult drawIndexed(const IndexedDrawArguments& arguments) = 0;
    virtual RHIResult dispatch(uint32_t groupCountX, uint32_t groupCountY = 1, uint32_t groupCountZ = 1) = 0;
    virtual RHIResult dispatchIndirect(BufferHandle argumentsBuffer, uint64_t offset = 0) = 0;
};

} // namespace luna

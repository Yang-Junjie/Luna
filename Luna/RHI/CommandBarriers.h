#pragma once

#include "CommandTypes.h"
#include "Descriptors.h"

#include <cstdint>

namespace luna {

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

} // namespace luna

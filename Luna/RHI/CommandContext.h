#pragma once

#include "Descriptors.h"

#include <cstdint>

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
    virtual RHIResult transitionImage(ImageHandle image, ImageLayout newLayout) = 0;
    virtual RHIResult copyImage(const ImageCopyInfo& copyInfo) = 0;
    virtual RHIResult bindGraphicsPipeline(PipelineHandle pipeline) = 0;
    virtual RHIResult bindComputePipeline(PipelineHandle pipeline) = 0;
    virtual RHIResult bindVertexBuffer(BufferHandle buffer, uint64_t offset = 0) = 0;
    virtual RHIResult bindIndexBuffer(BufferHandle buffer, IndexFormat indexFormat, uint64_t offset = 0) = 0;
    virtual RHIResult bindResourceSet(ResourceSetHandle resourceSet) = 0;
    virtual RHIResult pushConstants(const void* data,
                                    uint32_t size,
                                    uint32_t offset = 0,
                                    ShaderType visibility = ShaderType::AllGraphics) = 0;
    virtual RHIResult draw(const DrawArguments& arguments) = 0;
    virtual RHIResult drawIndexed(const IndexedDrawArguments& arguments) = 0;
    virtual RHIResult dispatch(uint32_t groupCountX, uint32_t groupCountY = 1, uint32_t groupCountZ = 1) = 0;
};

} // namespace luna

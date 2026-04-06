#pragma once

#include "CommandTypes.h"
#include "Descriptors.h"

#include <vector>

namespace luna {

struct ColorAttachmentInfo {
    ImageHandle image{};
    PixelFormat format = PixelFormat::Undefined;
    ClearColorValue clearColor{};
    ImageViewHandle view{};
    AttachmentLoadOp loadOp = AttachmentLoadOp::Clear;
    AttachmentStoreOp storeOp = AttachmentStoreOp::Store;
};

struct DepthAttachmentInfo {
    ImageHandle image{};
    PixelFormat format = PixelFormat::Undefined;
    float clearDepth = 1.0f;
    ImageViewHandle view{};
    AttachmentLoadOp loadOp = AttachmentLoadOp::Clear;
    AttachmentStoreOp storeOp = AttachmentStoreOp::Store;
};

struct RenderingInfo {
    uint32_t width = 0;
    uint32_t height = 0;
    std::vector<ColorAttachmentInfo> colorAttachments;
    DepthAttachmentInfo depthAttachment{};
};

} // namespace luna

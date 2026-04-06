#pragma once

#include "CommandContext.h"
#include "Descriptors.h"
#include "ResourceLayout.h"
#include "Shader.h"

#include <cstdint>
#include <string_view>

namespace luna {

struct DeviceCreateInfo {
    std::string_view applicationName = "Luna";
    RHIBackend backend = RHIBackend::Vulkan;
    uint64_t adapterId = 0;
    void* nativeWindow = nullptr;
    SwapchainDesc swapchain;
    bool enableValidation = false;
};

struct RHICapabilities {
    RHIBackend backend = RHIBackend::Vulkan;
    bool implemented = false;
    bool supportsGraphics = false;
    bool supportsPresent = false;
    bool supportsDynamicRendering = false;
    bool supportsIndexedDraw = false;
    bool supportsResourceSets = false;
    uint32_t framesInFlight = 0;
};

struct RHIDeviceLimits {
    uint32_t framesInFlight = 0;
    uint32_t minUniformBufferOffsetAlignment = 0;
    uint32_t maxColorAttachments = 0;
    uint32_t maxImageArrayLayers = 0;
};

struct RHIFormatSupport {
    PixelFormat format = PixelFormat::Undefined;
    std::string_view backendFormatName = "Undefined";
    bool sampled = false;
    bool colorAttachment = false;
    bool depthStencilAttachment = false;
    bool storage = false;
};

struct FrameContext {
    uint32_t frameIndex = 0;
    uint32_t renderWidth = 0;
    uint32_t renderHeight = 0;
    ImageHandle backbuffer{};
    PixelFormat backbufferFormat = PixelFormat::Undefined;
    IRHICommandContext* commandContext = nullptr;
};

} // namespace luna

#pragma once

#include "RHIDeviceTypes.h"

namespace luna {

struct RHISwapchainState {
    bool valid = false;
    uint64_t deviceId = 0;
    uint64_t swapchainId = 0;
    SwapchainDesc desc{};
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t imageCount = 0;
    PixelFormat currentFormat = PixelFormat::Undefined;
    bool vsyncActive = true;
    std::string_view presentModeName = "Unavailable";
};

class IRHISurface {
public:
    virtual ~IRHISurface() = default;

    virtual RHIBackend getBackend() const = 0;
    virtual uint64_t getSurfaceId() const = 0;
};

class IRHISwapchain {
public:
    virtual ~IRHISwapchain() = default;

    virtual RHIBackend getBackend() const = 0;
    virtual SwapchainHandle getHandle() const = 0;
    virtual RHISwapchainState getState() const = 0;
    virtual IRHISurface* getSurface() const = 0;
    virtual RHIResult requestRecreate() = 0;
};

} // namespace luna

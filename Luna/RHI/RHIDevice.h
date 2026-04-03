#pragma once

#include "Descriptors.h"

#include <memory>
#include <string_view>

namespace luna {

struct DeviceCreateInfo {
    std::string_view applicationName = "Luna";
    RHIBackend backend = RHIBackend::Vulkan;
    void* nativeWindow = nullptr;
    SwapchainDesc swapchain;
    bool enableValidation = false;
};

class IRHIDevice {
public:
    virtual ~IRHIDevice() = default;

    virtual RHIBackend getBackend() const = 0;
    virtual RHIResult init(const DeviceCreateInfo& createInfo) = 0;
    virtual void shutdown() = 0;
    virtual RHIResult beginFrame() = 0;
    virtual RHIResult endFrame() = 0;
};

std::unique_ptr<IRHIDevice> CreateRHIDevice(RHIBackend backend);

} // namespace luna

#pragma once

#include "RHIDeviceTypes.h"

#include <memory>
#include <string>

namespace luna {

class IRHIDevice;

struct RHIAdapterInfo {
    uint64_t adapterId = 0;
    RHIBackend backend = RHIBackend::Vulkan;
    std::string name;
    RHICapabilities capabilities{};
    RHIDeviceLimits limits{};
};

class IRHIAdapter {
public:
    virtual ~IRHIAdapter() = default;

    virtual RHIAdapterInfo getInfo() const = 0;
    virtual RHIResult createDevice(const DeviceCreateInfo& createInfo, std::unique_ptr<IRHIDevice>* outDevice) const = 0;
};

} // namespace luna

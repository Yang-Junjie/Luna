#include "RHI/RHIDevice.h"

namespace luna {
namespace {

class VulkanStubDevice final : public IRHIDevice {
public:
    RHIBackend getBackend() const override
    {
        return RHIBackend::Vulkan;
    }

    RHIResult init(const DeviceCreateInfo& createInfo) override
    {
        if (m_initialized) {
            return RHIResult::InvalidArgument;
        }

        m_createInfo = createInfo;
        m_initialized = true;
        return RHIResult::Success;
    }

    void shutdown() override
    {
        m_initialized = false;
    }

    RHIResult beginFrame() override
    {
        return m_initialized ? RHIResult::Success : RHIResult::NotReady;
    }

    RHIResult endFrame() override
    {
        return m_initialized ? RHIResult::Success : RHIResult::NotReady;
    }

private:
    bool m_initialized = false;
    DeviceCreateInfo m_createInfo{};
};

} // namespace

std::unique_ptr<IRHIDevice> CreateRHIDevice(RHIBackend backend)
{
    switch (backend) {
        case RHIBackend::Vulkan:
            return std::make_unique<VulkanStubDevice>();
        default:
            return nullptr;
    }
}

} // namespace luna

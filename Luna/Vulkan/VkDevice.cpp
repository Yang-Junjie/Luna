#include "VkDevice.h"

#include "VkPhysicalDevice.h"

#include <Core/Log.h>

#include <utility>

namespace {

template <typename T> void logVkbError(const char* step, const vkb::Result<T>& result)
{
    LUNA_CORE_ERROR("{} failed: {}", step, result.error().message());
    for (const std::string& reason : result.full_error().detailed_failure_reasons) {
        LUNA_CORE_ERROR("  {}", reason);
    }
}

} // namespace

namespace luna::vkcore {

Device::~Device()
{
    destroy();
}

Device::Device(Device&& other) noexcept
{
    *this = std::move(other);
}

Device& Device::operator=(Device&& other) noexcept
{
    if (this == &other) {
        return *this;
    }

    destroy();

    m_vkb_device = other.m_vkb_device;
    m_device = other.m_device;

    other.m_vkb_device = {};
    other.m_device = VK_NULL_HANDLE;
    return *this;
}

bool Device::create(const PhysicalDevice& physical_device)
{
    destroy();

    if (!physical_device.isValid()) {
        LUNA_CORE_ERROR("Cannot create a logical device without a selected physical device");
        return false;
    }

    vkb::DeviceBuilder device_builder{physical_device.getBootstrapPhysicalDevice()};
    auto device_ret = device_builder.build();
    if (!device_ret) {
        logVkbError("Logical device creation", device_ret);
        return false;
    }

    m_vkb_device = device_ret.value();
    m_device = m_vkb_device.device;
    return true;
}

void Device::destroy()
{
    if (m_device != VK_NULL_HANDLE) {
        vkDestroyDevice(m_device, nullptr);
    }

    m_device = VK_NULL_HANDLE;
    m_vkb_device = {};
}

} // namespace luna::vkcore

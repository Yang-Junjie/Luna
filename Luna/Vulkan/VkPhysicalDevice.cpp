#include "VkPhysicalDevice.h"

#include "VkInstance.h"

#include <Core/Log.h>

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

bool PhysicalDevice::select(const Instance& instance,
                            const VkPhysicalDeviceVulkan13Features& features13,
                            const VkPhysicalDeviceVulkan12Features& features12)
{
    reset();

    if (!instance.isValid()) {
        LUNA_CORE_ERROR("Cannot select a physical device before creating a Vulkan instance");
        return false;
    }

    vkb::PhysicalDeviceSelector selector{instance.getBootstrapInstance()};
    auto physical_device_ret = selector.set_minimum_version(1, 3)
                                   .set_required_features_13(features13)
                                   .set_required_features_12(features12)
                                   .set_surface(instance.getSurface())
                                   .select();
    if (!physical_device_ret) {
        logVkbError("Physical device selection", physical_device_ret);
        return false;
    }

    m_vkb_physical_device = physical_device_ret.value();
    m_physical_device = m_vkb_physical_device.physical_device;

    VkPhysicalDeviceProperties properties{};
    vkGetPhysicalDeviceProperties(m_physical_device, &properties);
    m_properties = properties;
    return true;
}

void PhysicalDevice::reset()
{
    m_vkb_physical_device = {};
    m_physical_device = VK_NULL_HANDLE;
    m_properties = {};
}

} // namespace luna::vkcore

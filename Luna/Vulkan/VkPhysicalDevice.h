#pragma once

#include "VkBootstrap.h"
#include "VkTypes.h"

namespace luna::vkcore {

class Instance;

class PhysicalDevice {
public:
    bool select(const Instance& instance,
                const VkPhysicalDeviceVulkan13Features& features13,
                const VkPhysicalDeviceVulkan12Features& features12);
    void reset();

    bool isValid() const
    {
        return m_physical_device != VK_NULL_HANDLE;
    }

    vk::PhysicalDevice get() const
    {
        return m_physical_device;
    }

    const vk::PhysicalDeviceProperties& getProperties() const
    {
        return m_properties;
    }

    const vkb::PhysicalDevice& getBootstrapPhysicalDevice() const
    {
        return m_vkb_physical_device;
    }

private:
    vkb::PhysicalDevice m_vkb_physical_device{};
    vk::PhysicalDevice m_physical_device{};
    vk::PhysicalDeviceProperties m_properties{};
};

} // namespace luna::vkcore

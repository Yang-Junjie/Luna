#pragma once

#include "VkBootstrap.h"
#include "VkTypes.h"

namespace luna::vkcore {

class PhysicalDevice;

class Device {
public:
    Device() = default;
    ~Device();

    Device(const Device&) = delete;
    Device& operator=(const Device&) = delete;

    Device(Device&& other) noexcept;
    Device& operator=(Device&& other) noexcept;

    bool create(const PhysicalDevice& physical_device);
    void destroy();

    bool isValid() const
    {
        return m_device != VK_NULL_HANDLE;
    }

    vk::Device get() const
    {
        return m_device;
    }

    const vkb::Device& getBootstrapDevice() const
    {
        return m_vkb_device;
    }

private:
    vkb::Device m_vkb_device{};
    vk::Device m_device{};
};

} // namespace luna::vkcore

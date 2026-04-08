#pragma once

#include "VkBootstrap.h"
#include "VkTypes.h"

namespace luna::vkcore {

class Device;

class Queue {
public:
    bool initialize(const Device& device, vkb::QueueType queue_type);
    void reset();

    bool isValid() const
    {
        return m_queue != VK_NULL_HANDLE;
    }

    vk::Queue get() const
    {
        return m_queue;
    }

    uint32_t getFamilyIndex() const
    {
        return m_family_index;
    }

    vkb::QueueType getType() const
    {
        return m_type;
    }

private:
    vk::Queue m_queue{};
    uint32_t m_family_index{0};
    vkb::QueueType m_type{vkb::QueueType::graphics};
};

} // namespace luna::vkcore

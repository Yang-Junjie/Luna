#pragma once

#include "VkTypes.h"
#include "VkVMAAllocator.h"

#include <optional>

namespace luna::vkcore {

struct TransientAllocation {
    vk::Buffer m_buffer{};
    vk::DeviceSize m_offset{0};
    vk::DeviceSize m_size{0};
    void* m_mapped_data{nullptr};

    bool isValid() const
    {
        return m_buffer != VK_NULL_HANDLE;
    }
};

class TransientArena {
public:
    bool init(const VMAAllocator& allocator,
              vk::DeviceSize capacity,
              vk::BufferUsageFlags usage,
              VmaMemoryUsage memory_usage,
              VmaAllocationCreateFlags allocation_flags = VMA_ALLOCATION_CREATE_MAPPED_BIT);
    void reset();
    void destroy();

    bool isValid() const
    {
        return m_buffer.isValid();
    }

    vk::DeviceSize getCapacity() const
    {
        return m_capacity;
    }

    vk::DeviceSize getUsedSize() const
    {
        return m_head;
    }

    std::optional<TransientAllocation> allocate(vk::DeviceSize size, vk::DeviceSize alignment = 1);

private:
    AllocatedBuffer m_buffer{};
    vk::DeviceSize m_capacity{0};
    vk::DeviceSize m_head{0};
};

} // namespace luna::vkcore

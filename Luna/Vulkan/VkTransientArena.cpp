#include "VkTransientArena.h"

#include <algorithm>
#include <cstddef>

namespace {

vk::DeviceSize alignUp(vk::DeviceSize value, vk::DeviceSize alignment)
{
    const vk::DeviceSize safe_alignment = std::max<vk::DeviceSize>(1, alignment);
    return (value + safe_alignment - 1) & ~(safe_alignment - 1);
}

} // namespace

namespace luna::vkcore {

bool TransientArena::init(const VMAAllocator& allocator,
                          vk::DeviceSize capacity,
                          vk::BufferUsageFlags usage,
                          VmaMemoryUsage memory_usage,
                          VmaAllocationCreateFlags allocation_flags)
{
    destroy();

    if (!allocator.isValid() || capacity == 0) {
        return false;
    }

    m_buffer = allocator.createBuffer(static_cast<size_t>(capacity), usage, memory_usage, allocation_flags);
    if (!m_buffer.isValid()) {
        destroy();
        return false;
    }

    m_capacity = capacity;
    m_head = 0;
    return true;
}

void TransientArena::reset()
{
    m_head = 0;
}

void TransientArena::destroy()
{
    m_buffer.reset();
    m_capacity = 0;
    m_head = 0;
}

std::optional<TransientAllocation> TransientArena::allocate(vk::DeviceSize size, vk::DeviceSize alignment)
{
    if (!m_buffer.isValid() || size == 0) {
        return std::nullopt;
    }

    const vk::DeviceSize aligned_offset = alignUp(m_head, alignment);
    if (aligned_offset + size > m_capacity) {
        return std::nullopt;
    }

    TransientAllocation allocation{};
    allocation.m_buffer = m_buffer.get();
    allocation.m_offset = aligned_offset;
    allocation.m_size = size;

    if (void* mapped_data = m_buffer.getMappedData()) {
        auto* byte_data = static_cast<std::byte*>(mapped_data);
        allocation.m_mapped_data = byte_data + static_cast<size_t>(aligned_offset);
    }

    m_head = aligned_offset + size;
    return allocation;
}

} // namespace luna::vkcore

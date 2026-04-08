#include "VkBuffer.h"

#include <utility>

namespace luna::vkcore {

Buffer::~Buffer()
{
    reset();
}

Buffer::Buffer(Buffer&& other) noexcept
{
    *this = std::move(other);
}

Buffer& Buffer::operator=(Buffer&& other) noexcept
{
    if (this == &other) {
        return *this;
    }

    reset();

    m_handle = other.m_handle;
    m_allocation = other.m_allocation;
    m_info = other.m_info;
    m_allocator = other.m_allocator;

    other.m_handle = VK_NULL_HANDLE;
    other.m_allocation = VK_NULL_HANDLE;
    other.m_info = {};
    other.m_allocator = VK_NULL_HANDLE;
    return *this;
}

void Buffer::assign(vk::Buffer handle, VmaAllocation allocation, const VmaAllocationInfo& info, VmaAllocator allocator)
{
    reset();
    m_handle = handle;
    m_allocation = allocation;
    m_info = info;
    m_allocator = allocator;
}

void Buffer::reset()
{
    if (m_allocator != VK_NULL_HANDLE && m_handle != VK_NULL_HANDLE && m_allocation != VK_NULL_HANDLE) {
        vmaDestroyBuffer(m_allocator, static_cast<VkBuffer>(m_handle), m_allocation);
    }

    m_handle = VK_NULL_HANDLE;
    m_allocation = VK_NULL_HANDLE;
    m_info = {};
    m_allocator = VK_NULL_HANDLE;
}

} // namespace luna::vkcore

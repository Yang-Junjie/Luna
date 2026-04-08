#include "VkImage.h"

#include <utility>

namespace luna::vkcore {

Image::~Image()
{
    reset();
}

Image::Image(Image&& other) noexcept
{
    *this = std::move(other);
}

Image& Image::operator=(Image&& other) noexcept
{
    if (this == &other) {
        return *this;
    }

    reset();

    m_handle = other.m_handle;
    m_view = std::move(other.m_view);
    m_allocation = other.m_allocation;
    m_allocator = other.m_allocator;
    m_extent = other.m_extent;
    m_format = other.m_format;
    m_mip_levels = other.m_mip_levels;

    other.m_handle = VK_NULL_HANDLE;
    other.m_allocation = VK_NULL_HANDLE;
    other.m_allocator = VK_NULL_HANDLE;
    other.m_extent = {};
    other.m_format = vk::Format::eUndefined;
    other.m_mip_levels = 1;
    return *this;
}

void Image::assign(vk::Image handle,
                   VmaAllocation allocation,
                   VmaAllocator allocator,
                   vk::Extent3D extent,
                   vk::Format format,
                   uint32_t mip_levels)
{
    reset();
    m_handle = handle;
    m_allocation = allocation;
    m_allocator = allocator;
    m_extent = extent;
    m_format = format;
    m_mip_levels = mip_levels;
}

void Image::reset()
{
    m_view.reset();

    if (m_allocator != VK_NULL_HANDLE && m_handle != VK_NULL_HANDLE && m_allocation != VK_NULL_HANDLE) {
        vmaDestroyImage(m_allocator, static_cast<VkImage>(m_handle), m_allocation);
    }

    m_handle = VK_NULL_HANDLE;
    m_allocation = VK_NULL_HANDLE;
    m_allocator = VK_NULL_HANDLE;
    m_extent = {};
    m_format = vk::Format::eUndefined;
    m_mip_levels = 1;
}

} // namespace luna::vkcore

#pragma once

#include "VkImageView.h"

#include <vk_mem_alloc.h>

namespace luna::vkcore {

class VMAAllocator;

class Image {
public:
    Image() = default;
    ~Image();

    Image(const Image&) = delete;
    Image& operator=(const Image&) = delete;

    Image(Image&& other) noexcept;
    Image& operator=(Image&& other) noexcept;

    bool isValid() const
    {
        return m_handle != VK_NULL_HANDLE;
    }

    vk::Image get() const
    {
        return m_handle;
    }

    const ImageView& getViewObject() const
    {
        return m_view;
    }

    ImageView& getViewObject()
    {
        return m_view;
    }

    vk::ImageView getView() const
    {
        return m_view.get();
    }

    VmaAllocation getAllocation() const
    {
        return m_allocation;
    }

    vk::Extent3D getExtent() const
    {
        return m_extent;
    }

    vk::Format getFormat() const
    {
        return m_format;
    }

    uint32_t getMipLevels() const
    {
        return m_mip_levels;
    }

    void reset();

    operator vk::Image() const
    {
        return m_handle;
    }

    bool operator==(const Image& other) const
    {
        return m_handle == other.m_handle;
    }

private:
    friend class VMAAllocator;

    void assign(vk::Image handle,
                VmaAllocation allocation,
                VmaAllocator allocator,
                vk::Extent3D extent,
                vk::Format format,
                uint32_t mip_levels);

    vk::Image m_handle{};
    ImageView m_view{};
    VmaAllocation m_allocation{VK_NULL_HANDLE};
    VmaAllocator m_allocator{VK_NULL_HANDLE};
    vk::Extent3D m_extent{};
    vk::Format m_format{vk::Format::eUndefined};
    uint32_t m_mip_levels{1};
};

} // namespace luna::vkcore

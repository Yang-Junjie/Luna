#pragma once

#include "VkImage.h"

namespace luna::vkcore {

class VMAAllocator;

class RenderTarget {
public:
    bool create(const VMAAllocator& allocator, vk::Extent2D extent, vk::Format color_format, vk::Format depth_format);
    bool resize(const VMAAllocator& allocator, vk::Extent2D extent, vk::Format color_format, vk::Format depth_format);
    void destroy();

    bool isValid() const
    {
        return m_color_image.isValid() && m_depth_image.isValid();
    }

    const Image& getColorImage() const
    {
        return m_color_image;
    }

    Image& getColorImage()
    {
        return m_color_image;
    }

    const Image& getDepthImage() const
    {
        return m_depth_image;
    }

    Image& getDepthImage()
    {
        return m_depth_image;
    }

    vk::Extent2D getExtent() const
    {
        return m_extent;
    }

    vk::Format getColorFormat() const
    {
        return m_color_image.getFormat();
    }

    vk::Format getDepthFormat() const
    {
        return m_depth_image.getFormat();
    }

    vk::ImageLayout getColorLayout() const
    {
        return m_color_layout;
    }

    void setColorLayout(vk::ImageLayout layout)
    {
        m_color_layout = layout;
    }

    vk::ImageLayout getDepthLayout() const
    {
        return m_depth_layout;
    }

    void setDepthLayout(vk::ImageLayout layout)
    {
        m_depth_layout = layout;
    }

private:
    Image m_color_image{};
    Image m_depth_image{};
    vk::Extent2D m_extent{};
    vk::ImageLayout m_color_layout{vk::ImageLayout::eUndefined};
    vk::ImageLayout m_depth_layout{vk::ImageLayout::eUndefined};
};

} // namespace luna::vkcore

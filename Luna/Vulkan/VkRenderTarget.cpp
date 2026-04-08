#include "VkRenderTarget.h"

#include "VkVMAAllocator.h"

namespace luna::vkcore {

bool RenderTarget::create(const VMAAllocator& allocator,
                          vk::Extent2D extent,
                          vk::Format color_format,
                          vk::Format depth_format)
{
    destroy();

    if (!allocator.isValid() || extent.width == 0 || extent.height == 0) {
        return false;
    }

    const vk::Extent3D image_extent{extent.width, extent.height, 1};
    m_color_image = allocator.createImage(image_extent,
                                          color_format,
                                          vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst |
                                              vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eColorAttachment);
    m_depth_image = allocator.createImage(image_extent, depth_format, vk::ImageUsageFlagBits::eDepthStencilAttachment);

    if (!m_color_image.isValid() || !m_depth_image.isValid()) {
        destroy();
        return false;
    }

    m_extent = extent;
    m_color_layout = vk::ImageLayout::eUndefined;
    m_depth_layout = vk::ImageLayout::eUndefined;
    return true;
}

bool RenderTarget::resize(const VMAAllocator& allocator,
                          vk::Extent2D extent,
                          vk::Format color_format,
                          vk::Format depth_format)
{
    return create(allocator, extent, color_format, depth_format);
}

void RenderTarget::destroy()
{
    m_color_image.reset();
    m_depth_image.reset();
    m_extent = {};
    m_color_layout = vk::ImageLayout::eUndefined;
    m_depth_layout = vk::ImageLayout::eUndefined;
}

} // namespace luna::vkcore

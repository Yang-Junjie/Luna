#include "VkImageView.h"

#include "VkInitializers.h"

#include <utility>

namespace luna::vkcore {

ImageView::~ImageView()
{
    reset();
}

ImageView::ImageView(ImageView&& other) noexcept
{
    *this = std::move(other);
}

ImageView& ImageView::operator=(ImageView&& other) noexcept
{
    if (this == &other) {
        return *this;
    }

    reset();

    m_handle = other.m_handle;
    m_device = other.m_device;

    other.m_handle = VK_NULL_HANDLE;
    other.m_device = VK_NULL_HANDLE;
    return *this;
}

ImageView ImageView::fromHandle(vk::Device device, vk::ImageView handle)
{
    ImageView view;
    view.assign(device, handle);
    return view;
}

bool ImageView::create(vk::Device device,
                       vk::Image image,
                       vk::Format format,
                       vk::ImageAspectFlags aspect_flags,
                       uint32_t mip_levels)
{
    reset();

    vk::ImageViewCreateInfo view_info = vkinit::imageviewCreateInfo(format, image, aspect_flags);
    view_info.subresourceRange.levelCount = mip_levels;

    vk::ImageView handle{};
    const vk::Result result = device.createImageView(&view_info, nullptr, &handle);
    if (result != vk::Result::eSuccess) {
        return false;
    }

    assign(device, handle);
    return true;
}

void ImageView::destroy(vk::Device device)
{
    (void) device;
    reset();
}

void ImageView::assign(vk::Device device, vk::ImageView handle)
{
    m_device = device;
    m_handle = handle;
}

void ImageView::reset()
{
    if (m_device != VK_NULL_HANDLE && m_handle != VK_NULL_HANDLE) {
        m_device.destroyImageView(m_handle, nullptr);
    }

    m_handle = VK_NULL_HANDLE;
    m_device = VK_NULL_HANDLE;
}

} // namespace luna::vkcore

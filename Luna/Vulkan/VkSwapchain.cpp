#include "VkSwapchain.h"

#include "VkDevice.h"
#include "VkPhysicalDevice.h"

#include <Core/Log.h>

#include <utility>

namespace {

template <typename T> void logVkbError(const char* step, const vkb::Result<T>& result)
{
    LUNA_CORE_ERROR("{} failed: {}", step, result.error().message());
    for (const std::string& reason : result.full_error().detailed_failure_reasons) {
        LUNA_CORE_ERROR("  {}", reason);
    }
}

} // namespace

namespace luna::vkcore {

Swapchain::~Swapchain()
{
    destroy();
}

Swapchain::Swapchain(Swapchain&& other) noexcept
{
    *this = std::move(other);
}

Swapchain& Swapchain::operator=(Swapchain&& other) noexcept
{
    if (this == &other) {
        return *this;
    }

    destroy();

    m_device = other.m_device;
    m_vkb_swapchain = other.m_vkb_swapchain;
    m_swapchain = other.m_swapchain;
    m_image_format = other.m_image_format;
    m_extent = other.m_extent;
    m_images = std::move(other.m_images);
    m_image_views = std::move(other.m_image_views);
    m_image_layouts = std::move(other.m_image_layouts);

    other.m_device = VK_NULL_HANDLE;
    other.m_vkb_swapchain = {};
    other.m_swapchain = VK_NULL_HANDLE;
    other.m_image_format = vk::Format::eUndefined;
    other.m_extent = {};
    other.m_images.clear();
    other.m_image_views.clear();
    other.m_image_layouts.clear();
    return *this;
}

bool Swapchain::create(const PhysicalDevice& physical_device,
                       const Device& device,
                       vk::SurfaceKHR surface,
                       uint32_t width,
                       uint32_t height)
{
    destroy();

    if (!physical_device.isValid() || !device.isValid() || surface == VK_NULL_HANDLE) {
        LUNA_CORE_ERROR("Cannot create swapchain because Vulkan device selection is incomplete");
        return false;
    }

    vkb::SwapchainBuilder swapchain_builder{physical_device.get(), device.get(), surface};
    auto swapchain_ret = swapchain_builder
                             .set_desired_format(
                                 VkSurfaceFormatKHR{.format = VK_FORMAT_B8G8R8A8_UNORM, .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR})
                             .set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR)
                             .set_desired_extent(width, height)
                             .add_image_usage_flags(VK_IMAGE_USAGE_TRANSFER_DST_BIT)
                             .build();
    if (!swapchain_ret) {
        logVkbError("Swapchain creation", swapchain_ret);
        return false;
    }

    m_vkb_swapchain = swapchain_ret.value();
    m_device = device.get();
    m_swapchain = m_vkb_swapchain.swapchain;
    m_extent = m_vkb_swapchain.extent;
    m_image_format = static_cast<vk::Format>(m_vkb_swapchain.image_format);

    auto images_ret = m_vkb_swapchain.get_images();
    if (!images_ret) {
        logVkbError("Swapchain image fetch", images_ret);
        destroy();
        return false;
    }

    m_images.clear();
    m_images.reserve(images_ret.value().size());
    for (VkImage image : images_ret.value()) {
        m_images.push_back(image);
    }

    auto image_views_ret = m_vkb_swapchain.get_image_views();
    if (!image_views_ret) {
        logVkbError("Swapchain image view fetch", image_views_ret);
        destroy();
        return false;
    }

    m_image_views.clear();
    m_image_views.reserve(image_views_ret.value().size());
    for (VkImageView image_view : image_views_ret.value()) {
        m_image_views.push_back(ImageView::fromHandle(m_device, image_view));
    }

    m_image_layouts.assign(m_images.size(), VK_IMAGE_LAYOUT_UNDEFINED);
    return true;
}

void Swapchain::destroy()
{
    if (m_device != VK_NULL_HANDLE) {
        m_image_views.clear();
        if (m_swapchain != VK_NULL_HANDLE) {
            vkDestroySwapchainKHR(m_device, m_swapchain, nullptr);
        }
    }

    m_images.clear();
    m_image_layouts.clear();
    m_device = VK_NULL_HANDLE;
    m_swapchain = VK_NULL_HANDLE;
    m_image_format = vk::Format::eUndefined;
    m_extent = {};
    m_vkb_swapchain = {};
}

} // namespace luna::vkcore

#pragma once

#include "VkBootstrap.h"
#include "VkImageView.h"
#include "VkTypes.h"

#include <vector>

namespace luna::vkcore {

class Device;
class PhysicalDevice;

class Swapchain {
public:
    Swapchain() = default;
    ~Swapchain();

    Swapchain(const Swapchain&) = delete;
    Swapchain& operator=(const Swapchain&) = delete;

    Swapchain(Swapchain&& other) noexcept;
    Swapchain& operator=(Swapchain&& other) noexcept;

    bool create(const PhysicalDevice& physical_device,
                const Device& device,
                vk::SurfaceKHR surface,
                uint32_t width,
                uint32_t height);
    void destroy();

    bool isValid() const
    {
        return m_swapchain != VK_NULL_HANDLE;
    }

    vk::SwapchainKHR get() const
    {
        return m_swapchain;
    }

    vk::Format getImageFormat() const
    {
        return m_image_format;
    }

    vk::Extent2D getExtent() const
    {
        return m_extent;
    }

    uint32_t getImageCount() const
    {
        return static_cast<uint32_t>(m_images.size());
    }

    const std::vector<vk::Image>& getImages() const
    {
        return m_images;
    }

    std::vector<vk::Image>& getImages()
    {
        return m_images;
    }

    const std::vector<ImageView>& getImageViews() const
    {
        return m_image_views;
    }

    std::vector<ImageView>& getImageViews()
    {
        return m_image_views;
    }

    const std::vector<VkImageLayout>& getImageLayouts() const
    {
        return m_image_layouts;
    }

    std::vector<VkImageLayout>& getImageLayouts()
    {
        return m_image_layouts;
    }

private:
    vk::Device m_device{};
    vkb::Swapchain m_vkb_swapchain{};
    vk::SwapchainKHR m_swapchain{};
    vk::Format m_image_format{vk::Format::eUndefined};
    vk::Extent2D m_extent{};
    std::vector<vk::Image> m_images;
    std::vector<ImageView> m_image_views;
    std::vector<VkImageLayout> m_image_layouts;
};

} // namespace luna::vkcore

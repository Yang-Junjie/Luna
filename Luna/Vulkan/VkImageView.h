#pragma once

#include <vulkan/vulkan.hpp>

namespace luna::vkcore {

class ImageView {
public:
    ImageView() = default;
    ~ImageView();

    ImageView(const ImageView&) = delete;
    ImageView& operator=(const ImageView&) = delete;

    ImageView(ImageView&& other) noexcept;
    ImageView& operator=(ImageView&& other) noexcept;

    static ImageView fromHandle(vk::Device device, vk::ImageView handle);

    bool create(vk::Device device,
                vk::Image image,
                vk::Format format,
                vk::ImageAspectFlags aspect_flags,
                uint32_t mip_levels = 1);
    void destroy(vk::Device device);

    bool isValid() const
    {
        return m_handle != VK_NULL_HANDLE;
    }

    vk::ImageView get() const
    {
        return m_handle;
    }

    void reset();

    operator vk::ImageView() const
    {
        return m_handle;
    }

    bool operator==(const ImageView& other) const
    {
        return m_handle == other.m_handle;
    }

private:
    void assign(vk::Device device, vk::ImageView handle);

    vk::ImageView m_handle{};
    vk::Device m_device{};
};

} // namespace luna::vkcore

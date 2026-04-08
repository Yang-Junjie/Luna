#include "Renderer/RenderContext.h"

#include "Vulkan/VkImages.h"

void RenderCommandList::transitionImage(AllocatedImage& image,
                                        luna::render::ImageLayout current_layout,
                                        luna::render::ImageLayout new_layout) const
{
    if (!isValid() || !image.isValid()) {
        return;
    }

    vkutil::transitionImage(m_handle, image.get(), toVk(current_layout), toVk(new_layout));
}

luna::render::ImageLayout FrameRenderContext::getSwapchainImageLayout() const
{
    if (m_swapchain_image_layout == nullptr) {
        return luna::render::ImageLayout::Undefined;
    }

    return fromVk(static_cast<vk::ImageLayout>(*m_swapchain_image_layout));
}

void FrameRenderContext::setSwapchainImageLayout(luna::render::ImageLayout layout) const
{
    if (m_swapchain_image_layout != nullptr) {
        *m_swapchain_image_layout = static_cast<VkImageLayout>(toVk(layout));
    }
}

void FrameRenderContext::transitionSwapchainImage(luna::render::ImageLayout new_layout) const
{
    if (!m_command_list.isValid() || m_swapchain_image == VK_NULL_HANDLE) {
        return;
    }

    vkutil::transitionImage(m_command_list.m_handle, m_swapchain_image, toVk(getSwapchainImageLayout()), toVk(new_layout));
    setSwapchainImageLayout(new_layout);
}

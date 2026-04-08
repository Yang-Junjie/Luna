#pragma once

#include "Renderer/RenderTypes.h"
#include "Vulkan/VkFrameContext.h"
#include "Vulkan/VkImageView.h"
#include "Vulkan/VkRenderTarget.h"
#include "Vulkan/VkTypes.h"

class VulkanEngine;
namespace luna {
class ImGuiLayer;
}

class RenderCommandList {
public:
    bool isValid() const
    {
        return m_handle != VK_NULL_HANDLE;
    }

    void transitionImage(AllocatedImage& image,
                         luna::render::ImageLayout current_layout,
                         luna::render::ImageLayout new_layout) const;

private:
    friend class VulkanEngine;
    friend class FrameRenderContext;
    friend class luna::ImGuiLayer;

    vk::CommandBuffer m_handle{};
};

class FrameRenderContext {
public:
    VulkanEngine& getEngine() const
    {
        return *m_engine;
    }

    luna::vkcore::FrameContext& getFrame() const
    {
        return *m_frame;
    }

    luna::vkcore::RenderTarget& getRenderTarget() const
    {
        return *m_render_target;
    }

    RenderCommandList& getCommandList()
    {
        return m_command_list;
    }

    const RenderCommandList& getCommandList() const
    {
        return m_command_list;
    }

    luna::render::Extent2D getDrawExtent() const
    {
        return m_draw_extent;
    }

    luna::render::Extent2D getSwapchainExtent() const
    {
        return m_swapchain_extent;
    }

    uint32_t getSwapchainImageIndex() const
    {
        return m_swapchain_image_index;
    }

    const luna::vkcore::ImageView& getSwapchainImageView() const
    {
        return *m_swapchain_image_view;
    }

    luna::render::ImageLayout getSwapchainImageLayout() const;
    void setSwapchainImageLayout(luna::render::ImageLayout layout) const;
    void transitionSwapchainImage(luna::render::ImageLayout new_layout) const;

    bool shouldCopyRenderTargetToSwapchain() const
    {
        return m_copy_render_target_to_swapchain;
    }

    void setCopyRenderTargetToSwapchain(bool enable)
    {
        m_copy_render_target_to_swapchain = enable;
    }

private:
    friend class VulkanEngine;

    VulkanEngine* m_engine{nullptr};
    luna::vkcore::FrameContext* m_frame{nullptr};
    luna::vkcore::RenderTarget* m_render_target{nullptr};
    RenderCommandList m_command_list{};
    luna::render::Extent2D m_draw_extent{};
    luna::render::Extent2D m_swapchain_extent{};
    uint32_t m_swapchain_image_index{0};
    vk::Image m_swapchain_image{};
    const luna::vkcore::ImageView* m_swapchain_image_view{nullptr};
    VkImageLayout* m_swapchain_image_layout{nullptr};
    bool m_copy_render_target_to_swapchain{true};
};

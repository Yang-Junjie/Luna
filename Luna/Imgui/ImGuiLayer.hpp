#pragma once

#include "Core/Layer.h"
#include "Renderer/RenderContext.h"

struct GLFWwindow;

class VulkanEngine;

namespace luna {

class Event;

class ImGuiLayer final : public Layer {
public:
    ImGuiLayer(GLFWwindow* window, VulkanEngine& engine, bool enable_multi_viewport);
    ~ImGuiLayer() override = default;

    void onAttach() override;
    void onDetach() override;
    void onEvent(Event& event) override;
    void onImGuiRender() override {}

    void begin();
    void end();

    void render(RenderCommandList& command_list,
                const luna::vkcore::ImageView& target_image_view,
                luna::render::Extent2D target_extent);
    void renderPlatformWindows();

    void blockEvents(bool block)
    {
        m_block_events = block;
    }

    bool isInitialized() const
    {
        return m_attached;
    }

    bool viewportsEnabled() const;

private:
    static void checkVulkanResult(VkResult result);
    void setImGuiWidgetStyle();
    void setDarkThemeColors();

private:
    bool m_block_events = true;
    bool m_attached = false;
    bool m_enable_multi_viewport = false;
    luna::render::PixelFormat m_color_attachment_format = luna::render::PixelFormat::Undefined;
    GLFWwindow* m_window = nullptr;
    VulkanEngine* m_engine = nullptr;
};

} // namespace luna


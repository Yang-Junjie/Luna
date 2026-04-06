#pragma once

#include "Core/layer.h"

#include <vulkan/vulkan.hpp>

struct GLFWwindow;
class VulkanDeviceContext;

namespace luna {

class Event;
class IRHIDevice;
struct FrameContext;

class ImGuiLayer final : public Layer {
public:
    ImGuiLayer(GLFWwindow* window, IRHIDevice& device, bool enableMultiViewport);
    ~ImGuiLayer() override = default;

    void onAttach() override;
    void onDetach() override;
    void onEvent(Event& event) override;
    void onImGuiRender() override {}

    void begin();
    void end();

    bool render(IRHIDevice& device, const FrameContext& frameContext);
    void render(vk::CommandBuffer commandBuffer, vk::ImageView targetImageView, vk::Extent2D targetExtent);
    void renderPlatformWindows();

    void blockEvents(bool block)
    {
        m_blockEvents = block;
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
    bool m_blockEvents = true;
    bool m_attached = false;
    bool m_enableMultiViewport = false;
    vk::Format m_colorAttachmentFormat = vk::Format::eUndefined;
    GLFWwindow* m_window = nullptr;
    IRHIDevice* m_rhiDevice = nullptr;
    VulkanDeviceContext* m_context = nullptr;
};

} // namespace luna

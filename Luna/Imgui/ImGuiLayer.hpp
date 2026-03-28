#pragma once

#include "Core/layer.h"

#include <vulkan/vulkan.h>

struct GLFWwindow;

class VulkanEngine;

namespace luna {

class Event;

class ImGuiLayer final : public Layer {
public:
    ImGuiLayer(GLFWwindow* window, VulkanEngine& engine, bool enableMultiViewport);
    ~ImGuiLayer() override = default;

    void onAttach() override;
    void onDetach() override;
    void onEvent(Event& event) override;
    void onImGuiRender() override {}

    void begin();
    void end();

    void render(VkCommandBuffer commandBuffer, VkImageView targetImageView, VkExtent2D targetExtent);
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
    VkFormat m_colorAttachmentFormat = VK_FORMAT_UNDEFINED;
    GLFWwindow* m_window = nullptr;
    VulkanEngine* m_engine = nullptr;
};

} // namespace luna

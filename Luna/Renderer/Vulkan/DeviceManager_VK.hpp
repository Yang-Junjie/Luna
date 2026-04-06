#pragma once

#include "Renderer/DeviceManager.hpp"
#include "Renderer/Vulkan/VulkanContext.hpp"

namespace luna::renderer::vulkan {

class DeviceManager_VK final : public DeviceManager {
public:
    GraphicsAPI graphicsAPI() const override { return GraphicsAPI::Vulkan; }

    bool initialize(GLFWwindow* window, const DeviceManagerCreateInfo& createInfo) override;
    void shutdown() override;

    bool validationEnabled() const { return m_context.validationEnabled(); }
    std::uint32_t apiVersion() const { return m_context.apiVersion(); }
    VkInstance instance() const { return m_context.instance(); }
    VkSurfaceKHR surface() const { return m_context.surface(); }
    VkPhysicalDevice physicalDevice() const { return m_context.physicalDevice(); }
    VkDevice device() const { return m_context.device(); }
    VkQueue graphicsQueue() const { return m_context.graphicsQueue(); }
    VkQueue presentQueue() const { return m_context.presentQueue(); }
    std::uint32_t graphicsQueueFamily() const { return m_context.graphicsQueueFamily(); }
    std::uint32_t presentQueueFamily() const { return m_context.presentQueueFamily(); }
    const std::string& gpuName() const { return m_context.gpuName(); }
    const std::vector<std::string>& requiredInstanceExtensions() const
    {
        return m_context.requiredInstanceExtensions();
    }

private:
    VulkanContext m_context;
};

} // namespace luna::renderer::vulkan

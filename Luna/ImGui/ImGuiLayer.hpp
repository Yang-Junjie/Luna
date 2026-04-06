#pragma once

#include "Events/event.h"

#include <vulkan/vulkan.h>

#include <cstdint>

struct GLFWwindow;

namespace luna {

struct ImGuiVulkanBackendConfig {
    GLFWwindow* window = nullptr;
    std::uint32_t apiVersion = VK_API_VERSION_1_1;
    VkInstance instance = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    std::uint32_t queueFamily = 0;
    VkQueue queue = VK_NULL_HANDLE;
    VkRenderPass renderPass = VK_NULL_HANDLE;
    std::uint32_t minImageCount = 2;
    std::uint32_t imageCount = 2;
};

class ImGuiLayer {
public:
    ImGuiLayer() = default;
    ~ImGuiLayer();

    bool initialize(const ImGuiVulkanBackendConfig& config);
    void shutdown();

    void beginFrame();
    void endFrame();
    void onEvent(Event& event);
    void setMinImageCount(std::uint32_t minImageCount);

    bool isInitialized() const { return m_initialized; }

private:
    bool m_initialized = false;
};

} // namespace luna

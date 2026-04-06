#pragma once

#include <VkBootstrap.h>
#include <vulkan/vulkan.h>

#include <cstdint>
#include <string>
#include <vector>

struct GLFWwindow;

namespace luna::renderer::vulkan {

class VulkanContext {
public:
    struct CreateInfo {
        const char* appName = "Luna Editor";
        const char* engineName = "Luna";
        std::uint32_t apiVersion = VK_API_VERSION_1_1;
        bool enableValidation = false;
    };

    bool initialize(GLFWwindow* window, const CreateInfo& createInfo);
    void shutdown();

    bool validationEnabled() const { return m_validationEnabled; }
    std::uint32_t apiVersion() const { return m_instance.instance_version; }
    VkInstance instance() const { return m_instance.instance; }
    VkSurfaceKHR surface() const { return m_surface; }
    VkPhysicalDevice physicalDevice() const { return m_physicalDevice; }
    VkDevice device() const { return m_device.device; }
    VkQueue graphicsQueue() const { return m_graphicsQueue; }
    VkQueue presentQueue() const { return m_presentQueue; }
    std::uint32_t graphicsQueueFamily() const { return m_graphicsQueueFamily; }
    std::uint32_t presentQueueFamily() const { return m_presentQueueFamily; }
    const std::string& gpuName() const { return m_gpuName; }
    const std::vector<std::string>& requiredInstanceExtensions() const { return m_requiredInstanceExtensions; }

private:
    bool createInstance(const CreateInfo& createInfo);
    bool createSurface(GLFWwindow* window);
    bool selectPhysicalDevice();
    bool createLogicalDevice();

    vkb::Instance m_instance;
    vkb::PhysicalDevice m_physicalDeviceSelection;
    vkb::Device m_device;

    VkSurfaceKHR m_surface = VK_NULL_HANDLE;
    VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;
    VkQueue m_graphicsQueue = VK_NULL_HANDLE;
    VkQueue m_presentQueue = VK_NULL_HANDLE;
    std::uint32_t m_graphicsQueueFamily = UINT32_MAX;
    std::uint32_t m_presentQueueFamily = UINT32_MAX;

    bool m_validationEnabled = false;
    std::string m_gpuName;
    std::vector<std::string> m_requiredInstanceExtensions;
};

void logVkResult(VkResult result);

} // namespace luna::renderer::vulkan

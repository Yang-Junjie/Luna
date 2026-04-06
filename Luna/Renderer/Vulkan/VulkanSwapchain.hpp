#pragma once

#include <vulkan/vulkan.h>

#include <cstdint>
#include <vector>

struct GLFWwindow;

namespace luna::renderer::vulkan {

class DeviceManager_VK;

enum class SwapchainStatus {
    Ready,
    Deferred,
    Failed
};

class VulkanSwapchain {
public:
    bool initialize(DeviceManager_VK& deviceManager, GLFWwindow* window);
    void shutdown();

    SwapchainStatus recreate(GLFWwindow* window, bool* renderPassChanged = nullptr);

    VkSwapchainKHR handle() const { return m_swapchain; }
    VkRenderPass renderPass() const { return m_renderPass; }
    VkFormat format() const { return m_swapchainFormat; }
    VkExtent2D extent() const { return m_extent; }
    const std::vector<VkImage>& images() const { return m_images; }
    const std::vector<VkImageView>& imageViews() const { return m_imageViews; }
    const std::vector<VkFramebuffer>& framebuffers() const { return m_framebuffers; }
    std::uint32_t imageCount() const { return static_cast<std::uint32_t>(m_images.size()); }
    std::uint32_t minImageCount() const
    {
        return m_images.size() < 2 ? 2u : static_cast<std::uint32_t>(m_images.size());
    }

private:
    bool queryFramebufferExtent(GLFWwindow* window, std::uint32_t& width, std::uint32_t& height) const;
    bool createSwapchain(GLFWwindow* window, bool logCreation, VkSwapchainKHR oldSwapchain);
    bool createRenderPass();
    bool createFramebuffers();

    void destroyFramebuffers();
    void destroyImageViews();
    void destroyRenderPass();
    void destroySwapchainHandle();

    DeviceManager_VK* m_deviceManager = nullptr;

    VkSwapchainKHR m_swapchain = VK_NULL_HANDLE;
    VkRenderPass m_renderPass = VK_NULL_HANDLE;
    VkFormat m_swapchainFormat = VK_FORMAT_UNDEFINED;
    VkFormat m_renderPassFormat = VK_FORMAT_UNDEFINED;
    VkExtent2D m_extent{0, 0};
    std::vector<VkImage> m_images;
    std::vector<VkImageView> m_imageViews;
    std::vector<VkFramebuffer> m_framebuffers;
};

} // namespace luna::renderer::vulkan

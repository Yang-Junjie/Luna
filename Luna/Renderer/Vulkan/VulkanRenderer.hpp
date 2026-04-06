#pragma once

#include "Renderer/Vulkan/DeviceManager_VK.hpp"
#include "Renderer/Vulkan/VulkanSwapchain.hpp"

#include <vulkan/vulkan.h>

#include <array>
#include <cstdint>
#include <string>
#include <vector>

struct GLFWwindow;
struct ImDrawData;

namespace luna::renderer::vulkan {

enum class SwapchainRebuildResult {
    Ready,
    Deferred,
    RenderPassChanged,
    Failed
};

class VulkanRenderer {
public:
    bool initialize(DeviceManager_VK& deviceManager, GLFWwindow* window);
    void shutdown();

    void requestSwapchainRebuild() { m_swapchainDirty = true; }
    bool isSwapchainDirty() const { return m_swapchainDirty; }

    SwapchainRebuildResult recreateSwapchain(GLFWwindow* window);
    bool renderFrame(ImDrawData* drawData, const std::array<float, 4>& clearColor);

    VkRenderPass renderPass() const { return m_swapchain.renderPass(); }
    std::uint32_t minImageCount() const { return m_swapchain.minImageCount(); }
    std::uint32_t imageCount() const { return m_swapchain.imageCount(); }

private:
    struct FrameResources {
        VkCommandPool commandPool = VK_NULL_HANDLE;
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkSemaphore imageAvailableSemaphore = VK_NULL_HANDLE;
        VkSemaphore renderFinishedSemaphore = VK_NULL_HANDLE;
        VkFence inFlightFence = VK_NULL_HANDLE;
    };

    static constexpr std::uint32_t kFramesInFlight = 2;

    bool createFrameResources();
    void destroyFrameResources();
    void resetImagesInFlight();

    DeviceManager_VK* m_deviceManager = nullptr;
    VulkanSwapchain m_swapchain;
    std::array<FrameResources, kFramesInFlight> m_frames{};
    std::vector<VkFence> m_imagesInFlight;

    std::uint64_t m_frameNumber = 0;
    bool m_swapchainDirty = false;
    bool m_firstFrameRendered = false;
    bool m_firstPresentSucceeded = false;
};

} // namespace luna::renderer::vulkan

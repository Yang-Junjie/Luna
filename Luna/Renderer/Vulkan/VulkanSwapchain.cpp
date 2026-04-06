#include "Renderer/Vulkan/VulkanSwapchain.hpp"

#include "Core/log.h"
#include "Renderer/Vulkan/DeviceManager_VK.hpp"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <algorithm>
#include <array>

namespace luna::renderer::vulkan {

bool VulkanSwapchain::initialize(DeviceManager_VK& deviceManager, GLFWwindow* window)
{
    m_deviceManager = &deviceManager;
    if (!createSwapchain(window, true, VK_NULL_HANDLE) || !createRenderPass() || !createFramebuffers()) {
        shutdown();
        return false;
    }
    return true;
}

void VulkanSwapchain::shutdown()
{
    destroyFramebuffers();
    destroyRenderPass();
    destroyImageViews();
    destroySwapchainHandle();

    m_images.clear();
    m_swapchainFormat = VK_FORMAT_UNDEFINED;
    m_renderPassFormat = VK_FORMAT_UNDEFINED;
    m_extent = {0, 0};
    m_deviceManager = nullptr;
}

SwapchainStatus VulkanSwapchain::recreate(GLFWwindow* window, bool* renderPassChanged)
{
    if (renderPassChanged != nullptr) {
        *renderPassChanged = false;
    }

    if (m_deviceManager == nullptr) {
        LUNA_CORE_ERROR("Cannot recreate swapchain without a Vulkan device manager");
        return SwapchainStatus::Failed;
    }

    std::uint32_t framebufferWidth = 0;
    std::uint32_t framebufferHeight = 0;
    if (!queryFramebufferExtent(window, framebufferWidth, framebufferHeight)) {
        return SwapchainStatus::Deferred;
    }

    if (vkDeviceWaitIdle(m_deviceManager->device()) != VK_SUCCESS) {
        LUNA_CORE_ERROR("Failed to idle device before recreating swapchain");
        return SwapchainStatus::Failed;
    }

    destroyFramebuffers();
    destroyImageViews();

    const VkSwapchainKHR oldSwapchain = m_swapchain;
    m_swapchain = VK_NULL_HANDLE;

    if (!createSwapchain(window, false, oldSwapchain)) {
        m_swapchain = oldSwapchain;
        return SwapchainStatus::Failed;
    }

    if (oldSwapchain != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(m_deviceManager->device(), oldSwapchain, nullptr);
    }

    if (m_renderPass == VK_NULL_HANDLE || m_renderPassFormat != m_swapchainFormat) {
        destroyRenderPass();
        if (!createRenderPass()) {
            return SwapchainStatus::Failed;
        }
        if (renderPassChanged != nullptr) {
            *renderPassChanged = true;
        }
    }

    if (!createFramebuffers()) {
        return SwapchainStatus::Failed;
    }

    LUNA_CORE_INFO("Swapchain Recreated: extent={}x{}, images={}",
                   m_extent.width,
                   m_extent.height,
                   m_images.size());
    return SwapchainStatus::Ready;
}

bool VulkanSwapchain::queryFramebufferExtent(GLFWwindow* window, std::uint32_t& width, std::uint32_t& height) const
{
    int framebufferWidth = 0;
    int framebufferHeight = 0;
    glfwGetFramebufferSize(window, &framebufferWidth, &framebufferHeight);
    if (framebufferWidth <= 0 || framebufferHeight <= 0) {
        width = 0;
        height = 0;
        return false;
    }

    width = static_cast<std::uint32_t>(framebufferWidth);
    height = static_cast<std::uint32_t>(framebufferHeight);
    return true;
}

bool VulkanSwapchain::createSwapchain(GLFWwindow* window, bool logCreation, VkSwapchainKHR oldSwapchain)
{
    if (m_deviceManager == nullptr) {
        return false;
    }

    std::uint32_t framebufferWidth = 0;
    std::uint32_t framebufferHeight = 0;
    if (!queryFramebufferExtent(window, framebufferWidth, framebufferHeight)) {
        LUNA_CORE_WARN("Framebuffer size is zero; postponing swapchain creation");
        return false;
    }

    VkSurfaceCapabilitiesKHR surfaceCapabilities{};
    if (vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
            m_deviceManager->physicalDevice(), m_deviceManager->surface(), &surfaceCapabilities) != VK_SUCCESS) {
        LUNA_CORE_ERROR("Failed to query surface capabilities");
        return false;
    }

    std::uint32_t formatCount = 0;
    if (vkGetPhysicalDeviceSurfaceFormatsKHR(
            m_deviceManager->physicalDevice(), m_deviceManager->surface(), &formatCount, nullptr) != VK_SUCCESS ||
        formatCount == 0) {
        LUNA_CORE_ERROR("Failed to query surface formats");
        return false;
    }

    std::vector<VkSurfaceFormatKHR> surfaceFormats(formatCount);
    if (vkGetPhysicalDeviceSurfaceFormatsKHR(
            m_deviceManager->physicalDevice(), m_deviceManager->surface(), &formatCount, surfaceFormats.data()) !=
        VK_SUCCESS) {
        LUNA_CORE_ERROR("Failed to fetch surface formats");
        return false;
    }

    std::uint32_t presentModeCount = 0;
    if (vkGetPhysicalDeviceSurfacePresentModesKHR(
            m_deviceManager->physicalDevice(), m_deviceManager->surface(), &presentModeCount, nullptr) !=
        VK_SUCCESS) {
        LUNA_CORE_ERROR("Failed to query present modes");
        return false;
    }

    std::vector<VkPresentModeKHR> presentModes(presentModeCount);
    if (presentModeCount > 0 &&
        vkGetPhysicalDeviceSurfacePresentModesKHR(
            m_deviceManager->physicalDevice(), m_deviceManager->surface(), &presentModeCount, presentModes.data()) !=
            VK_SUCCESS) {
        LUNA_CORE_ERROR("Failed to fetch present modes");
        return false;
    }

    VkSurfaceFormatKHR surfaceFormat = surfaceFormats.front();
    for (const VkSurfaceFormatKHR candidate : surfaceFormats) {
        const bool preferredFormat =
            candidate.format == VK_FORMAT_B8G8R8A8_UNORM || candidate.format == VK_FORMAT_B8G8R8A8_SRGB;
        if (preferredFormat && candidate.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            surfaceFormat = candidate;
            break;
        }
    }

    VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;
    if (std::find(presentModes.begin(), presentModes.end(), VK_PRESENT_MODE_MAILBOX_KHR) != presentModes.end()) {
        presentMode = VK_PRESENT_MODE_MAILBOX_KHR;
    }

    VkExtent2D extent = surfaceCapabilities.currentExtent;
    if (extent.width == UINT32_MAX) {
        extent.width = std::clamp(
            framebufferWidth, surfaceCapabilities.minImageExtent.width, surfaceCapabilities.maxImageExtent.width);
        extent.height = std::clamp(
            framebufferHeight, surfaceCapabilities.minImageExtent.height, surfaceCapabilities.maxImageExtent.height);
    }

    std::uint32_t imageCount = surfaceCapabilities.minImageCount + 1;
    if (surfaceCapabilities.maxImageCount > 0 && imageCount > surfaceCapabilities.maxImageCount) {
        imageCount = surfaceCapabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = m_deviceManager->surface();
    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = surfaceFormat.format;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    createInfo.imageExtent = extent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    const std::uint32_t queueFamilyIndices[] = {m_deviceManager->graphicsQueueFamily(),
                                                m_deviceManager->presentQueueFamily()};
    if (m_deviceManager->graphicsQueueFamily() != m_deviceManager->presentQueueFamily()) {
        createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices = queueFamilyIndices;
    } else {
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    createInfo.preTransform = surfaceCapabilities.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode = presentMode;
    createInfo.clipped = VK_TRUE;
    createInfo.oldSwapchain = oldSwapchain;

    VkSwapchainKHR newSwapchain = VK_NULL_HANDLE;
    if (vkCreateSwapchainKHR(m_deviceManager->device(), &createInfo, nullptr, &newSwapchain) != VK_SUCCESS) {
        LUNA_CORE_ERROR("Failed to create Vulkan swapchain");
        return false;
    }

    std::uint32_t swapchainImageCount = 0;
    if (vkGetSwapchainImagesKHR(m_deviceManager->device(), newSwapchain, &swapchainImageCount, nullptr) !=
            VK_SUCCESS ||
        swapchainImageCount == 0) {
        LUNA_CORE_ERROR("Failed to query swapchain images");
        vkDestroySwapchainKHR(m_deviceManager->device(), newSwapchain, nullptr);
        return false;
    }

    std::vector<VkImage> swapchainImages(swapchainImageCount);
    if (vkGetSwapchainImagesKHR(
            m_deviceManager->device(), newSwapchain, &swapchainImageCount, swapchainImages.data()) != VK_SUCCESS) {
        LUNA_CORE_ERROR("Failed to fetch swapchain images");
        vkDestroySwapchainKHR(m_deviceManager->device(), newSwapchain, nullptr);
        return false;
    }

    std::vector<VkImageView> imageViews;
    imageViews.reserve(swapchainImages.size());

    for (VkImage image : swapchainImages) {
        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = image;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = surfaceFormat.format;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;

        VkImageView imageView = VK_NULL_HANDLE;
        if (vkCreateImageView(m_deviceManager->device(), &viewInfo, nullptr, &imageView) != VK_SUCCESS) {
            LUNA_CORE_ERROR("Failed to create swapchain image view");
            for (VkImageView createdView : imageViews) {
                vkDestroyImageView(m_deviceManager->device(), createdView, nullptr);
            }
            vkDestroySwapchainKHR(m_deviceManager->device(), newSwapchain, nullptr);
            return false;
        }

        imageViews.push_back(imageView);
    }

    m_swapchain = newSwapchain;
    m_images = std::move(swapchainImages);
    m_imageViews = std::move(imageViews);
    m_swapchainFormat = surfaceFormat.format;
    m_extent = extent;

    if (logCreation) {
        LUNA_CORE_INFO("Swapchain created: format={}, extent={}x{}, images={}",
                       static_cast<int>(m_swapchainFormat),
                       m_extent.width,
                       m_extent.height,
                       m_images.size());
    }

    return true;
}

bool VulkanSwapchain::createRenderPass()
{
    if (m_deviceManager == nullptr) {
        return false;
    }

    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = m_swapchainFormat;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference colorAttachmentRef{};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;

    std::array<VkSubpassDependency, 2> dependencies{};
    dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[0].dstSubpass = 0;
    dependencies[0].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    dependencies[1].srcSubpass = 0;
    dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dependencies[1].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;

    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = 1;
    renderPassInfo.pAttachments = &colorAttachment;
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = static_cast<std::uint32_t>(dependencies.size());
    renderPassInfo.pDependencies = dependencies.data();

    if (vkCreateRenderPass(m_deviceManager->device(), &renderPassInfo, nullptr, &m_renderPass) != VK_SUCCESS) {
        LUNA_CORE_ERROR("Failed to create render pass");
        return false;
    }

    m_renderPassFormat = m_swapchainFormat;
    return true;
}

bool VulkanSwapchain::createFramebuffers()
{
    if (m_deviceManager == nullptr || m_renderPass == VK_NULL_HANDLE) {
        return false;
    }

    std::vector<VkFramebuffer> framebuffers;
    framebuffers.reserve(m_imageViews.size());

    for (VkImageView imageView : m_imageViews) {
        VkFramebufferCreateInfo framebufferInfo{};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = m_renderPass;
        framebufferInfo.attachmentCount = 1;
        framebufferInfo.pAttachments = &imageView;
        framebufferInfo.width = m_extent.width;
        framebufferInfo.height = m_extent.height;
        framebufferInfo.layers = 1;

        VkFramebuffer framebuffer = VK_NULL_HANDLE;
        if (vkCreateFramebuffer(m_deviceManager->device(), &framebufferInfo, nullptr, &framebuffer) != VK_SUCCESS) {
            LUNA_CORE_ERROR("Failed to create framebuffer");
            for (VkFramebuffer createdFramebuffer : framebuffers) {
                vkDestroyFramebuffer(m_deviceManager->device(), createdFramebuffer, nullptr);
            }
            return false;
        }

        framebuffers.push_back(framebuffer);
    }

    m_framebuffers = std::move(framebuffers);
    return true;
}

void VulkanSwapchain::destroyFramebuffers()
{
    if (m_deviceManager == nullptr) {
        m_framebuffers.clear();
        return;
    }

    for (VkFramebuffer framebuffer : m_framebuffers) {
        vkDestroyFramebuffer(m_deviceManager->device(), framebuffer, nullptr);
    }
    m_framebuffers.clear();
}

void VulkanSwapchain::destroyImageViews()
{
    if (m_deviceManager == nullptr) {
        m_imageViews.clear();
        m_images.clear();
        return;
    }

    for (VkImageView imageView : m_imageViews) {
        vkDestroyImageView(m_deviceManager->device(), imageView, nullptr);
    }
    m_imageViews.clear();
    m_images.clear();
}

void VulkanSwapchain::destroyRenderPass()
{
    if (m_deviceManager != nullptr && m_renderPass != VK_NULL_HANDLE) {
        vkDestroyRenderPass(m_deviceManager->device(), m_renderPass, nullptr);
    }
    m_renderPass = VK_NULL_HANDLE;
    m_renderPassFormat = VK_FORMAT_UNDEFINED;
}

void VulkanSwapchain::destroySwapchainHandle()
{
    if (m_deviceManager != nullptr && m_swapchain != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(m_deviceManager->device(), m_swapchain, nullptr);
    }
    m_swapchain = VK_NULL_HANDLE;
}

} // namespace luna::renderer::vulkan

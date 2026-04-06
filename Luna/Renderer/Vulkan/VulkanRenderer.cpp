#include "Renderer/Vulkan/VulkanRenderer.hpp"

#include "Core/log.h"

#include <backends/imgui_impl_vulkan.h>
#include <imgui.h>

#include <cstring>

namespace luna::renderer::vulkan {

bool VulkanRenderer::initialize(DeviceManager_VK& deviceManager, GLFWwindow* window)
{
    m_deviceManager = &deviceManager;

    if (!m_swapchain.initialize(deviceManager, window) || !createFrameResources()) {
        shutdown();
        return false;
    }

    resetImagesInFlight();
    m_swapchainDirty = false;
    return true;
}

void VulkanRenderer::shutdown()
{
    if (m_deviceManager != nullptr && m_deviceManager->device() != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(m_deviceManager->device());
    }

    destroyFrameResources();
    m_swapchain.shutdown();

    m_imagesInFlight.clear();
    m_frameNumber = 0;
    m_swapchainDirty = false;
    m_firstFrameRendered = false;
    m_firstPresentSucceeded = false;
    m_deviceManager = nullptr;
}

SwapchainRebuildResult VulkanRenderer::recreateSwapchain(GLFWwindow* window)
{
    if (!m_swapchainDirty) {
        return SwapchainRebuildResult::Ready;
    }

    bool renderPassChanged = false;
    switch (m_swapchain.recreate(window, &renderPassChanged)) {
        case SwapchainStatus::Ready:
            resetImagesInFlight();
            m_swapchainDirty = false;
            return renderPassChanged ? SwapchainRebuildResult::RenderPassChanged : SwapchainRebuildResult::Ready;
        case SwapchainStatus::Deferred:
            m_swapchainDirty = true;
            return SwapchainRebuildResult::Deferred;
        case SwapchainStatus::Failed:
        default:
            m_swapchainDirty = true;
            return SwapchainRebuildResult::Failed;
    }
}

bool VulkanRenderer::renderFrame(ImDrawData* drawData, const std::array<float, 4>& clearColor)
{
    if (m_deviceManager == nullptr || m_deviceManager->device() == VK_NULL_HANDLE ||
        m_swapchain.handle() == VK_NULL_HANDLE ||
        m_swapchain.renderPass() == VK_NULL_HANDLE) {
        return false;
    }

    auto& frame = m_frames[m_frameNumber % m_frames.size()];
    if (vkWaitForFences(m_deviceManager->device(), 1, &frame.inFlightFence, VK_TRUE, UINT64_MAX) != VK_SUCCESS) {
        LUNA_CORE_ERROR("Failed to wait for in-flight frame fence");
        return false;
    }

    std::uint32_t imageIndex = 0;
    const VkResult acquireResult = vkAcquireNextImageKHR(m_deviceManager->device(),
                                                         m_swapchain.handle(),
                                                         UINT64_MAX,
                                                         frame.imageAvailableSemaphore,
                                                         VK_NULL_HANDLE,
                                                         &imageIndex);
    if (acquireResult == VK_ERROR_OUT_OF_DATE_KHR) {
        m_swapchainDirty = true;
        return true;
    }
    if (acquireResult != VK_SUCCESS && acquireResult != VK_SUBOPTIMAL_KHR) {
        LUNA_CORE_ERROR("Failed to acquire swapchain image, VkResult={}", static_cast<int>(acquireResult));
        return false;
    }
    if (acquireResult == VK_SUBOPTIMAL_KHR) {
        m_swapchainDirty = true;
    }

    if (imageIndex >= m_imagesInFlight.size()) {
        LUNA_CORE_ERROR("Swapchain image index {} exceeded tracked frame fences", imageIndex);
        return false;
    }

    if (m_imagesInFlight[imageIndex] != VK_NULL_HANDLE &&
        vkWaitForFences(m_deviceManager->device(), 1, &m_imagesInFlight[imageIndex], VK_TRUE, UINT64_MAX) !=
            VK_SUCCESS) {
        LUNA_CORE_ERROR("Failed to wait for swapchain image fence");
        return false;
    }
    m_imagesInFlight[imageIndex] = frame.inFlightFence;

    if (vkResetFences(m_deviceManager->device(), 1, &frame.inFlightFence) != VK_SUCCESS) {
        LUNA_CORE_ERROR("Failed to reset in-flight fence");
        return false;
    }
    if (vkResetCommandPool(m_deviceManager->device(), frame.commandPool, 0) != VK_SUCCESS) {
        LUNA_CORE_ERROR("Failed to reset command pool");
        return false;
    }

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    if (vkBeginCommandBuffer(frame.commandBuffer, &beginInfo) != VK_SUCCESS) {
        LUNA_CORE_ERROR("Failed to begin command buffer");
        return false;
    }

    VkClearValue clearValue{};
    std::memcpy(clearValue.color.float32, clearColor.data(), sizeof(float) * clearColor.size());

    const auto& framebuffers = m_swapchain.framebuffers();
    if (imageIndex >= framebuffers.size()) {
        LUNA_CORE_ERROR("Swapchain framebuffer index {} exceeded framebuffer count", imageIndex);
        return false;
    }

    VkRenderPassBeginInfo renderPassBeginInfo{};
    renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassBeginInfo.renderPass = m_swapchain.renderPass();
    renderPassBeginInfo.framebuffer = framebuffers[imageIndex];
    renderPassBeginInfo.renderArea.offset = {0, 0};
    renderPassBeginInfo.renderArea.extent = m_swapchain.extent();
    renderPassBeginInfo.clearValueCount = 1;
    renderPassBeginInfo.pClearValues = &clearValue;

    vkCmdBeginRenderPass(frame.commandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
    if (drawData != nullptr) {
        ImGui_ImplVulkan_RenderDrawData(drawData, frame.commandBuffer);
    }
    vkCmdEndRenderPass(frame.commandBuffer);

    if (vkEndCommandBuffer(frame.commandBuffer) != VK_SUCCESS) {
        LUNA_CORE_ERROR("Failed to end command buffer");
        return false;
    }

    const VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = &frame.imageAvailableSemaphore;
    submitInfo.pWaitDstStageMask = &waitStage;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &frame.commandBuffer;
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = &frame.renderFinishedSemaphore;

    if (vkQueueSubmit(m_deviceManager->graphicsQueue(), 1, &submitInfo, frame.inFlightFence) != VK_SUCCESS) {
        LUNA_CORE_ERROR("Failed to submit graphics queue");
        return false;
    }

    if (!m_firstFrameRendered) {
        LUNA_CORE_INFO("First Frame Rendered");
        m_firstFrameRendered = true;
    }

    const VkSwapchainKHR swapchainHandle = m_swapchain.handle();
    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = &frame.renderFinishedSemaphore;
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &swapchainHandle;
    presentInfo.pImageIndices = &imageIndex;

    const VkResult presentResult = vkQueuePresentKHR(m_deviceManager->presentQueue(), &presentInfo);
    if (presentResult == VK_ERROR_OUT_OF_DATE_KHR || presentResult == VK_SUBOPTIMAL_KHR) {
        m_swapchainDirty = true;
    } else if (presentResult != VK_SUCCESS) {
        LUNA_CORE_ERROR("Failed to present swapchain image, VkResult={}", static_cast<int>(presentResult));
        return false;
    }

    if (!m_firstPresentSucceeded && presentResult == VK_SUCCESS) {
        LUNA_CORE_INFO("First Present Succeeded");
        m_firstPresentSucceeded = true;
    }

    ++m_frameNumber;
    return true;
}

bool VulkanRenderer::createFrameResources()
{
    if (m_deviceManager == nullptr || m_deviceManager->device() == VK_NULL_HANDLE) {
        return false;
    }

    for (auto& frame : m_frames) {
        VkCommandPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        poolInfo.queueFamilyIndex = m_deviceManager->graphicsQueueFamily();

        if (vkCreateCommandPool(m_deviceManager->device(), &poolInfo, nullptr, &frame.commandPool) != VK_SUCCESS) {
            LUNA_CORE_ERROR("Failed to create command pool");
            destroyFrameResources();
            return false;
        }

        VkCommandBufferAllocateInfo commandBufferInfo{};
        commandBufferInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        commandBufferInfo.commandPool = frame.commandPool;
        commandBufferInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        commandBufferInfo.commandBufferCount = 1;

        if (vkAllocateCommandBuffers(m_deviceManager->device(), &commandBufferInfo, &frame.commandBuffer) !=
            VK_SUCCESS) {
            LUNA_CORE_ERROR("Failed to allocate command buffer");
            destroyFrameResources();
            return false;
        }

        VkSemaphoreCreateInfo semaphoreInfo{};
        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        if (vkCreateSemaphore(m_deviceManager->device(), &semaphoreInfo, nullptr, &frame.imageAvailableSemaphore) !=
                VK_SUCCESS ||
            vkCreateSemaphore(m_deviceManager->device(), &semaphoreInfo, nullptr, &frame.renderFinishedSemaphore) !=
                VK_SUCCESS) {
            LUNA_CORE_ERROR("Failed to create synchronization semaphores");
            destroyFrameResources();
            return false;
        }

        VkFenceCreateInfo fenceInfo{};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
        if (vkCreateFence(m_deviceManager->device(), &fenceInfo, nullptr, &frame.inFlightFence) != VK_SUCCESS) {
            LUNA_CORE_ERROR("Failed to create in-flight fence");
            destroyFrameResources();
            return false;
        }
    }

    return true;
}

void VulkanRenderer::destroyFrameResources()
{
    if (m_deviceManager == nullptr || m_deviceManager->device() == VK_NULL_HANDLE) {
        for (auto& frame : m_frames) {
            frame = {};
        }
        return;
    }

    for (auto& frame : m_frames) {
        if (frame.commandPool != VK_NULL_HANDLE) {
            vkDestroyCommandPool(m_deviceManager->device(), frame.commandPool, nullptr);
        }
        if (frame.imageAvailableSemaphore != VK_NULL_HANDLE) {
            vkDestroySemaphore(m_deviceManager->device(), frame.imageAvailableSemaphore, nullptr);
        }
        if (frame.renderFinishedSemaphore != VK_NULL_HANDLE) {
            vkDestroySemaphore(m_deviceManager->device(), frame.renderFinishedSemaphore, nullptr);
        }
        if (frame.inFlightFence != VK_NULL_HANDLE) {
            vkDestroyFence(m_deviceManager->device(), frame.inFlightFence, nullptr);
        }
        frame = {};
    }
}

void VulkanRenderer::resetImagesInFlight()
{
    m_imagesInFlight.assign(m_swapchain.imageCount(), VK_NULL_HANDLE);
}

} // namespace luna::renderer::vulkan

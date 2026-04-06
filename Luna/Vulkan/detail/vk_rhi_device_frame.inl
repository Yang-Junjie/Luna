RHIResult VulkanRHIDevice::beginFrame(FrameContext* outFrameContext)
{
    if (!m_initialized || outFrameContext == nullptr || m_frameInProgress || m_pendingPresent) {
        return RHIResult::InvalidArgument;
    }

    FrameData& frame = m_context.get_current_frame();
    VK_CHECK(m_context._device.waitForFences(1, &frame._renderFence, VK_TRUE, 1'000'000'000));
    if (frame._submitSerial > 0 && frame._submitSerial > m_lastCompletedSerial) {
        retireCompletedSerial(frame._submitSerial);
    }
    frame._deletionQueue.flush();
    frame._frameDescriptors.clear_pools(m_context._device);

    for (uint32_t attempt = 0; attempt < 2; ++attempt) {
        if (!ensureFramebufferReady()) {
            return RHIResult::NotReady;
        }

        m_swapchainImageIndex = 0;
        m_recreateAfterPresent = false;
        const vk::Result acquireResult = m_context._device.acquireNextImageKHR(
            m_context._swapchain, 1'000'000'000, frame._swapchainSemaphore, nullptr, &m_swapchainImageIndex);
        if (acquireResult == vk::Result::eErrorOutOfDateKHR) {
            if (!m_context.resize_swapchain()) {
                return RHIResult::InternalError;
            }
            continue;
        }
        if (acquireResult == vk::Result::eSuboptimalKHR) {
            m_recreateAfterPresent = true;
        } else if (acquireResult != vk::Result::eSuccess) {
            LUNA_CORE_ERROR("vkAcquireNextImageKHR failed: {}", vk::to_string(acquireResult));
            return RHIResult::InternalError;
        }

        VK_CHECK(m_context._device.resetFences(1, &frame._renderFence));
        VK_CHECK(frame._mainCommandBuffer.reset({}));
        const vk::CommandBufferBeginInfo beginInfo =
            vkinit::command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
        VK_CHECK(frame._mainCommandBuffer.begin(&beginInfo));

        m_context.begin_upload_batch(frame._mainCommandBuffer, frame);
        m_commandContext->beginFrame(frame._mainCommandBuffer);
        refreshBackbufferHandle();

        outFrameContext->frameIndex = static_cast<uint32_t>(m_context._frameNumber);
        outFrameContext->renderWidth = m_context._swapchainExtent.width;
        outFrameContext->renderHeight = m_context._swapchainExtent.height;
        outFrameContext->backbuffer = m_currentBackbufferHandle;
        outFrameContext->backbufferFormat = from_vulkan_format(m_context._swapchainImageFormat);
        outFrameContext->commandContext = m_commandContext.get();

        m_frameInProgress = true;
        if (!m_loggedBeginFramePass) {
            LUNA_CORE_INFO("BeginFrame PASS");
            m_loggedBeginFramePass = true;
        }
        if (!m_loggedAcquirePass) {
            LUNA_CORE_INFO("Acquire PASS");
            m_loggedAcquirePass = true;
        }
        return RHIResult::Success;
    }

    return RHIResult::InternalError;
}

RHIResult VulkanRHIDevice::endFrame()
{
    if (!m_frameInProgress) {
        return RHIResult::InvalidArgument;
    }

    FrameData& frame = m_context.get_current_frame();
    m_context.end_upload_batch();

    if (m_commandContext->isRendering()) {
        const RHIResult endRenderingResult = m_commandContext->endRendering();
        if (endRenderingResult != RHIResult::Success) {
            return endRenderingResult;
        }
    }

    if (m_currentBackbufferHandle.isValid()) {
        ImageResource* backbuffer = findImage(m_currentBackbufferHandle);
        if (backbuffer != nullptr) {
            vk::ImageLayout currentLayout = vk::ImageLayout::eUndefined;
            if (m_swapchainImageIndex < m_context._swapchainImageLayouts.size()) {
                currentLayout = static_cast<vk::ImageLayout>(m_context._swapchainImageLayouts[m_swapchainImageIndex]);
            } else if (!getImageRangeLayout(*backbuffer, 0, 1, 0, 1, &currentLayout)) {
                currentLayout = backbuffer->layout;
            }

            if (currentLayout != vk::ImageLayout::ePresentSrcKHR) {
                vkutil::transition_image(frame._mainCommandBuffer,
                                         backbuffer->image.image,
                                         currentLayout,
                                         vk::ImageLayout::ePresentSrcKHR);
                setImageRangeLayout(*backbuffer, 0, 1, 0, 1, vk::ImageLayout::ePresentSrcKHR);
                if (m_swapchainImageIndex < m_context._swapchainImageLayouts.size()) {
                    m_context._swapchainImageLayouts[m_swapchainImageIndex] = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
                }
            }
        }
    }

    VK_CHECK(frame._mainCommandBuffer.end());

    const vk::CommandBufferSubmitInfo commandInfo = vkinit::command_buffer_submit_info(frame._mainCommandBuffer);
    const vk::SemaphoreSubmitInfo waitInfo = vkinit::semaphore_submit_info(
        vk::PipelineStageFlagBits2::eColorAttachmentOutput, frame._swapchainSemaphore);
    const vk::SemaphoreSubmitInfo signalInfo =
        vkinit::semaphore_submit_info(vk::PipelineStageFlagBits2::eAllGraphics, frame._renderSemaphore);
    const vk::SubmitInfo2 submitInfo = vkinit::submit_info(&commandInfo, &signalInfo, &waitInfo);
    const uint64_t submitSerial = ++m_lastSubmittedSerial;
    frame._submitSerial = submitSerial;
    VK_CHECK(m_context._graphicsQueue.submit2(1, &submitInfo, frame._renderFence));
    appendTimelineEvent("Graphics submit #" + std::to_string(submitSerial));
    appendTimelineEvent("submitted fence #" + std::to_string(submitSerial));
    if (frame._uploadBatchOps > 0) {
        appendTimelineEvent("Upload batch #" + std::to_string(submitSerial) + " ops=" +
                            std::to_string(frame._uploadBatchOps) + " bytes=" +
                            std::to_string(static_cast<unsigned long long>(frame._uploadBatchBytes)) +
                            " staging ring capacity=" +
                            std::to_string(static_cast<unsigned long long>(frame._uploadStagingCapacity)));
    }

    m_commandContext->reset();
    m_frameInProgress = false;
    m_pendingPresent = true;
    return RHIResult::Success;
}

RHIResult VulkanRHIDevice::present()
{
    if (!m_pendingPresent) {
        return RHIResult::InvalidArgument;
    }

    FrameData& frame = m_context.get_current_frame();
    vk::PresentInfoKHR presentInfo{};
    presentInfo.pSwapchains = &m_context._swapchain;
    presentInfo.swapchainCount = 1;
    presentInfo.pWaitSemaphores = &frame._renderSemaphore;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pImageIndices = &m_swapchainImageIndex;

    const vk::Result presentResult = m_context._graphicsQueue.presentKHR(&presentInfo);
    if (presentResult == vk::Result::eErrorOutOfDateKHR || presentResult == vk::Result::eSuboptimalKHR) {
        m_context.request_swapchain_resize();
    } else if (presentResult != vk::Result::eSuccess) {
        LUNA_CORE_ERROR("vkQueuePresentKHR failed: {}", vk::to_string(presentResult));
        return RHIResult::InternalError;
    }

    if (m_recreateAfterPresent) {
        m_context.request_swapchain_resize();
    }

    if (!m_loggedPresentPass) {
        LUNA_CORE_INFO("Present PASS");
        m_loggedPresentPass = true;
    }

    m_context._frameNumber++;
    m_pendingPresent = false;
    return RHIResult::Success;
}

RHIResult VulkanRHIDevice::renderOverlay(ImGuiLayer& imguiLayer)
{
    if (!m_frameInProgress || m_swapchainImageIndex >= m_context._swapchainImageViews.size() ||
        m_swapchainImageIndex >= m_context._swapchainImageLayouts.size()) {
        return RHIResult::InvalidArgument;
    }

    FrameData& frame = m_context.get_current_frame();
    vk::ImageLayout currentLayout = static_cast<vk::ImageLayout>(m_context._swapchainImageLayouts[m_swapchainImageIndex]);
    if (currentLayout != vk::ImageLayout::eColorAttachmentOptimal) {
        vkutil::transition_image(frame._mainCommandBuffer,
                                 m_context._swapchainImages[m_swapchainImageIndex],
                                 currentLayout,
                                 vk::ImageLayout::eColorAttachmentOptimal);
        m_context._swapchainImageLayouts[m_swapchainImageIndex] = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        if (ImageResource* backbuffer = findImage(m_currentBackbufferHandle); backbuffer != nullptr) {
            setImageRangeLayout(*backbuffer, 0, 1, 0, 1, vk::ImageLayout::eColorAttachmentOptimal);
        }
    }

    imguiLayer.render(frame._mainCommandBuffer,
                      m_context._swapchainImageViews[m_swapchainImageIndex],
                      m_context._swapchainExtent);
    return RHIResult::Success;
}



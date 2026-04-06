RHIResult VulkanRHIDevice::CommandContext::beginRendering(const RenderingInfo& renderingInfo)
{
    if (!m_commandBuffer || m_rendering) {
        return RHIResult::InvalidArgument;
    }

    const bool hasDepthAttachment =
        renderingInfo.depthAttachment.image.isValid() || renderingInfo.depthAttachment.view.isValid();
    if (renderingInfo.colorAttachments.empty() && !hasDepthAttachment) {
        return RHIResult::InvalidArgument;
    }

    std::vector<ResolvedAttachmentTarget> colorTargets;
    colorTargets.reserve(renderingInfo.colorAttachments.size());

    vk::Extent2D fallbackExtent{};
    const auto absorb_extent = [&](vk::Extent2D extent) -> bool {
        if (extent.width == 0 || extent.height == 0) {
            return false;
        }

        if (fallbackExtent.width == 0 || fallbackExtent.height == 0) {
            fallbackExtent = extent;
            return true;
        }

        if ((renderingInfo.width == 0 || renderingInfo.height == 0) &&
            (fallbackExtent.width != extent.width || fallbackExtent.height != extent.height)) {
            return false;
        }

        return true;
    };

    ImageHandle firstColorAttachment{};
    for (size_t attachmentIndex = 0; attachmentIndex < renderingInfo.colorAttachments.size(); ++attachmentIndex) {
        const ColorAttachmentInfo& colorAttachment = renderingInfo.colorAttachments[attachmentIndex];
        ResolvedAttachmentTarget target{};
        if (!resolveAttachmentTarget(colorAttachment.image, colorAttachment.view, ImageAspect::Color, &target) ||
            target.image == nullptr || luna::is_depth_format(target.image->desc.format) || !absorb_extent(target.extent)) {
            return RHIResult::InvalidArgument;
        }

        colorTargets.push_back(target);
        if (attachmentIndex == 0) {
            firstColorAttachment = target.imageHandle;
        }
    }

    ResolvedAttachmentTarget depthTarget{};
    if (hasDepthAttachment &&
        (!resolveAttachmentTarget(renderingInfo.depthAttachment.image,
                                  renderingInfo.depthAttachment.view,
                                  ImageAspect::Depth,
                                  &depthTarget) ||
         depthTarget.image == nullptr || !luna::is_depth_format(depthTarget.image->desc.format) ||
         !absorb_extent(depthTarget.extent))) {
        return RHIResult::InvalidArgument;
    }

    if (fallbackExtent.width == 0 || fallbackExtent.height == 0) {
        return RHIResult::InvalidArgument;
    }

    m_renderExtent = {
        renderingInfo.width > 0 ? renderingInfo.width : fallbackExtent.width,
        renderingInfo.height > 0 ? renderingInfo.height : fallbackExtent.height,
    };
    const auto fits_render_extent = [&](vk::Extent2D attachmentExtent) {
        return m_renderExtent.width <= attachmentExtent.width && m_renderExtent.height <= attachmentExtent.height;
    };
    for (const ResolvedAttachmentTarget& target : colorTargets) {
        if (!fits_render_extent(target.extent)) {
            return RHIResult::InvalidArgument;
        }
    }
    if (hasDepthAttachment && !fits_render_extent(depthTarget.extent)) {
        return RHIResult::InvalidArgument;
    }

    std::vector<vk::RenderingAttachmentInfo> colorAttachmentInfos;
    colorAttachmentInfos.reserve(colorTargets.size());
    for (size_t attachmentIndex = 0; attachmentIndex < colorTargets.size(); ++attachmentIndex) {
        const ColorAttachmentInfo& colorAttachment = renderingInfo.colorAttachments[attachmentIndex];
        const ResolvedAttachmentTarget& target = colorTargets[attachmentIndex];

        if (imageBarrier({.image = target.imageHandle,
                          .oldLayout = ImageLayout::Undefined,
                          .newLayout = ImageLayout::ColorAttachment,
                          .srcStage = PipelineStage::AllCommands,
                          .dstStage = PipelineStage::ColorAttachmentOutput,
                          .srcAccess = ResourceAccess::MemoryRead | ResourceAccess::MemoryWrite,
                          .dstAccess = ResourceAccess::ColorAttachmentRead | ResourceAccess::ColorAttachmentWrite,
                          .aspect = ImageAspect::Color,
                          .baseMipLevel = target.baseMipLevel,
                          .mipCount = target.mipCount,
                          .baseArrayLayer = target.baseArrayLayer,
                          .layerCount = target.layerCount}) != RHIResult::Success) {
            return RHIResult::InvalidArgument;
        }

        vk::RenderingAttachmentInfo colorAttachmentInfo{};
        colorAttachmentInfo.imageView = target.imageView;
        colorAttachmentInfo.imageLayout = vk::ImageLayout::eColorAttachmentOptimal;
        colorAttachmentInfo.loadOp = to_vulkan_attachment_load_op(colorAttachment.loadOp);
        colorAttachmentInfo.storeOp = to_vulkan_attachment_store_op(colorAttachment.storeOp);
        colorAttachmentInfo.clearValue.color = vk::ClearColorValue(std::array<float, 4>{
            colorAttachment.clearColor.r,
            colorAttachment.clearColor.g,
            colorAttachment.clearColor.b,
            colorAttachment.clearColor.a,
        });
        colorAttachmentInfos.push_back(colorAttachmentInfo);
    }

    vk::RenderingAttachmentInfo depthAttachmentInfo{};
    const vk::RenderingAttachmentInfo* depthAttachmentPtr = nullptr;
    if (hasDepthAttachment) {
        if (imageBarrier({.image = depthTarget.imageHandle,
                          .oldLayout = ImageLayout::Undefined,
                          .newLayout = ImageLayout::DepthStencilAttachment,
                          .srcStage = PipelineStage::AllCommands,
                          .dstStage = PipelineStage::AllCommands,
                          .srcAccess = ResourceAccess::MemoryRead | ResourceAccess::MemoryWrite,
                          .dstAccess = ResourceAccess::DepthStencilRead | ResourceAccess::DepthStencilWrite,
                          .aspect = ImageAspect::Depth,
                          .baseMipLevel = depthTarget.baseMipLevel,
                          .mipCount = depthTarget.mipCount,
                          .baseArrayLayer = depthTarget.baseArrayLayer,
                          .layerCount = depthTarget.layerCount}) != RHIResult::Success) {
            return RHIResult::InvalidArgument;
        }

        depthAttachmentInfo.imageView = depthTarget.imageView;
        depthAttachmentInfo.imageLayout = vk::ImageLayout::eDepthAttachmentOptimal;
        depthAttachmentInfo.loadOp = to_vulkan_attachment_load_op(renderingInfo.depthAttachment.loadOp);
        depthAttachmentInfo.storeOp = to_vulkan_attachment_store_op(renderingInfo.depthAttachment.storeOp);
        depthAttachmentInfo.clearValue.depthStencil.depth = renderingInfo.depthAttachment.clearDepth;
        depthAttachmentInfo.clearValue.depthStencil.stencil = 0;
        depthAttachmentPtr = &depthAttachmentInfo;
    }

    const vk::RenderingInfo vkRenderingInfo = vkinit::rendering_info(
        m_renderExtent, std::span<const vk::RenderingAttachmentInfo>(colorAttachmentInfos), depthAttachmentPtr);
    m_commandBuffer.beginRendering(&vkRenderingInfo);
    if (setViewport({0.0f,
                     0.0f,
                     static_cast<float>(m_renderExtent.width),
                     static_cast<float>(m_renderExtent.height),
                     0.0f,
                     1.0f}) != RHIResult::Success ||
        setScissor({0, 0, m_renderExtent.width, m_renderExtent.height}) != RHIResult::Success) {
        m_commandBuffer.endRendering();
        return RHIResult::InvalidArgument;
    }

    m_rendering = true;
    m_currentAttachment = firstColorAttachment;
    return RHIResult::Success;
}

RHIResult VulkanRHIDevice::CommandContext::endRendering()
{
    if (!m_commandBuffer || !m_rendering) {
        return RHIResult::InvalidArgument;
    }

    m_commandBuffer.endRendering();
    m_rendering = false;
    return RHIResult::Success;
}

RHIResult VulkanRHIDevice::CommandContext::clearColor(const ClearColorValue& color)
{
    if (!m_commandBuffer) {
        return RHIResult::NotReady;
    }

    if (m_rendering && !m_currentAttachment.isValid()) {
        return RHIResult::InvalidArgument;
    }

    const ImageHandle targetHandle = m_currentAttachment.isValid() ? m_currentAttachment : m_device.m_currentBackbufferHandle;
    ImageResource* image = m_device.findImage(targetHandle);
    if (image == nullptr || luna::is_depth_format(image->desc.format)) {
        return RHIResult::InvalidArgument;
    }

    if (m_rendering) {
        vk::ClearAttachment clearAttachment{};
        clearAttachment.aspectMask = vk::ImageAspectFlagBits::eColor;
        clearAttachment.colorAttachment = 0;
        clearAttachment.clearValue.color =
            vk::ClearColorValue(std::array<float, 4>{color.r, color.g, color.b, color.a});

        vk::ClearRect clearRect{};
        clearRect.rect = vk::Rect2D{{0, 0}, m_renderExtent};
        clearRect.baseArrayLayer = 0;
        clearRect.layerCount = 1;
        m_commandBuffer.clearAttachments(1, &clearAttachment, 1, &clearRect);
        return RHIResult::Success;
    }

    vk::ImageLayout currentLayout = vk::ImageLayout::eUndefined;
    if (targetHandle == m_device.m_currentBackbufferHandle &&
        m_device.m_swapchainImageIndex < m_device.m_context._swapchainImageLayouts.size()) {
        currentLayout = static_cast<vk::ImageLayout>(m_device.m_context._swapchainImageLayouts[m_device.m_swapchainImageIndex]);
    } else if (!m_device.getImageRangeLayout(
                   *image, 0, image->desc.mipLevels, 0, m_device.imageLayerCount(image->desc), &currentLayout)) {
        return RHIResult::InvalidArgument;
    }

    vkutil::transition_image(m_commandBuffer, image->image.image, currentLayout, vk::ImageLayout::eTransferDstOptimal);

    const vk::ClearColorValue clearValue(std::array<float, 4>{color.r, color.g, color.b, color.a});
    vk::ImageSubresourceRange subresourceRange{};
    subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
    subresourceRange.baseMipLevel = 0;
    subresourceRange.levelCount = image->desc.mipLevels;
    subresourceRange.baseArrayLayer = 0;
    subresourceRange.layerCount = m_device.imageLayerCount(image->desc);
    m_commandBuffer.clearColorImage(
        image->image.image, vk::ImageLayout::eTransferDstOptimal, &clearValue, 1, &subresourceRange);

    m_device.setImageRangeLayout(*image,
                                 0,
                                 image->desc.mipLevels,
                                 0,
                                 m_device.imageLayerCount(image->desc),
                                 vk::ImageLayout::eTransferDstOptimal);
    if (targetHandle == m_device.m_currentBackbufferHandle &&
        m_device.m_swapchainImageIndex < m_device.m_context._swapchainImageLayouts.size()) {
        m_device.m_context._swapchainImageLayouts[m_device.m_swapchainImageIndex] = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    }

    return RHIResult::Success;
}

RHIResult VulkanRHIDevice::CommandContext::imageBarrier(const ImageBarrierInfo& barrierInfo)
{
    if (!m_commandBuffer || m_rendering || !barrierInfo.image.isValid() || barrierInfo.mipCount == 0 ||
        barrierInfo.layerCount == 0 || barrierInfo.newLayout == ImageLayout::Undefined ||
        !validate_stage_access_pair(barrierInfo.srcStage, barrierInfo.srcAccess) ||
        !validate_stage_access_pair(barrierInfo.dstStage, barrierInfo.dstAccess)) {
        return RHIResult::InvalidArgument;
    }

    ImageResource* image = m_device.findImage(barrierInfo.image);
    if (image == nullptr) {
        return RHIResult::InvalidArgument;
    }

    if (!m_device.validateImageRange(
            *image, barrierInfo.baseMipLevel, barrierInfo.mipCount, barrierInfo.baseArrayLayer, barrierInfo.layerCount)) {
        return RHIResult::InvalidArgument;
    }

    const vk::ImageLayout targetLayout = to_vulkan_image_layout(barrierInfo.newLayout);
    std::vector<vk::ImageMemoryBarrier2> barriers;
    barriers.reserve(static_cast<size_t>(barrierInfo.mipCount) * barrierInfo.layerCount);
    for (uint32_t mipLevel = barrierInfo.baseMipLevel; mipLevel < barrierInfo.baseMipLevel + barrierInfo.mipCount; ++mipLevel) {
        for (uint32_t arrayLayer = barrierInfo.baseArrayLayer;
             arrayLayer < barrierInfo.baseArrayLayer + barrierInfo.layerCount;
             ++arrayLayer) {
            vk::ImageLayout currentLayout = vk::ImageLayout::eUndefined;
            if (barrierInfo.image == m_device.m_currentBackbufferHandle &&
                m_device.m_swapchainImageIndex < m_device.m_context._swapchainImageLayouts.size()) {
                currentLayout =
                    static_cast<vk::ImageLayout>(m_device.m_context._swapchainImageLayouts[m_device.m_swapchainImageIndex]);
            } else {
                currentLayout = m_device.getImageSubresourceLayout(*image, mipLevel, arrayLayer);
            }

            if (barrierInfo.oldLayout != ImageLayout::Undefined &&
                currentLayout != to_vulkan_image_layout(barrierInfo.oldLayout)) {
                return RHIResult::InvalidArgument;
            }

            vk::ImageMemoryBarrier2 barrier{};
            barrier.srcStageMask = to_vulkan_pipeline_stages(barrierInfo.srcStage);
            barrier.srcAccessMask = to_vulkan_access_flags(barrierInfo.srcAccess);
            barrier.dstStageMask = to_vulkan_pipeline_stages(barrierInfo.dstStage);
            barrier.dstAccessMask = to_vulkan_access_flags(barrierInfo.dstAccess);
            barrier.oldLayout = currentLayout;
            barrier.newLayout = targetLayout;
            barrier.image = image->image.image;
            barrier.subresourceRange.aspectMask = to_vulkan_image_aspect_flags(barrierInfo.aspect);
            barrier.subresourceRange.baseMipLevel = mipLevel;
            barrier.subresourceRange.levelCount = 1;
            barrier.subresourceRange.baseArrayLayer = arrayLayer;
            barrier.subresourceRange.layerCount = 1;
            barriers.push_back(barrier);
        }
    }

    vk::DependencyInfo dependencyInfo{};
    dependencyInfo.imageMemoryBarrierCount = static_cast<uint32_t>(barriers.size());
    dependencyInfo.pImageMemoryBarriers = barriers.data();
    m_commandBuffer.pipelineBarrier2(&dependencyInfo);

    m_device.setImageRangeLayout(*image,
                                 barrierInfo.baseMipLevel,
                                 barrierInfo.mipCount,
                                 barrierInfo.baseArrayLayer,
                                 barrierInfo.layerCount,
                                 targetLayout);
    if (barrierInfo.image == m_device.m_currentBackbufferHandle &&
        m_device.m_swapchainImageIndex < m_device.m_context._swapchainImageLayouts.size()) {
        m_device.m_context._swapchainImageLayouts[m_device.m_swapchainImageIndex] = static_cast<VkImageLayout>(targetLayout);
    }

    return RHIResult::Success;
}

RHIResult VulkanRHIDevice::CommandContext::bufferBarrier(const BufferBarrierInfo& barrierInfo)
{
    if (!m_commandBuffer || m_rendering || !barrierInfo.buffer.isValid() ||
        !validate_stage_access_pair(barrierInfo.srcStage, barrierInfo.srcAccess) ||
        !validate_stage_access_pair(barrierInfo.dstStage, barrierInfo.dstAccess)) {
        return RHIResult::InvalidArgument;
    }

    const BufferResource* buffer = m_device.findBuffer(barrierInfo.buffer);
    if (buffer == nullptr || !buffer->buffer.buffer || barrierInfo.offset > buffer->desc.size) {
        return RHIResult::InvalidArgument;
    }

    const uint64_t size = barrierInfo.size == 0 ? (buffer->desc.size - barrierInfo.offset) : barrierInfo.size;
    if (size == 0 || barrierInfo.offset + size > buffer->desc.size) {
        return RHIResult::InvalidArgument;
    }

    vk::BufferMemoryBarrier2 barrier{};
    barrier.srcStageMask = to_vulkan_pipeline_stages(barrierInfo.srcStage);
    barrier.srcAccessMask = to_vulkan_access_flags(barrierInfo.srcAccess);
    barrier.dstStageMask = to_vulkan_pipeline_stages(barrierInfo.dstStage);
    barrier.dstAccessMask = to_vulkan_access_flags(barrierInfo.dstAccess);
    barrier.buffer = buffer->buffer.buffer;
    barrier.offset = static_cast<vk::DeviceSize>(barrierInfo.offset);
    barrier.size = barrierInfo.size == 0 ? VK_WHOLE_SIZE : static_cast<vk::DeviceSize>(barrierInfo.size);

    vk::DependencyInfo dependencyInfo{};
    dependencyInfo.bufferMemoryBarrierCount = 1;
    dependencyInfo.pBufferMemoryBarriers = &barrier;
    m_commandBuffer.pipelineBarrier2(&dependencyInfo);
    return RHIResult::Success;
}

RHIResult VulkanRHIDevice::CommandContext::transitionImage(ImageHandle imageHandle, luna::ImageLayout newLayout)
{
    ImageResource* image = m_device.findImage(imageHandle);
    if (!m_commandBuffer || m_rendering || image == nullptr) {
        return RHIResult::InvalidArgument;
    }

    const uint32_t fullLayerCount = m_device.imageLayerCount(image->desc);
    return imageBarrier({.image = imageHandle,
                         .oldLayout = ImageLayout::Undefined,
                         .newLayout = newLayout,
                         .srcStage = PipelineStage::AllCommands,
                         .dstStage = PipelineStage::AllCommands,
                         .srcAccess = ResourceAccess::MemoryWrite,
                         .dstAccess = ResourceAccess::MemoryRead | ResourceAccess::MemoryWrite,
                         .aspect = default_image_aspect(image->desc),
                         .baseMipLevel = 0,
                         .mipCount = image->desc.mipLevels,
                         .baseArrayLayer = 0,
                         .layerCount = fullLayerCount});
}

RHIResult VulkanRHIDevice::CommandContext::copyImage(const ImageCopyInfo& copyInfo)
{
    if (!m_commandBuffer || m_rendering || !copyInfo.source.isValid() || !copyInfo.destination.isValid()) {
        return RHIResult::InvalidArgument;
    }

    ImageResource* sourceImage = m_device.findImage(copyInfo.source);
    ImageResource* destinationImage = m_device.findImage(copyInfo.destination);
    if (sourceImage == nullptr || destinationImage == nullptr) {
        return RHIResult::InvalidArgument;
    }

    vk::ImageLayout sourceLayout = vk::ImageLayout::eUndefined;
    if (copyInfo.source == m_device.m_currentBackbufferHandle &&
        m_device.m_swapchainImageIndex < m_device.m_context._swapchainImageLayouts.size()) {
        sourceLayout = static_cast<vk::ImageLayout>(m_device.m_context._swapchainImageLayouts[m_device.m_swapchainImageIndex]);
    } else if (!m_device.getImageRangeLayout(*sourceImage,
                                             0,
                                             sourceImage->desc.mipLevels,
                                             0,
                                             m_device.imageLayerCount(sourceImage->desc),
                                             &sourceLayout)) {
        return RHIResult::InvalidArgument;
    }

    vk::ImageLayout destinationLayout = vk::ImageLayout::eUndefined;
    if (copyInfo.destination == m_device.m_currentBackbufferHandle &&
        m_device.m_swapchainImageIndex < m_device.m_context._swapchainImageLayouts.size()) {
        destinationLayout =
            static_cast<vk::ImageLayout>(m_device.m_context._swapchainImageLayouts[m_device.m_swapchainImageIndex]);
    } else if (!m_device.getImageRangeLayout(*destinationImage,
                                             0,
                                             destinationImage->desc.mipLevels,
                                             0,
                                             m_device.imageLayerCount(destinationImage->desc),
                                             &destinationLayout)) {
        return RHIResult::InvalidArgument;
    }

    if (sourceLayout != vk::ImageLayout::eTransferSrcOptimal ||
        destinationLayout != vk::ImageLayout::eTransferDstOptimal) {
        return RHIResult::InvalidArgument;
    }

    const vk::Extent2D sourceExtent{
        copyInfo.sourceWidth > 0 ? copyInfo.sourceWidth : sourceImage->desc.width,
        copyInfo.sourceHeight > 0 ? copyInfo.sourceHeight : sourceImage->desc.height,
    };
    const vk::Extent2D destinationExtent{
        copyInfo.destinationWidth > 0 ? copyInfo.destinationWidth : destinationImage->desc.width,
        copyInfo.destinationHeight > 0 ? copyInfo.destinationHeight : destinationImage->desc.height,
    };

    if (sourceExtent.width == 0 || sourceExtent.height == 0 || destinationExtent.width == 0 ||
        destinationExtent.height == 0) {
        return RHIResult::InvalidArgument;
    }

    vkutil::copy_image_to_image(
        m_commandBuffer, sourceImage->image.image, destinationImage->image.image, sourceExtent, destinationExtent);
    return RHIResult::Success;
}

RHIResult VulkanRHIDevice::CommandContext::copyBuffer(const BufferCopyInfo& copyInfo)
{
    if (!m_commandBuffer || m_rendering || !copyInfo.source.isValid() || !copyInfo.destination.isValid() || copyInfo.size == 0) {
        return RHIResult::InvalidArgument;
    }

    const BufferResource* sourceBuffer = m_device.findBuffer(copyInfo.source);
    const BufferResource* destinationBuffer = m_device.findBuffer(copyInfo.destination);
    if (sourceBuffer == nullptr || destinationBuffer == nullptr || !sourceBuffer->buffer.buffer ||
        !destinationBuffer->buffer.buffer || copyInfo.sourceOffset + copyInfo.size > sourceBuffer->desc.size ||
        copyInfo.destinationOffset + copyInfo.size > destinationBuffer->desc.size) {
        return RHIResult::InvalidArgument;
    }

    vk::BufferCopy region{};
    region.srcOffset = static_cast<vk::DeviceSize>(copyInfo.sourceOffset);
    region.dstOffset = static_cast<vk::DeviceSize>(copyInfo.destinationOffset);
    region.size = static_cast<vk::DeviceSize>(copyInfo.size);
    m_commandBuffer.copyBuffer(sourceBuffer->buffer.buffer, destinationBuffer->buffer.buffer, 1, &region);
    return RHIResult::Success;
}

RHIResult VulkanRHIDevice::CommandContext::copyBufferToImage(const BufferImageCopyInfo& copyInfo)
{
    if (!m_commandBuffer || m_rendering || !copyInfo.buffer.isValid() || !copyInfo.image.isValid()) {
        return RHIResult::InvalidArgument;
    }

    const BufferResource* buffer = m_device.findBuffer(copyInfo.buffer);
    ImageResource* image = m_device.findImage(copyInfo.image);
    if (buffer == nullptr || image == nullptr || !buffer->buffer.buffer || !image->image.image) {
        return RHIResult::InvalidArgument;
    }

    if (!m_device.validateImageRange(*image, copyInfo.mipLevel, 1, copyInfo.baseArrayLayer, copyInfo.layerCount)) {
        return RHIResult::InvalidArgument;
    }

    vk::ImageLayout imageLayout = vk::ImageLayout::eUndefined;
    if (copyInfo.image == m_device.m_currentBackbufferHandle &&
        m_device.m_swapchainImageIndex < m_device.m_context._swapchainImageLayouts.size()) {
        imageLayout = static_cast<vk::ImageLayout>(m_device.m_context._swapchainImageLayouts[m_device.m_swapchainImageIndex]);
    } else if (!m_device.getImageRangeLayout(
                   *image, copyInfo.mipLevel, 1, copyInfo.baseArrayLayer, copyInfo.layerCount, &imageLayout)) {
        return RHIResult::InvalidArgument;
    }
    if (imageLayout != vk::ImageLayout::eTransferDstOptimal) {
        return RHIResult::InvalidArgument;
    }

    const uint32_t extentWidth = copyInfo.imageExtentWidth > 0 ? copyInfo.imageExtentWidth : image->desc.width;
    const uint32_t extentHeight = copyInfo.imageExtentHeight > 0 ? copyInfo.imageExtentHeight : image->desc.height;
    const uint32_t extentDepth = copyInfo.imageExtentDepth > 0
                                     ? copyInfo.imageExtentDepth
                                     : (image->desc.type == ImageType::Image3D ? image->desc.depth : 1u);
    if (extentWidth == 0 || extentHeight == 0 || extentDepth == 0) {
        return RHIResult::InvalidArgument;
    }

    vk::BufferImageCopy region{};
    region.bufferOffset = static_cast<vk::DeviceSize>(copyInfo.bufferOffset);
    region.bufferRowLength = copyInfo.bufferRowLength;
    region.bufferImageHeight = copyInfo.bufferImageHeight;
    region.imageSubresource.aspectMask = to_vulkan_image_aspect_flags(copyInfo.aspect);
    region.imageSubresource.mipLevel = copyInfo.mipLevel;
    region.imageSubresource.baseArrayLayer = copyInfo.baseArrayLayer;
    region.imageSubresource.layerCount = copyInfo.layerCount;
    region.imageOffset =
        vk::Offset3D{static_cast<int32_t>(copyInfo.imageOffsetX),
                     static_cast<int32_t>(copyInfo.imageOffsetY),
                     static_cast<int32_t>(copyInfo.imageOffsetZ)};
    region.imageExtent = vk::Extent3D{extentWidth, extentHeight, extentDepth};

    m_commandBuffer.copyBufferToImage(buffer->buffer.buffer, image->image.image, vk::ImageLayout::eTransferDstOptimal, 1, &region);
    return RHIResult::Success;
}

RHIResult VulkanRHIDevice::CommandContext::copyImageToBuffer(const BufferImageCopyInfo& copyInfo)
{
    if (!m_commandBuffer || m_rendering || !copyInfo.buffer.isValid() || !copyInfo.image.isValid()) {
        return RHIResult::InvalidArgument;
    }

    const BufferResource* buffer = m_device.findBuffer(copyInfo.buffer);
    ImageResource* image = m_device.findImage(copyInfo.image);
    if (buffer == nullptr || image == nullptr || !buffer->buffer.buffer || !image->image.image) {
        return RHIResult::InvalidArgument;
    }

    if (!m_device.validateImageRange(*image, copyInfo.mipLevel, 1, copyInfo.baseArrayLayer, copyInfo.layerCount)) {
        return RHIResult::InvalidArgument;
    }

    vk::ImageLayout imageLayout = vk::ImageLayout::eUndefined;
    if (copyInfo.image == m_device.m_currentBackbufferHandle &&
        m_device.m_swapchainImageIndex < m_device.m_context._swapchainImageLayouts.size()) {
        imageLayout = static_cast<vk::ImageLayout>(m_device.m_context._swapchainImageLayouts[m_device.m_swapchainImageIndex]);
    } else if (!m_device.getImageRangeLayout(
                   *image, copyInfo.mipLevel, 1, copyInfo.baseArrayLayer, copyInfo.layerCount, &imageLayout)) {
        return RHIResult::InvalidArgument;
    }
    if (imageLayout != vk::ImageLayout::eTransferSrcOptimal) {
        return RHIResult::InvalidArgument;
    }

    const uint32_t extentWidth = copyInfo.imageExtentWidth > 0 ? copyInfo.imageExtentWidth : image->desc.width;
    const uint32_t extentHeight = copyInfo.imageExtentHeight > 0 ? copyInfo.imageExtentHeight : image->desc.height;
    const uint32_t extentDepth = copyInfo.imageExtentDepth > 0
                                     ? copyInfo.imageExtentDepth
                                     : (image->desc.type == ImageType::Image3D ? image->desc.depth : 1u);
    if (extentWidth == 0 || extentHeight == 0 || extentDepth == 0) {
        return RHIResult::InvalidArgument;
    }

    vk::BufferImageCopy region{};
    region.bufferOffset = static_cast<vk::DeviceSize>(copyInfo.bufferOffset);
    region.bufferRowLength = copyInfo.bufferRowLength;
    region.bufferImageHeight = copyInfo.bufferImageHeight;
    region.imageSubresource.aspectMask = to_vulkan_image_aspect_flags(copyInfo.aspect);
    region.imageSubresource.mipLevel = copyInfo.mipLevel;
    region.imageSubresource.baseArrayLayer = copyInfo.baseArrayLayer;
    region.imageSubresource.layerCount = copyInfo.layerCount;
    region.imageOffset =
        vk::Offset3D{static_cast<int32_t>(copyInfo.imageOffsetX),
                     static_cast<int32_t>(copyInfo.imageOffsetY),
                     static_cast<int32_t>(copyInfo.imageOffsetZ)};
    region.imageExtent = vk::Extent3D{extentWidth, extentHeight, extentDepth};

    m_commandBuffer.copyImageToBuffer(image->image.image, vk::ImageLayout::eTransferSrcOptimal, buffer->buffer.buffer, 1, &region);
    return RHIResult::Success;
}

RHIResult VulkanRHIDevice::CommandContext::bindGraphicsPipeline(PipelineHandle pipeline)
{
    if (!m_commandBuffer) {
        return RHIResult::NotReady;
    }

    const PipelineResource* pipelineResource = m_device.findPipeline(pipeline);
    if (pipelineResource == nullptr || !pipelineResource->pipeline ||
        pipelineResource->bindPoint != vk::PipelineBindPoint::eGraphics) {
        return RHIResult::InvalidArgument;
    }

    m_commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, pipelineResource->pipeline);
    m_boundPipelineLayout = pipelineResource->layout;
    m_boundPipelineBindPoint = vk::PipelineBindPoint::eGraphics;
    return RHIResult::Success;
}

RHIResult VulkanRHIDevice::CommandContext::bindComputePipeline(PipelineHandle pipeline)
{
    if (!m_commandBuffer || m_rendering) {
        return RHIResult::InvalidArgument;
    }

    const PipelineResource* pipelineResource = m_device.findPipeline(pipeline);
    if (pipelineResource == nullptr || !pipelineResource->pipeline ||
        pipelineResource->bindPoint != vk::PipelineBindPoint::eCompute) {
        return RHIResult::InvalidArgument;
    }

    m_commandBuffer.bindPipeline(vk::PipelineBindPoint::eCompute, pipelineResource->pipeline);
    m_boundPipelineLayout = pipelineResource->layout;
    m_boundPipelineBindPoint = vk::PipelineBindPoint::eCompute;
    return RHIResult::Success;
}

RHIResult VulkanRHIDevice::CommandContext::bindVertexBuffer(BufferHandle buffer, uint64_t offset)
{
    if (!m_commandBuffer) {
        return RHIResult::NotReady;
    }

    const BufferResource* bufferResource = m_device.findBuffer(buffer);
    if (bufferResource == nullptr || !bufferResource->buffer.buffer) {
        return RHIResult::InvalidArgument;
    }

    const vk::DeviceSize vkOffset = static_cast<vk::DeviceSize>(offset);
    m_commandBuffer.bindVertexBuffers(0, 1, &bufferResource->buffer.buffer, &vkOffset);
    return RHIResult::Success;
}

RHIResult VulkanRHIDevice::CommandContext::bindIndexBuffer(BufferHandle buffer,
                                                           IndexFormat indexFormat,
                                                           uint64_t offset)
{
    if (!m_commandBuffer) {
        return RHIResult::NotReady;
    }

    const BufferResource* bufferResource = m_device.findBuffer(buffer);
    if (bufferResource == nullptr || !bufferResource->buffer.buffer) {
        return RHIResult::InvalidArgument;
    }

    m_commandBuffer.bindIndexBuffer(
        bufferResource->buffer.buffer, static_cast<vk::DeviceSize>(offset), to_vulkan_index_type(indexFormat));
    return RHIResult::Success;
}

RHIResult VulkanRHIDevice::CommandContext::setViewport(const Viewport& viewport)
{
    if (!m_commandBuffer || viewport.width <= 0.0f || viewport.height <= 0.0f || viewport.minDepth > viewport.maxDepth) {
        return RHIResult::InvalidArgument;
    }

    const vk::Viewport vkViewport{
        viewport.x,
        viewport.y,
        viewport.width,
        viewport.height,
        viewport.minDepth,
        viewport.maxDepth,
    };
    m_commandBuffer.setViewport(0, 1, &vkViewport);
    return RHIResult::Success;
}

RHIResult VulkanRHIDevice::CommandContext::setScissor(const ScissorRect& scissor)
{
    if (!m_commandBuffer || scissor.width == 0 || scissor.height == 0) {
        return RHIResult::InvalidArgument;
    }

    const vk::Rect2D vkScissor{
        {scissor.x, scissor.y},
        {scissor.width, scissor.height},
    };
    m_commandBuffer.setScissor(0, 1, &vkScissor);
    return RHIResult::Success;
}

RHIResult VulkanRHIDevice::CommandContext::bindResourceSet(ResourceSetHandle resourceSet,
                                                           std::span<const uint32_t> dynamicOffsets)
{
    if (!m_commandBuffer || !m_boundPipelineLayout) {
        return RHIResult::NotReady;
    }

    const ResourceSetResource* resourceSetResource = m_device.findResourceSet(resourceSet);
    if (resourceSetResource == nullptr || !resourceSetResource->set) {
        return RHIResult::InvalidArgument;
    }

    const ResourceLayoutResource* layout = m_device.findResourceLayout(resourceSetResource->layoutHandle);
    if (layout == nullptr || !layout->layout) {
        return RHIResult::InvalidArgument;
    }

    m_commandBuffer.bindDescriptorSets(
        m_boundPipelineBindPoint,
        m_boundPipelineLayout,
        layout->desc.setIndex,
        1,
        &resourceSetResource->set,
        static_cast<uint32_t>(dynamicOffsets.size()),
        dynamicOffsets.data());
    return RHIResult::Success;
}

RHIResult VulkanRHIDevice::CommandContext::pushConstants(const void* data,
                                                         uint32_t size,
                                                         uint32_t offset,
                                                         ShaderType visibility)
{
    if (!m_commandBuffer || !m_boundPipelineLayout || data == nullptr || size == 0) {
        return RHIResult::InvalidArgument;
    }

    m_commandBuffer.pushConstants(m_boundPipelineLayout, to_vulkan_shader_stages(visibility), offset, size, data);
    return RHIResult::Success;
}

RHIResult VulkanRHIDevice::CommandContext::draw(const DrawArguments& arguments)
{
    if (!m_commandBuffer || m_boundPipelineBindPoint != vk::PipelineBindPoint::eGraphics) {
        return RHIResult::NotReady;
    }

    m_commandBuffer.draw(arguments.vertexCount, arguments.instanceCount, arguments.firstVertex, arguments.firstInstance);
    return RHIResult::Success;
}

RHIResult VulkanRHIDevice::CommandContext::drawIndexed(const IndexedDrawArguments& arguments)
{
    if (!m_commandBuffer || m_boundPipelineBindPoint != vk::PipelineBindPoint::eGraphics) {
        return RHIResult::NotReady;
    }

    m_commandBuffer.drawIndexed(arguments.indexCount,
                                arguments.instanceCount,
                                arguments.firstIndex,
                                arguments.vertexOffset,
                                arguments.firstInstance);
    return RHIResult::Success;
}

RHIResult VulkanRHIDevice::CommandContext::drawIndirect(BufferHandle argumentsBuffer, uint64_t offset)
{
    if (!m_commandBuffer || m_boundPipelineBindPoint != vk::PipelineBindPoint::eGraphics) {
        return RHIResult::NotReady;
    }

    const BufferResource* buffer = m_device.findBuffer(argumentsBuffer);
    if (buffer == nullptr || !buffer->buffer.buffer || offset + sizeof(VkDrawIndirectCommand) > buffer->desc.size) {
        return RHIResult::InvalidArgument;
    }

    m_commandBuffer.drawIndirect(buffer->buffer.buffer,
                                 static_cast<vk::DeviceSize>(offset),
                                 1,
                                 sizeof(VkDrawIndirectCommand));
    return RHIResult::Success;
}

RHIResult VulkanRHIDevice::CommandContext::dispatch(uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ)
{
    if (!m_commandBuffer || m_rendering || m_boundPipelineBindPoint != vk::PipelineBindPoint::eCompute ||
        groupCountX == 0 || groupCountY == 0 || groupCountZ == 0) {
        return RHIResult::InvalidArgument;
    }

    m_commandBuffer.dispatch(groupCountX, groupCountY, groupCountZ);
    return RHIResult::Success;
}

RHIResult VulkanRHIDevice::CommandContext::dispatchIndirect(BufferHandle argumentsBuffer, uint64_t offset)
{
    if (!m_commandBuffer || m_rendering || m_boundPipelineBindPoint != vk::PipelineBindPoint::eCompute) {
        return RHIResult::InvalidArgument;
    }

    const BufferResource* buffer = m_device.findBuffer(argumentsBuffer);
    if (buffer == nullptr || !buffer->buffer.buffer || offset + sizeof(VkDispatchIndirectCommand) > buffer->desc.size) {
        return RHIResult::InvalidArgument;
    }

    m_commandBuffer.dispatchIndirect(buffer->buffer.buffer, static_cast<vk::DeviceSize>(offset));
    return RHIResult::Success;
}

RHIResult VulkanRHIDevice::CommandContext::beginDebugLabel(std::string_view label, const ClearColorValue& color)
{
    if (!m_commandBuffer || label.empty()) {
        return RHIResult::InvalidArgument;
    }

    std::string labelText(label);
    LUNA_CORE_INFO("GPU Label Begin: {}", labelText);

    if (const PFN_vkCmdBeginDebugUtilsLabelEXT fn = begin_debug_utils_label_fn(m_device.m_context._device); fn != nullptr) {
        VkDebugUtilsLabelEXT labelInfo{};
        labelInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
        labelInfo.pLabelName = labelText.c_str();
        labelInfo.color[0] = color.r;
        labelInfo.color[1] = color.g;
        labelInfo.color[2] = color.b;
        labelInfo.color[3] = color.a;
        fn(static_cast<VkCommandBuffer>(m_commandBuffer), &labelInfo);
    }

    m_debugLabelStack.push_back(std::move(labelText));
    return RHIResult::Success;
}

RHIResult VulkanRHIDevice::CommandContext::endDebugLabel()
{
    if (!m_commandBuffer || m_debugLabelStack.empty()) {
        return RHIResult::InvalidArgument;
    }

    if (const PFN_vkCmdEndDebugUtilsLabelEXT fn = end_debug_utils_label_fn(m_device.m_context._device); fn != nullptr) {
        fn(static_cast<VkCommandBuffer>(m_commandBuffer));
    }

    LUNA_CORE_INFO("GPU Label End: {}", m_debugLabelStack.back());
    m_debugLabelStack.pop_back();
    return RHIResult::Success;
}



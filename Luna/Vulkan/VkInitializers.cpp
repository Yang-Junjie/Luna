#include "VkInitializers.h"

vk::CommandPoolCreateInfo vkinit::commandPoolCreateInfo(uint32_t queue_family_index,
                                                           vk::CommandPoolCreateFlags flags)
{
    vk::CommandPoolCreateInfo info{};
    info.queueFamilyIndex = queue_family_index;
    info.flags = flags;
    return info;
}

vk::CommandBufferAllocateInfo vkinit::commandBufferAllocateInfo(vk::CommandPool pool, uint32_t count)
{
    vk::CommandBufferAllocateInfo info{};
    info.commandPool = pool;
    info.commandBufferCount = count;
    info.level = vk::CommandBufferLevel::ePrimary;
    return info;
}

vk::CommandBufferBeginInfo vkinit::commandBufferBeginInfo(vk::CommandBufferUsageFlags flags)
{
    vk::CommandBufferBeginInfo info{};
    info.pInheritanceInfo = nullptr;
    info.flags = flags;
    return info;
}

vk::CommandBufferSubmitInfo vkinit::commandBufferSubmitInfo(vk::CommandBuffer cmd)
{
    vk::CommandBufferSubmitInfo info{};
    info.commandBuffer = cmd;
    info.deviceMask = 0;
    return info;
}

vk::FenceCreateInfo vkinit::fenceCreateInfo(vk::FenceCreateFlags flags)
{
    vk::FenceCreateInfo info{};
    info.flags = flags;
    return info;
}

vk::SemaphoreCreateInfo vkinit::semaphoreCreateInfo(vk::SemaphoreCreateFlags flags)
{
    vk::SemaphoreCreateInfo info{};
    info.flags = flags;
    return info;
}

vk::SubmitInfo2 vkinit::submitInfo(const vk::CommandBufferSubmitInfo* cmd,
                                    const vk::SemaphoreSubmitInfo* signal_semaphore_info,
                                    const vk::SemaphoreSubmitInfo* wait_semaphore_info)
{
    vk::SubmitInfo2 info{};
    info.waitSemaphoreInfoCount = wait_semaphore_info == nullptr ? 0u : 1u;
    info.pWaitSemaphoreInfos = wait_semaphore_info;
    info.signalSemaphoreInfoCount = signal_semaphore_info == nullptr ? 0u : 1u;
    info.pSignalSemaphoreInfos = signal_semaphore_info;
    info.commandBufferInfoCount = cmd == nullptr ? 0u : 1u;
    info.pCommandBufferInfos = cmd;
    return info;
}

vk::PresentInfoKHR vkinit::presentInfo()
{
    vk::PresentInfoKHR info{};
    info.swapchainCount = 0;
    info.pSwapchains = nullptr;
    info.pWaitSemaphores = nullptr;
    info.waitSemaphoreCount = 0;
    info.pImageIndices = nullptr;
    return info;
}

vk::RenderingAttachmentInfo vkinit::attachmentInfo(vk::ImageView view,
                                                    const vk::ClearValue* clear,
                                                    vk::ImageLayout layout)
{
    vk::RenderingAttachmentInfo color_attachment{};
    color_attachment.imageView = view;
    color_attachment.imageLayout = layout;
    color_attachment.loadOp = clear ? vk::AttachmentLoadOp::eClear : vk::AttachmentLoadOp::eLoad;
    color_attachment.storeOp = vk::AttachmentStoreOp::eStore;
    if (clear != nullptr) {
        color_attachment.clearValue = *clear;
    }
    return color_attachment;
}

vk::RenderingAttachmentInfo vkinit::depthAttachmentInfo(vk::ImageView view, vk::ImageLayout layout)
{
    vk::RenderingAttachmentInfo depth_attachment{};
    depth_attachment.imageView = view;
    depth_attachment.imageLayout = layout;
    depth_attachment.loadOp = vk::AttachmentLoadOp::eClear;
    depth_attachment.storeOp = vk::AttachmentStoreOp::eStore;
    depth_attachment.clearValue.depthStencil.depth = 1.0f;
    return depth_attachment;
}

vk::RenderingInfo vkinit::renderingInfo(vk::Extent2D render_extent,
                                         const vk::RenderingAttachmentInfo* color_attachment,
                                         const vk::RenderingAttachmentInfo* depth_attachment)
{
    vk::RenderingInfo render_info{};
    render_info.renderArea = vk::Rect2D{vk::Offset2D{0, 0}, render_extent};
    render_info.layerCount = 1;
    render_info.colorAttachmentCount = color_attachment == nullptr ? 0u : 1u;
    render_info.pColorAttachments = color_attachment;
    render_info.pDepthAttachment = depth_attachment;
    render_info.pStencilAttachment = nullptr;
    return render_info;
}

vk::ImageSubresourceRange vkinit::imageSubresourceRange(vk::ImageAspectFlags aspect_mask)
{
    vk::ImageSubresourceRange sub_image{};
    sub_image.aspectMask = aspect_mask;
    sub_image.baseMipLevel = 0;
    sub_image.levelCount = VK_REMAINING_MIP_LEVELS;
    sub_image.baseArrayLayer = 0;
    sub_image.layerCount = VK_REMAINING_ARRAY_LAYERS;
    return sub_image;
}

vk::SemaphoreSubmitInfo vkinit::semaphoreSubmitInfo(vk::PipelineStageFlags2 stage_mask, vk::Semaphore semaphore)
{
    vk::SemaphoreSubmitInfo submit_info{};
    submit_info.semaphore = semaphore;
    submit_info.stageMask = stage_mask;
    submit_info.deviceIndex = 0;
    submit_info.value = 1;
    return submit_info;
}

vk::DescriptorSetLayoutBinding vkinit::descriptorsetLayoutBinding(vk::DescriptorType type,
                                                                    vk::ShaderStageFlags stage_flags,
                                                                    uint32_t binding)
{
    vk::DescriptorSetLayoutBinding setbind{};
    setbind.binding = binding;
    setbind.descriptorCount = 1;
    setbind.descriptorType = type;
    setbind.pImmutableSamplers = nullptr;
    setbind.stageFlags = stage_flags;
    return setbind;
}

vk::DescriptorSetLayoutCreateInfo vkinit::descriptorsetLayoutCreateInfo(
    const vk::DescriptorSetLayoutBinding* bindings,
    uint32_t binding_count)
{
    vk::DescriptorSetLayoutCreateInfo info{};
    info.pBindings = bindings;
    info.bindingCount = binding_count;
    info.flags = {};
    return info;
}

vk::WriteDescriptorSet vkinit::writeDescriptorImage(vk::DescriptorType type,
                                                      vk::DescriptorSet dst_set,
                                                      const vk::DescriptorImageInfo* image_info,
                                                      uint32_t binding)
{
    vk::WriteDescriptorSet write{};
    write.dstBinding = binding;
    write.dstSet = dst_set;
    write.descriptorCount = 1;
    write.descriptorType = type;
    write.pImageInfo = image_info;
    return write;
}

vk::WriteDescriptorSet vkinit::writeDescriptorBuffer(vk::DescriptorType type,
                                                       vk::DescriptorSet dst_set,
                                                       const vk::DescriptorBufferInfo* buffer_info,
                                                       uint32_t binding)
{
    vk::WriteDescriptorSet write{};
    write.dstBinding = binding;
    write.dstSet = dst_set;
    write.descriptorCount = 1;
    write.descriptorType = type;
    write.pBufferInfo = buffer_info;
    return write;
}

vk::DescriptorBufferInfo vkinit::bufferInfo(vk::Buffer buffer, vk::DeviceSize offset, vk::DeviceSize range)
{
    vk::DescriptorBufferInfo binfo{};
    binfo.buffer = buffer;
    binfo.offset = offset;
    binfo.range = range;
    return binfo;
}

vk::ImageCreateInfo vkinit::imageCreateInfo(vk::Format format,
                                              vk::ImageUsageFlags usage_flags,
                                              vk::Extent3D extent)
{
    vk::ImageCreateInfo info{};
    info.imageType = vk::ImageType::e2D;
    info.format = format;
    info.extent = extent;
    info.mipLevels = 1;
    info.arrayLayers = 1;
    info.samples = vk::SampleCountFlagBits::e1;
    info.tiling = vk::ImageTiling::eOptimal;
    info.usage = usage_flags;
    return info;
}

vk::ImageViewCreateInfo vkinit::imageviewCreateInfo(vk::Format format,
                                                      vk::Image image,
                                                      vk::ImageAspectFlags aspect_flags)
{
    vk::ImageViewCreateInfo info{};
    info.viewType = vk::ImageViewType::e2D;
    info.image = image;
    info.format = format;
    info.subresourceRange.baseMipLevel = 0;
    info.subresourceRange.levelCount = 1;
    info.subresourceRange.baseArrayLayer = 0;
    info.subresourceRange.layerCount = 1;
    info.subresourceRange.aspectMask = aspect_flags;
    return info;
}

vk::PipelineLayoutCreateInfo vkinit::pipelineLayoutCreateInfo()
{
    vk::PipelineLayoutCreateInfo info{};
    info.flags = {};
    info.setLayoutCount = 0;
    info.pSetLayouts = nullptr;
    info.pushConstantRangeCount = 0;
    info.pPushConstantRanges = nullptr;
    return info;
}

vk::PipelineShaderStageCreateInfo vkinit::pipelineShaderStageCreateInfo(vk::ShaderStageFlagBits stage,
                                                                            vk::ShaderModule shader_module,
                                                                            const char* entry)
{
    vk::PipelineShaderStageCreateInfo info{};
    info.stage = stage;
    info.module = shader_module;
    info.pName = entry;
    return info;
}


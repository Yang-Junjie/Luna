#include "vk_initializers.h"

vk::CommandPoolCreateInfo vkinit::command_pool_create_info(uint32_t queueFamilyIndex,
                                                           vk::CommandPoolCreateFlags flags)
{
    vk::CommandPoolCreateInfo info{};
    info.queueFamilyIndex = queueFamilyIndex;
    info.flags = flags;
    return info;
}

vk::CommandBufferAllocateInfo vkinit::command_buffer_allocate_info(vk::CommandPool pool, uint32_t count)
{
    vk::CommandBufferAllocateInfo info{};
    info.commandPool = pool;
    info.commandBufferCount = count;
    info.level = vk::CommandBufferLevel::ePrimary;
    return info;
}

vk::CommandBufferBeginInfo vkinit::command_buffer_begin_info(vk::CommandBufferUsageFlags flags)
{
    vk::CommandBufferBeginInfo info{};
    info.pInheritanceInfo = nullptr;
    info.flags = flags;
    return info;
}

vk::CommandBufferSubmitInfo vkinit::command_buffer_submit_info(vk::CommandBuffer cmd)
{
    vk::CommandBufferSubmitInfo info{};
    info.commandBuffer = cmd;
    info.deviceMask = 0;
    return info;
}

vk::FenceCreateInfo vkinit::fence_create_info(vk::FenceCreateFlags flags)
{
    vk::FenceCreateInfo info{};
    info.flags = flags;
    return info;
}

vk::SemaphoreCreateInfo vkinit::semaphore_create_info(vk::SemaphoreCreateFlags flags)
{
    vk::SemaphoreCreateInfo info{};
    info.flags = flags;
    return info;
}

vk::SubmitInfo2 vkinit::submit_info(const vk::CommandBufferSubmitInfo* cmd,
                                    const vk::SemaphoreSubmitInfo* signalSemaphoreInfo,
                                    const vk::SemaphoreSubmitInfo* waitSemaphoreInfo)
{
    vk::SubmitInfo2 info{};
    info.waitSemaphoreInfoCount = waitSemaphoreInfo == nullptr ? 0u : 1u;
    info.pWaitSemaphoreInfos = waitSemaphoreInfo;
    info.signalSemaphoreInfoCount = signalSemaphoreInfo == nullptr ? 0u : 1u;
    info.pSignalSemaphoreInfos = signalSemaphoreInfo;
    info.commandBufferInfoCount = cmd == nullptr ? 0u : 1u;
    info.pCommandBufferInfos = cmd;
    return info;
}

vk::PresentInfoKHR vkinit::present_info()
{
    vk::PresentInfoKHR info{};
    info.swapchainCount = 0;
    info.pSwapchains = nullptr;
    info.pWaitSemaphores = nullptr;
    info.waitSemaphoreCount = 0;
    info.pImageIndices = nullptr;
    return info;
}

vk::RenderingAttachmentInfo vkinit::attachment_info(vk::ImageView view,
                                                    const vk::ClearValue* clear,
                                                    vk::ImageLayout layout)
{
    vk::RenderingAttachmentInfo colorAttachment{};
    colorAttachment.imageView = view;
    colorAttachment.imageLayout = layout;
    colorAttachment.loadOp = clear ? vk::AttachmentLoadOp::eClear : vk::AttachmentLoadOp::eLoad;
    colorAttachment.storeOp = vk::AttachmentStoreOp::eStore;
    if (clear != nullptr) {
        colorAttachment.clearValue = *clear;
    }
    return colorAttachment;
}

vk::RenderingAttachmentInfo vkinit::depth_attachment_info(vk::ImageView view, vk::ImageLayout layout)
{
    vk::RenderingAttachmentInfo depthAttachment{};
    depthAttachment.imageView = view;
    depthAttachment.imageLayout = layout;
    depthAttachment.loadOp = vk::AttachmentLoadOp::eClear;
    depthAttachment.storeOp = vk::AttachmentStoreOp::eStore;
    depthAttachment.clearValue.depthStencil.depth = 1.0f;
    return depthAttachment;
}

vk::RenderingInfo vkinit::rendering_info(vk::Extent2D renderExtent,
                                         const vk::RenderingAttachmentInfo* colorAttachment,
                                         const vk::RenderingAttachmentInfo* depthAttachment)
{
    vk::RenderingInfo renderInfo{};
    renderInfo.renderArea = vk::Rect2D{vk::Offset2D{0, 0}, renderExtent};
    renderInfo.layerCount = 1;
    renderInfo.colorAttachmentCount = colorAttachment == nullptr ? 0u : 1u;
    renderInfo.pColorAttachments = colorAttachment;
    renderInfo.pDepthAttachment = depthAttachment;
    renderInfo.pStencilAttachment = nullptr;
    return renderInfo;
}

vk::ImageSubresourceRange vkinit::image_subresource_range(vk::ImageAspectFlags aspectMask)
{
    vk::ImageSubresourceRange subImage{};
    subImage.aspectMask = aspectMask;
    subImage.baseMipLevel = 0;
    subImage.levelCount = VK_REMAINING_MIP_LEVELS;
    subImage.baseArrayLayer = 0;
    subImage.layerCount = VK_REMAINING_ARRAY_LAYERS;
    return subImage;
}

vk::SemaphoreSubmitInfo vkinit::semaphore_submit_info(vk::PipelineStageFlags2 stageMask, vk::Semaphore semaphore)
{
    vk::SemaphoreSubmitInfo submitInfo{};
    submitInfo.semaphore = semaphore;
    submitInfo.stageMask = stageMask;
    submitInfo.deviceIndex = 0;
    submitInfo.value = 1;
    return submitInfo;
}

vk::DescriptorSetLayoutBinding vkinit::descriptorset_layout_binding(vk::DescriptorType type,
                                                                    vk::ShaderStageFlags stageFlags,
                                                                    uint32_t binding)
{
    vk::DescriptorSetLayoutBinding setbind{};
    setbind.binding = binding;
    setbind.descriptorCount = 1;
    setbind.descriptorType = type;
    setbind.pImmutableSamplers = nullptr;
    setbind.stageFlags = stageFlags;
    return setbind;
}

vk::DescriptorSetLayoutCreateInfo vkinit::descriptorset_layout_create_info(
    const vk::DescriptorSetLayoutBinding* bindings,
    uint32_t bindingCount)
{
    vk::DescriptorSetLayoutCreateInfo info{};
    info.pBindings = bindings;
    info.bindingCount = bindingCount;
    info.flags = {};
    return info;
}

vk::WriteDescriptorSet vkinit::write_descriptor_image(vk::DescriptorType type,
                                                      vk::DescriptorSet dstSet,
                                                      const vk::DescriptorImageInfo* imageInfo,
                                                      uint32_t binding)
{
    vk::WriteDescriptorSet write{};
    write.dstBinding = binding;
    write.dstSet = dstSet;
    write.descriptorCount = 1;
    write.descriptorType = type;
    write.pImageInfo = imageInfo;
    return write;
}

vk::WriteDescriptorSet vkinit::write_descriptor_buffer(vk::DescriptorType type,
                                                       vk::DescriptorSet dstSet,
                                                       const vk::DescriptorBufferInfo* bufferInfo,
                                                       uint32_t binding)
{
    vk::WriteDescriptorSet write{};
    write.dstBinding = binding;
    write.dstSet = dstSet;
    write.descriptorCount = 1;
    write.descriptorType = type;
    write.pBufferInfo = bufferInfo;
    return write;
}

vk::DescriptorBufferInfo vkinit::buffer_info(vk::Buffer buffer, vk::DeviceSize offset, vk::DeviceSize range)
{
    vk::DescriptorBufferInfo binfo{};
    binfo.buffer = buffer;
    binfo.offset = offset;
    binfo.range = range;
    return binfo;
}

vk::ImageCreateInfo vkinit::image_create_info(vk::Format format,
                                              vk::ImageUsageFlags usageFlags,
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
    info.usage = usageFlags;
    return info;
}

vk::ImageViewCreateInfo vkinit::imageview_create_info(vk::Format format,
                                                      vk::Image image,
                                                      vk::ImageAspectFlags aspectFlags)
{
    vk::ImageViewCreateInfo info{};
    info.viewType = vk::ImageViewType::e2D;
    info.image = image;
    info.format = format;
    info.subresourceRange.baseMipLevel = 0;
    info.subresourceRange.levelCount = 1;
    info.subresourceRange.baseArrayLayer = 0;
    info.subresourceRange.layerCount = 1;
    info.subresourceRange.aspectMask = aspectFlags;
    return info;
}

vk::PipelineLayoutCreateInfo vkinit::pipeline_layout_create_info()
{
    vk::PipelineLayoutCreateInfo info{};
    info.flags = {};
    info.setLayoutCount = 0;
    info.pSetLayouts = nullptr;
    info.pushConstantRangeCount = 0;
    info.pPushConstantRanges = nullptr;
    return info;
}

vk::PipelineShaderStageCreateInfo vkinit::pipeline_shader_stage_create_info(vk::ShaderStageFlagBits stage,
                                                                            vk::ShaderModule shaderModule,
                                                                            const char* entry)
{
    vk::PipelineShaderStageCreateInfo info{};
    info.stage = stage;
    info.module = shaderModule;
    info.pName = entry;
    return info;
}

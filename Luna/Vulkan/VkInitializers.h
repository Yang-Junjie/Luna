#pragma once

#include "VkTypes.h"

namespace vkinit {
vk::CommandPoolCreateInfo commandPoolCreateInfo(uint32_t queue_family_index,
                                                   vk::CommandPoolCreateFlags flags = {});
inline vk::CommandPoolCreateInfo commandPoolCreateInfo(uint32_t queue_family_index, VkCommandPoolCreateFlags flags)
{
    return commandPoolCreateInfo(queue_family_index, static_cast<vk::CommandPoolCreateFlags>(flags));
}

vk::CommandBufferAllocateInfo commandBufferAllocateInfo(vk::CommandPool pool, uint32_t count = 1);

vk::CommandBufferBeginInfo commandBufferBeginInfo(vk::CommandBufferUsageFlags flags = {});
inline vk::CommandBufferBeginInfo commandBufferBeginInfo(VkCommandBufferUsageFlags flags)
{
    return commandBufferBeginInfo(static_cast<vk::CommandBufferUsageFlags>(flags));
}

vk::CommandBufferSubmitInfo commandBufferSubmitInfo(vk::CommandBuffer cmd);

vk::FenceCreateInfo fenceCreateInfo(vk::FenceCreateFlags flags = {});
inline vk::FenceCreateInfo fenceCreateInfo(VkFenceCreateFlags flags)
{
    return fenceCreateInfo(static_cast<vk::FenceCreateFlags>(flags));
}

vk::SemaphoreCreateInfo semaphoreCreateInfo(vk::SemaphoreCreateFlags flags = {});
inline vk::SemaphoreCreateInfo semaphoreCreateInfo(VkSemaphoreCreateFlags flags)
{
    return semaphoreCreateInfo(static_cast<vk::SemaphoreCreateFlags>(flags));
}

vk::SubmitInfo2 submitInfo(const vk::CommandBufferSubmitInfo* cmd,
                            const vk::SemaphoreSubmitInfo* signal_semaphore_info,
                            const vk::SemaphoreSubmitInfo* wait_semaphore_info);

vk::PresentInfoKHR presentInfo();

vk::RenderingAttachmentInfo attachmentInfo(
    vk::ImageView view,
    const vk::ClearValue* clear,
    vk::ImageLayout layout = vk::ImageLayout::eColorAttachmentOptimal);
inline vk::RenderingAttachmentInfo attachmentInfo(vk::ImageView view,
                                                   const vk::ClearValue* clear,
                                                   VkImageLayout layout)
{
    return attachmentInfo(view, clear, static_cast<vk::ImageLayout>(layout));
}

vk::RenderingAttachmentInfo depthAttachmentInfo(
    vk::ImageView view,
    vk::ImageLayout layout = vk::ImageLayout::eDepthAttachmentOptimal);
inline vk::RenderingAttachmentInfo depthAttachmentInfo(vk::ImageView view, VkImageLayout layout)
{
    return depthAttachmentInfo(view, static_cast<vk::ImageLayout>(layout));
}

vk::RenderingInfo renderingInfo(vk::Extent2D render_extent,
                                 const vk::RenderingAttachmentInfo* color_attachment,
                                 const vk::RenderingAttachmentInfo* depth_attachment);

vk::ImageSubresourceRange imageSubresourceRange(vk::ImageAspectFlags aspect_mask);

vk::SemaphoreSubmitInfo semaphoreSubmitInfo(vk::PipelineStageFlags2 stage_mask, vk::Semaphore semaphore);
vk::DescriptorSetLayoutBinding
    descriptorsetLayoutBinding(vk::DescriptorType type, vk::ShaderStageFlags stage_flags, uint32_t binding);
vk::DescriptorSetLayoutCreateInfo descriptorsetLayoutCreateInfo(const vk::DescriptorSetLayoutBinding* bindings,
                                                                   uint32_t binding_count);
vk::WriteDescriptorSet writeDescriptorImage(vk::DescriptorType type,
                                              vk::DescriptorSet dst_set,
                                              const vk::DescriptorImageInfo* image_info,
                                              uint32_t binding);
vk::WriteDescriptorSet writeDescriptorBuffer(vk::DescriptorType type,
                                               vk::DescriptorSet dst_set,
                                               const vk::DescriptorBufferInfo* buffer_info,
                                               uint32_t binding);
vk::DescriptorBufferInfo bufferInfo(vk::Buffer buffer, vk::DeviceSize offset, vk::DeviceSize range);

vk::ImageCreateInfo imageCreateInfo(vk::Format format, vk::ImageUsageFlags usage_flags, vk::Extent3D extent);
vk::ImageViewCreateInfo imageviewCreateInfo(vk::Format format, vk::Image image, vk::ImageAspectFlags aspect_flags);
vk::PipelineLayoutCreateInfo pipelineLayoutCreateInfo();
vk::PipelineShaderStageCreateInfo pipelineShaderStageCreateInfo(vk::ShaderStageFlagBits stage,
                                                                    vk::ShaderModule shader_module,
                                                                    const char* entry = "main");
} // namespace vkinit


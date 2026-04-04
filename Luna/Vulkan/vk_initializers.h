#pragma once

#include "vk_types.h"

namespace vkinit {
vk::CommandPoolCreateInfo command_pool_create_info(uint32_t queueFamilyIndex,
                                                   vk::CommandPoolCreateFlags flags = {});
inline vk::CommandPoolCreateInfo command_pool_create_info(uint32_t queueFamilyIndex, VkCommandPoolCreateFlags flags)
{
    return command_pool_create_info(queueFamilyIndex, static_cast<vk::CommandPoolCreateFlags>(flags));
}

vk::CommandBufferAllocateInfo command_buffer_allocate_info(vk::CommandPool pool, uint32_t count = 1);

vk::CommandBufferBeginInfo command_buffer_begin_info(vk::CommandBufferUsageFlags flags = {});
inline vk::CommandBufferBeginInfo command_buffer_begin_info(VkCommandBufferUsageFlags flags)
{
    return command_buffer_begin_info(static_cast<vk::CommandBufferUsageFlags>(flags));
}

vk::CommandBufferSubmitInfo command_buffer_submit_info(vk::CommandBuffer cmd);

vk::FenceCreateInfo fence_create_info(vk::FenceCreateFlags flags = {});
inline vk::FenceCreateInfo fence_create_info(VkFenceCreateFlags flags)
{
    return fence_create_info(static_cast<vk::FenceCreateFlags>(flags));
}

vk::SemaphoreCreateInfo semaphore_create_info(vk::SemaphoreCreateFlags flags = {});
inline vk::SemaphoreCreateInfo semaphore_create_info(VkSemaphoreCreateFlags flags)
{
    return semaphore_create_info(static_cast<vk::SemaphoreCreateFlags>(flags));
}

vk::SubmitInfo2 submit_info(const vk::CommandBufferSubmitInfo* cmd,
                            const vk::SemaphoreSubmitInfo* signalSemaphoreInfo,
                            const vk::SemaphoreSubmitInfo* waitSemaphoreInfo);

vk::PresentInfoKHR present_info();

vk::RenderingAttachmentInfo attachment_info(
    vk::ImageView view,
    const vk::ClearValue* clear,
    vk::ImageLayout layout = vk::ImageLayout::eColorAttachmentOptimal);
inline vk::RenderingAttachmentInfo attachment_info(vk::ImageView view,
                                                   const vk::ClearValue* clear,
                                                   VkImageLayout layout)
{
    return attachment_info(view, clear, static_cast<vk::ImageLayout>(layout));
}

vk::RenderingAttachmentInfo depth_attachment_info(
    vk::ImageView view,
    vk::ImageLayout layout = vk::ImageLayout::eDepthAttachmentOptimal);
inline vk::RenderingAttachmentInfo depth_attachment_info(vk::ImageView view, VkImageLayout layout)
{
    return depth_attachment_info(view, static_cast<vk::ImageLayout>(layout));
}

vk::RenderingInfo rendering_info(vk::Extent2D renderExtent,
                                 const vk::RenderingAttachmentInfo* colorAttachment,
                                 const vk::RenderingAttachmentInfo* depthAttachment);
vk::RenderingInfo rendering_info(vk::Extent2D renderExtent,
                                 std::span<const vk::RenderingAttachmentInfo> colorAttachments,
                                 const vk::RenderingAttachmentInfo* depthAttachment);

vk::ImageSubresourceRange image_subresource_range(vk::ImageAspectFlags aspectMask);

vk::SemaphoreSubmitInfo semaphore_submit_info(vk::PipelineStageFlags2 stageMask, vk::Semaphore semaphore);
vk::DescriptorSetLayoutBinding
    descriptorset_layout_binding(vk::DescriptorType type, vk::ShaderStageFlags stageFlags, uint32_t binding);
vk::DescriptorSetLayoutCreateInfo descriptorset_layout_create_info(const vk::DescriptorSetLayoutBinding* bindings,
                                                                   uint32_t bindingCount);
vk::WriteDescriptorSet write_descriptor_image(vk::DescriptorType type,
                                              vk::DescriptorSet dstSet,
                                              const vk::DescriptorImageInfo* imageInfo,
                                              uint32_t binding);
vk::WriteDescriptorSet write_descriptor_buffer(vk::DescriptorType type,
                                               vk::DescriptorSet dstSet,
                                               const vk::DescriptorBufferInfo* bufferInfo,
                                               uint32_t binding);
vk::DescriptorBufferInfo buffer_info(vk::Buffer buffer, vk::DeviceSize offset, vk::DeviceSize range);

vk::ImageCreateInfo image_create_info(vk::Format format,
                                      vk::ImageUsageFlags usageFlags,
                                      vk::Extent3D extent,
                                      vk::ImageType imageType = vk::ImageType::e2D,
                                      uint32_t mipLevels = 1,
                                      uint32_t arrayLayers = 1);
vk::ImageViewCreateInfo imageview_create_info(vk::Format format,
                                              vk::Image image,
                                              vk::ImageAspectFlags aspectFlags,
                                              vk::ImageViewType viewType = vk::ImageViewType::e2D,
                                              uint32_t levelCount = 1,
                                              uint32_t layerCount = 1,
                                              uint32_t baseMipLevel = 0,
                                              uint32_t baseArrayLayer = 0);
vk::PipelineLayoutCreateInfo pipeline_layout_create_info();
vk::PipelineShaderStageCreateInfo pipeline_shader_stage_create_info(vk::ShaderStageFlagBits stage,
                                                                    vk::ShaderModule shaderModule,
                                                                    const char* entry = "main");
} // namespace vkinit

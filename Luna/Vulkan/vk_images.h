#pragma once
#include "vk_types.h"

namespace vkutil {
void transition_image(vk::CommandBuffer cmd,
                      vk::Image image,
                      vk::ImageLayout currentLayout,
                      vk::ImageLayout newLayout);
inline void transition_image(vk::CommandBuffer cmd, vk::Image image, VkImageLayout currentLayout, VkImageLayout newLayout)
{
    transition_image(
        cmd, image, static_cast<vk::ImageLayout>(currentLayout), static_cast<vk::ImageLayout>(newLayout));
}
void copy_image_to_image(vk::CommandBuffer cmd,
                         vk::Image source,
                         vk::Image destination,
                         vk::Extent2D srcSize,
                         vk::Extent2D dstSize);
inline void copy_image_to_image(
    vk::CommandBuffer cmd, vk::Image source, vk::Image destination, VkExtent2D srcSize, VkExtent2D dstSize)
{
    copy_image_to_image(cmd, source, destination, to_vk(srcSize), to_vk(dstSize));
}
}; // namespace vkutil

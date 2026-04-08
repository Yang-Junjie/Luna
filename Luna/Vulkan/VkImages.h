#pragma once
#include "VkTypes.h"

namespace vkutil {
void transitionImage(vk::CommandBuffer cmd,
                      vk::Image image,
                      vk::ImageLayout current_layout,
                      vk::ImageLayout new_layout);
inline void transitionImage(vk::CommandBuffer cmd, vk::Image image, VkImageLayout current_layout, VkImageLayout new_layout)
{
    transitionImage(
        cmd, image, static_cast<vk::ImageLayout>(current_layout), static_cast<vk::ImageLayout>(new_layout));
}
void copyImageToImage(vk::CommandBuffer cmd,
                         vk::Image source,
                         vk::Image destination,
                         vk::Extent2D src_size,
                         vk::Extent2D dst_size);
inline void copyImageToImage(
    vk::CommandBuffer cmd, vk::Image source, vk::Image destination, VkExtent2D src_size, VkExtent2D dst_size)
{
    copyImageToImage(cmd, source, destination, toVk(src_size), toVk(dst_size));
}
}; // namespace vkutil


#include "vk_images.h"
#include "vk_initializers.h"

void vkutil::transition_image(vk::CommandBuffer cmd,
                              vk::Image image,
                              vk::ImageLayout currentLayout,
                              vk::ImageLayout newLayout)
{
    vk::ImageMemoryBarrier2 imageBarrier{};
    imageBarrier.srcStageMask = vk::PipelineStageFlagBits2::eAllCommands;
    imageBarrier.srcAccessMask = vk::AccessFlagBits2::eMemoryWrite;
    imageBarrier.dstStageMask = vk::PipelineStageFlagBits2::eAllCommands;
    imageBarrier.dstAccessMask = vk::AccessFlagBits2::eMemoryWrite | vk::AccessFlagBits2::eMemoryRead;
    imageBarrier.oldLayout = currentLayout;
    imageBarrier.newLayout = newLayout;

    const vk::ImageAspectFlags aspectMask = (newLayout == vk::ImageLayout::eDepthAttachmentOptimal)
                                                ? vk::ImageAspectFlagBits::eDepth
                                                : vk::ImageAspectFlagBits::eColor;
    imageBarrier.subresourceRange = vkinit::image_subresource_range(aspectMask);
    imageBarrier.image = image;

    vk::DependencyInfo depInfo{};
    depInfo.imageMemoryBarrierCount = 1;
    depInfo.pImageMemoryBarriers = &imageBarrier;

    cmd.pipelineBarrier2(&depInfo);
}

void vkutil::copy_image_to_image(vk::CommandBuffer cmd,
                                 vk::Image source,
                                 vk::Image destination,
                                 vk::Extent2D srcSize,
                                 vk::Extent2D dstSize)
{
    vk::ImageBlit2 blitRegion{};
    blitRegion.srcOffsets[1] = vk::Offset3D{static_cast<int32_t>(srcSize.width), static_cast<int32_t>(srcSize.height), 1};
    blitRegion.dstOffsets[1] = vk::Offset3D{static_cast<int32_t>(dstSize.width), static_cast<int32_t>(dstSize.height), 1};
    blitRegion.srcSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
    blitRegion.srcSubresource.baseArrayLayer = 0;
    blitRegion.srcSubresource.layerCount = 1;
    blitRegion.srcSubresource.mipLevel = 0;
    blitRegion.dstSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
    blitRegion.dstSubresource.baseArrayLayer = 0;
    blitRegion.dstSubresource.layerCount = 1;
    blitRegion.dstSubresource.mipLevel = 0;

    vk::BlitImageInfo2 blitInfo{};
    blitInfo.dstImage = destination;
    blitInfo.dstImageLayout = vk::ImageLayout::eTransferDstOptimal;
    blitInfo.srcImage = source;
    blitInfo.srcImageLayout = vk::ImageLayout::eTransferSrcOptimal;
    blitInfo.filter = vk::Filter::eLinear;
    blitInfo.regionCount = 1;
    blitInfo.pRegions = &blitRegion;

    cmd.blitImage2(&blitInfo);
}

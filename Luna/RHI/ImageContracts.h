#pragma once

#include "PipelineDescriptors.h"

#include <string_view>

namespace luna {

constexpr ImageAspect default_image_aspect_for_format(PixelFormat format) noexcept
{
    return is_depth_format(format) ? ImageAspect::Depth : ImageAspect::Color;
}

constexpr bool is_cube_image_type(ImageType type) noexcept
{
    return type == ImageType::Cube;
}

constexpr bool is_cube_view_type(ImageViewType type) noexcept
{
    return type == ImageViewType::Cube;
}

constexpr ImageViewType default_view_type_for_image_desc(const ImageDesc& desc) noexcept
{
    switch (desc.type) {
        case ImageType::Image2DArray:
            return ImageViewType::Image2DArray;
        case ImageType::Image3D:
            return ImageViewType::Image3D;
        case ImageType::Cube:
            return ImageViewType::Cube;
        case ImageType::Image2D:
        default:
            return ImageViewType::Image2D;
    }
}

constexpr std::string_view cube_image_desc_contract_error(const ImageDesc& desc) noexcept
{
    if (desc.type != ImageType::Cube) {
        return "cube image type mismatch";
    }
    if (desc.width == 0 || desc.height == 0) {
        return "cube image extent must be non-zero";
    }
    if (desc.width != desc.height) {
        return "cube image width != height";
    }
    if (desc.depth != 1) {
        return "cube image depth != 1";
    }
    if (desc.arrayLayers != 6) {
        return "cube image arrayLayers != 6";
    }
    if (desc.mipLevels == 0) {
        return "cube image mipLevels == 0";
    }
    if (desc.format == PixelFormat::Undefined) {
        return "cube image format undefined";
    }
    return {};
}

constexpr bool is_cube_image_desc_legal(const ImageDesc& desc) noexcept
{
    return cube_image_desc_contract_error(desc).empty();
}

constexpr std::string_view image_desc_contract_error(const ImageDesc& desc) noexcept
{
    if (desc.width == 0 || desc.height == 0 || desc.depth == 0) {
        return "image extent must be non-zero";
    }
    if (desc.mipLevels == 0) {
        return "image mipLevels == 0";
    }
    if (desc.arrayLayers == 0) {
        return "image arrayLayers == 0";
    }
    if (desc.format == PixelFormat::Undefined) {
        return "image format undefined";
    }

    switch (desc.type) {
        case ImageType::Image2D:
            if (desc.depth != 1) {
                return "Image2D depth != 1";
            }
            if (desc.arrayLayers != 1) {
                return "Image2D arrayLayers != 1";
            }
            return {};
        case ImageType::Image2DArray:
            return desc.depth == 1 ? std::string_view{} : std::string_view{"Image2DArray depth != 1"};
        case ImageType::Image3D:
            return desc.arrayLayers == 1 ? std::string_view{} : std::string_view{"Image3D arrayLayers != 1"};
        case ImageType::Cube:
            return cube_image_desc_contract_error(desc);
        default:
            return "image type unknown";
    }
}

constexpr bool is_image_desc_legal(const ImageDesc& desc) noexcept
{
    return image_desc_contract_error(desc).empty();
}

constexpr std::string_view cube_view_desc_contract_error(const ImageDesc& imageDesc, const ImageViewDesc& desc) noexcept
{
    if (imageDesc.type != ImageType::Cube) {
        return "cube view image type mismatch";
    }
    if (!desc.image.isValid()) {
        return "cube view image handle invalid";
    }
    if (desc.mipCount == 0) {
        return "cube view mipCount == 0";
    }
    if (desc.baseMipLevel >= imageDesc.mipLevels || desc.baseMipLevel + desc.mipCount > imageDesc.mipLevels) {
        return "cube view mip range out of bounds";
    }
    if (desc.aspect != default_image_aspect_for_format(imageDesc.format)) {
        return "cube view aspect mismatch";
    }

    if (desc.type == ImageViewType::Cube) {
        if (desc.baseArrayLayer != 0) {
            return "cube view baseArrayLayer != 0";
        }
        if (desc.layerCount != 6) {
            return "cube view layer count != 6";
        }
        return {};
    }

    if (desc.type == ImageViewType::Image2D) {
        if (desc.baseArrayLayer >= imageDesc.arrayLayers) {
            return "cube face baseArrayLayer out of bounds";
        }
        if (desc.layerCount != 1) {
            return "face view layer count != 1";
        }
        return {};
    }

    return "cube image only supports Cube view or single-face 2D view";
}

constexpr bool is_cube_view_desc_legal(const ImageDesc& imageDesc, const ImageViewDesc& desc) noexcept
{
    return cube_view_desc_contract_error(imageDesc, desc).empty();
}

constexpr std::string_view image_view_desc_contract_error(const ImageDesc& imageDesc, const ImageViewDesc& desc) noexcept
{
    if (!desc.image.isValid()) {
        return "image view handle invalid";
    }
    if (desc.mipCount == 0) {
        return "image view mipCount == 0";
    }
    if (desc.layerCount == 0) {
        return "image view layerCount == 0";
    }
    if (desc.baseMipLevel >= imageDesc.mipLevels || desc.baseMipLevel + desc.mipCount > imageDesc.mipLevels) {
        return "image view mip range out of bounds";
    }
    if (desc.aspect != default_image_aspect_for_format(imageDesc.format)) {
        return "image view aspect mismatch";
    }

    switch (imageDesc.type) {
        case ImageType::Image2D:
            if (desc.type != ImageViewType::Image2D) {
                return "Image2D view type mismatch";
            }
            if (desc.baseArrayLayer != 0) {
                return "Image2D baseArrayLayer != 0";
            }
            return desc.layerCount == 1 ? std::string_view{} : std::string_view{"Image2D layerCount != 1"};
        case ImageType::Image2DArray:
            if (desc.baseArrayLayer >= imageDesc.arrayLayers ||
                desc.baseArrayLayer + desc.layerCount > imageDesc.arrayLayers) {
                return "Image2DArray layer range out of bounds";
            }
            if (desc.type == ImageViewType::Image2D) {
                return desc.layerCount == 1 ? std::string_view{} : std::string_view{"Image2DArray face layerCount != 1"};
            }
            return desc.type == ImageViewType::Image2DArray
                       ? std::string_view{}
                       : std::string_view{"Image2DArray view type mismatch"};
        case ImageType::Image3D:
            if (desc.type != ImageViewType::Image3D) {
                return "Image3D view type mismatch";
            }
            if (desc.baseArrayLayer != 0) {
                return "Image3D baseArrayLayer != 0";
            }
            return desc.layerCount == 1 ? std::string_view{} : std::string_view{"Image3D layerCount != 1"};
        case ImageType::Cube:
            return cube_view_desc_contract_error(imageDesc, desc);
        default:
            return "image view image type unknown";
    }
}

constexpr bool is_image_view_desc_legal(const ImageDesc& imageDesc, const ImageViewDesc& desc) noexcept
{
    return image_view_desc_contract_error(imageDesc, desc).empty();
}

} // namespace luna

